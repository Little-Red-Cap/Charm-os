#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>

import charm.foundation;
import charm.runtime;
import charm.system.bringup;
import charm.system.bringup.win_stub;
import charm.system.app_host;
import charm.system.init_block;
import out.api;
import platform.board.win_stub;
import platform.win.irq_guard;
import platform.win.time_source;
import platform.win.wakeup;
import usb.class_msc;
import usb.class_msc_block;
import usb.device;
import usb.device_driver;
import usb.dsl;
import util.expected;

namespace {
    struct StdoutSink {
        out::result<std::size_t> write(out::bytes b) noexcept {
            if (std::fwrite(b.data(), 1, b.size(), stdout) != b.size()) {
                return util::unexpected(out::errc::io_error);
            }
            return out::ok(b.size());
        }
        out::result<std::size_t> flush() noexcept {
            return std::fflush(stdout) == 0 ? out::ok<std::size_t>(0u)
                                            : util::unexpected(out::errc::io_error);
        }
    };

    constexpr usb::u16 kLangs[] = { 0x0409 };
    constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
    constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
    constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm MSC");
    constexpr auto kSerialStr = usb::make_ascii_string_descriptor("0001");

    static const usb::StringTable<4> kStrings{
        std::array<std::span<const usb::u8>, 4>{
            std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
            std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
            std::span<const usb::u8>(kProductStr.data(), kProductStr.size()),
            std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
        }
    };

    struct CbwBuilder {
        usb::class_driver::MscCbw cbw{};

        CbwBuilder(usb::u32 tag,
                   usb::u32 data_len,
                   bool dir_in,
                   usb::u8 lun,
                   usb::u8 cb_len) noexcept {
            cbw.signature = 0x43425355;
            cbw.tag = tag;
            cbw.data_transfer_length = data_len;
            cbw.flags = dir_in ? 0x80 : 0x00;
            cbw.lun = lun;
            cbw.cb_length = cb_len;
        }

        static void write_be32(usb::u8* dst, usb::u32 v) noexcept {
            dst[0] = static_cast<usb::u8>((v >> 24) & 0xFF);
            dst[1] = static_cast<usb::u8>((v >> 16) & 0xFF);
            dst[2] = static_cast<usb::u8>((v >> 8) & 0xFF);
            dst[3] = static_cast<usb::u8>(v & 0xFF);
        }

        static void write_be16(usb::u8* dst, usb::u16 v) noexcept {
            dst[0] = static_cast<usb::u8>((v >> 8) & 0xFF);
            dst[1] = static_cast<usb::u8>(v & 0xFF);
        }
    };

    bool run_scsi_in(StdoutSink& sink,
                     usb::class_driver::MscBot& bot,
                     const usb::class_driver::MscConfig& cfg,
                     usb::class_driver::ScsiCmd cmd,
                     usb::u32 data_len,
                     usb::u32 tag,
                     usb::u32 lba = 0,
                     usb::u16 blocks = 0) {
        CbwBuilder builder{tag, data_len, true, 0, 10};
        std::memset(builder.cbw.cb, 0, sizeof(builder.cbw.cb));
        builder.cbw.cb[0] = static_cast<usb::u8>(cmd);
        switch (cmd) {
        case usb::class_driver::ScsiCmd::inquiry:
            builder.cbw.cb_length = 6;
            builder.cbw.cb[4] = static_cast<usb::u8>(data_len);
            break;
        case usb::class_driver::ScsiCmd::request_sense:
            builder.cbw.cb_length = 6;
            builder.cbw.cb[4] = static_cast<usb::u8>(data_len);
            break;
        case usb::class_driver::ScsiCmd::read_capacity_10:
            builder.cbw.cb_length = 10;
            break;
        case usb::class_driver::ScsiCmd::read_10:
            builder.cbw.cb_length = 10;
            CbwBuilder::write_be32(&builder.cbw.cb[2], lba);
            CbwBuilder::write_be16(&builder.cbw.cb[7], blocks);
            break;
        default:
            break;
        }

        const auto* raw = reinterpret_cast<const usb::u8*>(&builder.cbw);
        if (!bot.on_out_packet(std::span<const usb::u8>(raw, sizeof(builder.cbw)))) {
            (void)out::println<"[ERR] MSC BOT rejected CBW">(sink);
            return false;
        }

        constexpr std::size_t kMaxResp = 1024;
        if (data_len + 13 > kMaxResp) {
            (void)out::println<"[ERR] response too large: {}">(sink, data_len + 13);
            return false;
        }

        std::array<usb::u8, kMaxResp> resp{};
        std::size_t resp_len = 0;
        while (resp_len < data_len + 13) {
            auto chunk = bot.on_in_request(cfg.ep_mps);
            if (chunk.empty()) break;
            const auto n = chunk.size();
            std::memcpy(resp.data() + resp_len, chunk.data(), n);
            resp_len += n;
        }

        (void)out::println<"[msc] cmd={} data={} resp={}">(sink,
            static_cast<int>(cmd), data_len, resp_len);

        if (resp_len < 13) {
            (void)out::println<"[ERR] MSC CSW missing">(sink);
            return false;
        }

        if (cmd == usb::class_driver::ScsiCmd::read_10 && data_len >= 16) {
            (void)out::println<"[msc] read10 first16: {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}">(sink,
                resp[0], resp[1], resp[2], resp[3],
                resp[4], resp[5], resp[6], resp[7],
                resp[8], resp[9], resp[10], resp[11],
                resp[12], resp[13], resp[14], resp[15]);
        }

        return true;
    }
}

