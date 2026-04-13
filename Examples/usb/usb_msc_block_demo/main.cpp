#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

import block.registry;
import charm.system.bringup.block;
import charm.system.init_block;
import charm.system.init_usb;
import init.materialize;
import init.node;
import init.observe;
import init.plan;
import kernel.eda;
import kernel.evt;
import out.api;
import charm.system.reactor_pump;
import platform.board.win_stub;
import usb.class_msc;
import usb.class_msc_block;
import usb.class_msc_block.node;
import usb.common;
import usb.driver;
import usb.dsl;
import util.core;
import util.expected;

namespace {
    constexpr const char* kDefaultExportImage = "observe-usb-block.img";

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

    struct FakeDcd {
        usb::driver::DcdOps ops{};
        usb::driver::DcdDeviceAdapter adapter{};

        FakeDcd() noexcept {
            ops.ep.open = &FakeDcd::ep_open;
            ops.ep.close = &FakeDcd::ep_close;
            ops.ep.send = &FakeDcd::ep_send;
            ops.ep.stall = &FakeDcd::ep_stall;
            ops.set_address = &FakeDcd::set_address;
            ops.set_configured = &FakeDcd::set_configured;
            ops.connect = &FakeDcd::connect;
        }

        static bool ep_open(void*, const usb::driver::EpConfig&, usb::driver::EpCallbacks) noexcept {
            return true;
        }
        static bool ep_close(void*, usb::u8) noexcept { return true; }
        static bool ep_send(void*, usb::u8, std::span<const usb::u8>, bool) noexcept { return true; }
        static bool ep_stall(void*, usb::u8) noexcept { return true; }
        static bool set_address(void*, usb::u8) noexcept { return true; }
        static bool set_configured(void*, bool) noexcept { return true; }
        static bool connect(void*, bool) noexcept { return true; }
    };

    struct DemoContext {
        StdoutSink* sink{nullptr};
    };

    struct MiniHost {
        charm::system::ReactorPumpTask pump_task{};

        charm::system::ReactorPumpTask& pump() noexcept { return pump_task; }

        static bool post(void*, kernel::TaskId, kernel::Event) noexcept {
            return true;
        }

        charm::system::PostFn post_fn() noexcept { return &MiniHost::post; }
        charm::system::PostFn post_io_ready_fn() noexcept { return &MiniHost::post; }
        charm::system::PostFn post_demand_fn() noexcept { return &MiniHost::post; }
        void* post_ctx() noexcept { return nullptr; }
        kernel::TaskId pump_id() noexcept { return kernel::TaskId{0}; }
    };

    void on_msc_ready(void* ctx,
                      usb::class_driver::MscBot* bot,
                      const usb::class_driver::MscConfig* cfg) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (!demo || !demo->sink || !bot || !cfg) return;

        (void)run_scsi_in(*demo->sink, *bot, *cfg,
            usb::class_driver::ScsiCmd::inquiry, 36, 1);
        (void)run_scsi_in(*demo->sink, *bot, *cfg,
            usb::class_driver::ScsiCmd::read_capacity_10, 8, 2);
    }

    bool write_text_file(const char* path, const char* data, std::size_t bytes) noexcept {
        if (!path || !data) {
            return false;
        }
        std::FILE* file = std::fopen(path, "wb");
        if (!file) {
            return false;
        }
        const auto written = std::fwrite(data, 1, bytes, file);
        std::fclose(file);
        return written == bytes;
    }
}

