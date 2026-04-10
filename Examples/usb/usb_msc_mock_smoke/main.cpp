#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <vector>

import block.device;
import block.registry;
import usb.class_msc;
import usb.common;
import usb.device_driver;
import usb.mock;
import usb.model;
import usb.plan;
import usb.runtime;
import usb.spec;
import util.core;
import util.error;

namespace {
    constexpr usb::u16 kLangs[] = { 0x0409 };
    constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
    constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
    constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm MSC Mock");
    constexpr auto kSerialStr = usb::make_ascii_string_descriptor("0001");

    static const usb::StringTable<4> kStrings{
        std::array<std::span<const usb::u8>, 4>{
            std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
            std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
            std::span<const usb::u8>(kProductStr.data(), kProductStr.size()),
            std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
        }
    };

    struct MemoryDisk {
        static constexpr std::size_t block_size = 512;
        static constexpr std::size_t block_count = 16;
        std::array<usb::u8, block_size * block_count> bytes{};
        block::Device device{};

        MemoryDisk() noexcept {
            bytes[0] = 0xEB;
            bytes[1] = 0x3C;
            bytes[2] = 0x90;
            std::memcpy(bytes.data() + 3, "MSDOS5.0", 8);
            bytes[510] = 0x55;
            bytes[511] = 0xAA;

            device.ctx = this;
            device.read = &MemoryDisk::read_cb;
            device.write = nullptr;
            device.erase = nullptr;
            device.flush = nullptr;
            device.block_size = block_size;
            device.block_count = block_count;
            device.caps = block::to_bits(block::Caps::read);
        }

        static block::Status read_cb(void* ctx, util::u64 lba, std::span<util::u8> out) noexcept {
            auto* self = static_cast<MemoryDisk*>(ctx);
            if (!self || out.empty() || (out.size() % block_size) != 0) {
                return {util::Errc::invalid_arg};
            }
            const auto blocks = static_cast<util::u64>(out.size() / block_size);
            if (lba + blocks > block_count) {
                return {util::Errc::invalid_arg};
            }
            const auto offset = static_cast<std::size_t>(lba * block_size);
            std::memcpy(out.data(), self->bytes.data() + offset, out.size());
            return {};
        }
    };

    struct DemoContext {
        bool ready{false};
        usb::class_driver::MscBot* bot{nullptr};
        usb::class_driver::MscConfig cfg{};
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

    bool expect(bool cond, const char* message) {
        if (!cond) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    void on_ready(void* ctx,
                  usb::class_driver::MscBot* bot,
                  const usb::class_driver::MscConfig* cfg) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (!demo || !bot || !cfg) return;
        demo->ready = true;
        demo->bot = bot;
        demo->cfg = *cfg;
    }

    std::vector<usb::u8> control_in(usb::mock::Session& session,
                                    const usb::SetupPacket& setup) {
        std::vector<usb::u8> out{};
        session.feed_setup(setup);
        while (auto pkt = session.poll_in()) {
            out.insert(out.end(), pkt->data.begin(), pkt->data.end());
            if (!session.ack_in(pkt->ep, pkt->data.size(), pkt->zlp)) {
                return {};
            }
        }
        return out;
    }

    bool control_out(usb::mock::Session& session,
                     const usb::SetupPacket& setup,
                     std::span<const usb::u8> data = {}) {
        session.feed_setup(setup);
        if (!data.empty()) {
            if (!session.feed_out(0x00, data)) {
                return false;
            }
        }
        auto pkt = session.poll_in();
        if (!pkt || pkt->ep != 0x80 || !pkt->zlp) {
            return false;
        }
        return session.ack_in(pkt->ep, pkt->data.size(), pkt->zlp);
    }