int main(int argc, char** argv) {
    StdoutSink sink{};
    if (argc < 2) {
        (void)out::println<"usage: usb-msc-block-demo <disk.img|vhd>">(sink);
        return 1;
    }
    if (std::FILE* f = std::fopen(argv[1], "rb"); !f) {
        const int err = errno;
        if (err == ENOENT) {
            (void)out::println<"[ERR] image not found: {}">(sink, argv[1]);
        } else {
            (void)out::println<"[ERR] open image failed: {} err={}">(sink, argv[1], err);
        }
        return 1;
    } else {
        std::fclose(f);
    }

    auto caps = platform::board::win_stub::make_board_caps();
    charm::system::PumpCaps pump_caps{};
    charm::system::AppHost<charm::system::PumpCaps> host{pump_caps};
    charm::system::BringupMinimal<8, 16, 8, 64, 64> bringup{caps, host};

    charm::system::FileInitChain<block::Registry<8>> file_chain{
        bringup.block_registry(),
        argv[1],
        512,
        "block.sd0"
    };

    auto r = bringup.start(
        static_cast<util::u32>(init::Runlevel::all),
        init::Phase::app,
        file_chain.node_span());
    if (!r) {
        (void)out::println<"[ERR] bringup failed err={}">(sink, static_cast<int>(r.error()));
        return 1;
    }

    auto* dev = bringup.block_registry().open_device("block.sd0");
    if (!dev) {
        (void)out::println<"[ERR] block capability not found: block.sd0">(sink);
        return 1;
    }

    usb::class_driver::MscBlockConfig cfg{};
    cfg.read_only = true;
    auto storage = usb::class_driver::make_storage_from_block_device(*dev, cfg);

    std::array<usb::class_driver::MscStorage, 1> luns{ storage };
    std::array<usb::u8, 4096> io_buf{};
    if (dev->block_size > io_buf.size()) {
        (void)out::println<"[ERR] io buffer too small: {}">(sink, dev->block_size);
        return 1;
    }

    usb::class_driver::MscBot bot{
        std::span<usb::class_driver::MscStorage>(luns.data(), luns.size()),
        std::span<usb::u8>(io_buf.data(), io_buf.size())
    };
    auto msc_ops = usb::class_driver::make_msc_ops(bot);
    usb::class_driver::MscDevice msc{ &bot, msc_ops };

    std::array<usb::u8, 64> dev_desc{};
    std::array<usb::u8, 256> cfg_desc{};
    usb::device::DescriptorTable table{};
    usb::device::ConfigTree tree{};
    usb::dsl::DeviceBuildContext build_ctx{
        std::span<usb::u8>(dev_desc.data(), dev_desc.size()),
        std::span<usb::u8>(cfg_desc.data(), cfg_desc.size()),
        &table,
        &tree
    };

    usb::dsl::DeviceInfo dev_info{};
    dev_info.vendor_id = 0x1209;
    dev_info.product_id = 0x0002;
    dev_info.i_manufacturer = 1;
    dev_info.i_product = 2;
    dev_info.i_serial = 3;

    usb::dsl::ConfigInfo cfg_info{};
    usb::class_driver::MscConfig msc_cfg{};
    usb::dsl::MscClassDescriptors msc_desc{};

    usb::device::Device device{};
    const auto ok = usb::device::examples::build_and_attach_msc(
        device, msc, build_ctx,
        dev_info, cfg_info, msc_cfg,
        msc_desc.view(), kStrings.entries.data(), kStrings.entries.size());
    if (!ok) {
        (void)out::println<"[ERR] build MSC descriptors failed">(sink);
        return 1;
    }

    (void)out::println<"[OK] msc descriptors built; block_size={} blocks={}">(sink,
        dev->block_size, dev->block_count);

    if (!run_scsi_in(sink, bot, msc_cfg,
            usb::class_driver::ScsiCmd::inquiry, 36, 1)) {
        return 1;
    }
    if (!run_scsi_in(sink, bot, msc_cfg,
            usb::class_driver::ScsiCmd::read_capacity_10, 8, 2)) {
        return 1;
    }
    if (!run_scsi_in(sink, bot, msc_cfg,
            usb::class_driver::ScsiCmd::read_10,
            static_cast<usb::u32>(dev->block_size), 3, 0, 1)) {
        return 1;
    }

    return 0;
}