int main(int argc, char** argv) {
    StdoutSink sink{};
    const char* image_path = nullptr;
    const char* dot_path = nullptr;
    const char* json_path = nullptr;
    bool export_only = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--dot") == 0) {
            if (i + 1 >= argc) {
                (void)out::println<"usage: usb-msc-block-demo [--export-only] [--dot PATH] [--json PATH] [--image PATH] <disk.img|vhd>">(sink);
                return 1;
            }
            dot_path = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--json") == 0) {
            if (i + 1 >= argc) {
                (void)out::println<"usage: usb-msc-block-demo [--export-only] [--dot PATH] [--json PATH] [--image PATH] <disk.img|vhd>">(sink);
                return 1;
            }
            json_path = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--image") == 0) {
            if (i + 1 >= argc) {
                (void)out::println<"usage: usb-msc-block-demo [--export-only] [--dot PATH] [--json PATH] [--image PATH] <disk.img|vhd>">(sink);
                return 1;
            }
            image_path = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--export-only") == 0) {
            export_only = true;
            continue;
        }
        if (!image_path) {
            image_path = argv[i];
            continue;
        }
        (void)out::println<"[ERR] unexpected arg: {}">(sink, argv[i]);
        return 1;
    }

    if (!image_path && export_only) {
        image_path = kDefaultExportImage;
    }
    if (!image_path) {
        (void)out::println<"usage: usb-msc-block-demo [--export-only] [--dot PATH] [--json PATH] [--image PATH] <disk.img|vhd>">(sink);
        return 1;
    }
    if (!export_only) {
        if (std::FILE* f = std::fopen(image_path, "rb"); !f) {
            const int err = errno;
            if (err == ENOENT) {
                (void)out::println<"[ERR] image not found: {}">(sink, image_path);
            } else {
                (void)out::println<"[ERR] open image failed: {} err={}">(sink, image_path, err);
            }
            return 1;
        } else {
            std::fclose(f);
        }
    }

    auto caps = platform::board::win_stub::make_block_caps();
    MiniHost host{};
    charm::system::BringupBlock<8, 16, 8> bringup{caps, host};

    charm::system::FileInitChain<block::Registry<8>> file_chain{
        bringup.block_registry(),
        image_path,
        512,
        "block.sd0"
    };

    FakeDcd dcd{};
    DemoContext demo_ctx{&sink};

    usb::device::MscBlockDesc msc_desc{};
    msc_desc.block_cap = "block.sd0";
    msc_desc.dcd = dcd.ops;
    msc_desc.dcd_ctx = &dcd;
    msc_desc.adapter = &dcd.adapter;
    msc_desc.dev_info.vendor_id = 0x1209;
    msc_desc.dev_info.product_id = 0x0002;
    msc_desc.dev_info.i_manufacturer = 1;
    msc_desc.dev_info.i_product = 2;
    msc_desc.dev_info.i_serial = 3;
    msc_desc.storage_cfg.read_only = true;
    msc_desc.strings = std::span<const std::span<const usb::u8>>(
        kStrings.entries.data(), kStrings.entries.size());
    msc_desc.on_ready = &on_msc_ready;
    msc_desc.on_ready_ctx = &demo_ctx;

    charm::system::UsbMscBlockInitChain<block::Registry<8>> usb_chain{
        bringup.block_registry(),
        msc_desc,
        init::Phase::app
    };

    const auto bringup_plan = init::compose(
        file_chain.plan(),
        usb_chain.plan());

    if (dot_path || json_path) {
        auto mats = init::materialize<24, 48>(bringup.plan(
            bringup_plan,
            static_cast<util::u32>(init::Runlevel::all),
            init::Phase::app));
        if (!mats) {
            (void)out::println<"[ERR] export materialize failed err={}">(sink, static_cast<int>(mats.error()));
            return 1;
        }
        if (dot_path) {
            std::array<char, 16384> dot{};
            const auto dot_bytes = init::format_dot(*mats, dot.data(), dot.size());
            if (dot_bytes == 0 || !write_text_file(dot_path, dot.data(), dot_bytes)) {
                (void)out::println<"[ERR] write dot failed: {}">(sink, dot_path);
                return 1;
            }
            (void)out::println<"[OK] dot exported: {} bytes={}">(sink, dot_path, dot_bytes);
        }
        if (json_path) {
            std::array<char, 16384> json{};
            const auto json_bytes = init::format_json_sample(*mats, json.data(), json.size());
            if (json_bytes == 0 || !write_text_file(json_path, json.data(), json_bytes)) {
                (void)out::println<"[ERR] write json failed: {}">(sink, json_path);
                return 1;
            }
            (void)out::println<"[OK] json exported: {} bytes={}">(sink, json_path, json_bytes);
        }
    }

    if (export_only) {
        return 0;
    }

    (void)out::println<"[OK] bringup starting">(sink);
    auto r = bringup.start_plan(
        bringup_plan,
        static_cast<util::u32>(init::Runlevel::all),
        init::Phase::app);
    if (!r) {
        (void)out::println<"[ERR] bringup failed err={}">(sink, static_cast<int>(r.error()));
        return 1;
    }
    (void)out::println<"[OK] bringup done">(sink);

    auto* dev = bringup.block_registry().open_device("block.sd0");
    if (!dev) {
        (void)out::println<"[ERR] block capability not found: block.sd0">(sink);
        return 1;
    }
    (void)out::println<"[OK] block open">(sink);

    (void)out::println<"[OK] msc binding ready; block_size={} blocks={}">(sink,
        dev->block_size, dev->block_count);
    (void)out::println<"[WARN] DCD is stub; enumeration needs real device">(sink);

    return 0;
}