    bool run_scsi_in(usb::mock::Session& session,
                     usb::class_driver::MscBot& bot,
                     const usb::class_driver::MscConfig& cfg,
                     usb::class_driver::ScsiCmd cmd,
                     usb::u32 data_len,
                     usb::u32 tag,
                     std::vector<usb::u8>& out,
                     usb::u32 lba = 0,
                     usb::u16 blocks = 0) {
        CbwBuilder builder{tag, data_len, true, 0, 10};
        std::memset(builder.cbw.cb, 0, sizeof(builder.cbw.cb));
        builder.cbw.cb[0] = static_cast<usb::u8>(cmd);
        switch (cmd) {
        case usb::class_driver::ScsiCmd::inquiry:
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

        if (!session.feed_out(cfg.ep_out,
                              std::span<const usb::u8>(reinterpret_cast<const usb::u8*>(&builder.cbw),
                                                       sizeof(builder.cbw)))) {
            return false;
        }

        out.clear();
        while (bot.has_in_data()) {
            if (!usb::device::examples::send_msc_in_packet(session.dcd_ops(), &session, bot, cfg)) {
                return false;
            }
            auto pkt = session.poll_in();
            if (!pkt) {
                return false;
            }
            out.insert(out.end(), pkt->data.begin(), pkt->data.end());
            if (!session.ack_in(pkt->ep, pkt->data.size(), pkt->zlp)) {
                return false;
            }
        }

        return out.size() >= data_len + sizeof(usb::class_driver::MscCsw);
    }
}

int main() {
    MemoryDisk disk{};
    block::Registry<2> registry{};
    registry.init();
    auto reg = registry.register_device({"block.sd0", block::cap_id("block.sd0")}, disk.device);
    if (!reg) {
        std::fprintf(stderr, "[ERR] registry register failed err=%d\n", static_cast<int>(reg.error()));
        return 1;
    }

    DemoContext demo{};
    usb::mock::Session session{};

    usb::spec::DeviceSpec device{};
    device.vendor_id = 0x1209;
    device.product_id = 0x0002;
    device.i_manufacturer = 1;
    device.i_product = 2;
    device.i_serial = 3;
    device.strings = std::span<const std::span<const usb::u8>>(kStrings.entries.data(), kStrings.entries.size());

    usb::spec::MscFunctionSpec msc{};
    msc.cap_name = "usb.msc0";
    msc.block_cap = "block.sd0";
    msc.vendor = "Charm";
    msc.product = "MockDisk";
    msc.revision = "1.00";
    msc.read_only = true;

    const auto spec = usb::spec::msc_device(device, msc);
    const auto model = usb::build(spec);
    const auto plan = usb::plan::build(model);
    if (!plan) {
        std::fprintf(stderr, "[ERR] plan build failed err=%d\n", static_cast<int>(plan.error()));
        return 1;
    }

    const auto runtime = usb::runtime::host_mock(
        session.dcd_ops(),
        &session,
        &session.adapter(),
        usb::runtime::MscReadyHook{&on_ready, &demo});

    auto binding = usb::runtime::make(plan.value(), registry, runtime);
    const auto init = decltype(binding)::init_trampoline(&binding);
    if (!init) {
        std::fprintf(stderr, "[ERR] binding init failed err=%d\n", static_cast<int>(init.error()));
        return 1;
    }

    if (!expect(demo.ready, "msc ready hook not called")) return 1;
    if (!expect(session.pullup_enabled(), "device pull-up not enabled")) return 1;

    session.signal_reset();

    const auto dev_desc = control_in(session, usb::SetupPacket{0x80, 0x06, 0x0100, 0x0000, 0x0040});
    if (!expect(dev_desc.size() == 18, "device descriptor length mismatch")) return 1;

    if (!control_out(session, usb::SetupPacket{0x00, 0x05, 0x0007, 0x0000, 0x0000})) {
        std::fprintf(stderr, "[ERR] set address failed\n");
        return 1;
    }
    if (!expect(session.address() == 7, "device address not applied")) return 1;

    const auto cfg_full = control_in(session, usb::SetupPacket{0x80, 0x06, 0x0200, 0x0000, 0x00FF});
    if (!expect(cfg_full.size() == 32, "full config descriptor length mismatch")) return 1;

    if (!control_out(session, usb::SetupPacket{0x00, 0x09, 0x0001, 0x0000, 0x0000})) {
        std::fprintf(stderr, "[ERR] set configuration failed\n");
        return 1;
    }
    if (!expect(session.configured(), "device not configured")) return 1;

    const auto max_lun = control_in(session, usb::SetupPacket{0xA1, 0xFE, 0x0000, 0x0000, 0x0001});
    if (!expect(max_lun.size() == 1 && max_lun[0] == 0x00, "max lun response mismatch")) return 1;
    if (!expect(session.endpoint_state(0x01).opened, "msc bulk out endpoint not opened")) return 1;
    if (!expect(session.endpoint_state(0x81).opened, "msc bulk in endpoint not opened")) return 1;

    std::vector<usb::u8> resp{};
    if (!run_scsi_in(session, *demo.bot, demo.cfg, usb::class_driver::ScsiCmd::inquiry, 36, 1, resp)) {
        std::fprintf(stderr, "[ERR] inquiry transaction failed\n");
        return 1;
    }
    if (!expect(resp.size() >= 49, "inquiry response too short")) return 1;
    if (!expect(resp[8] == 'C' && resp[16] == 'M', "inquiry payload mismatch")) return 1;

    if (!run_scsi_in(session, *demo.bot, demo.cfg, usb::class_driver::ScsiCmd::read_capacity_10, 8, 2, resp)) {
        std::fprintf(stderr, "[ERR] read capacity transaction failed\n");
        return 1;
    }
    if (!expect(resp.size() >= 21, "read capacity response too short")) return 1;
    const usb::u32 last_lba = (static_cast<usb::u32>(resp[0]) << 24)
                            | (static_cast<usb::u32>(resp[1]) << 16)
                            | (static_cast<usb::u32>(resp[2]) << 8)
                            | static_cast<usb::u32>(resp[3]);
    const usb::u32 block_size = (static_cast<usb::u32>(resp[4]) << 24)
                              | (static_cast<usb::u32>(resp[5]) << 16)
                              | (static_cast<usb::u32>(resp[6]) << 8)
                              | static_cast<usb::u32>(resp[7]);
    if (!expect(last_lba == MemoryDisk::block_count - 1, "last lba mismatch")) return 1;
    if (!expect(block_size == MemoryDisk::block_size, "block size mismatch")) return 1;

    if (!run_scsi_in(session, *demo.bot, demo.cfg, usb::class_driver::ScsiCmd::read_10, 512, 3, resp, 0, 1)) {
        std::fprintf(stderr, "[ERR] read10 transaction failed\n");
        return 1;
    }
    if (!expect(resp.size() >= 525, "read10 response too short")) return 1;
    if (!expect(resp[0] == 0xEB && resp[1] == 0x3C && resp[2] == 0x90, "read10 boot sector mismatch")) return 1;
    if (!expect(resp[510] == 0x55 && resp[511] == 0xAA, "read10 tail signature mismatch")) return 1;

    std::printf("[OK] usb-msc-mock-smoke passed\n");
    std::printf("[state] address=%u configured=%d resets=%zu cfg_calls=%zu\n",
                static_cast<unsigned>(session.address()),
                session.configured() ? 1 : 0,
                session.reset_count(),
                session.set_configured_count());
    return 0;
}
