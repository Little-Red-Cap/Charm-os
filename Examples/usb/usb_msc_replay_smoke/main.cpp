#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

import block.device;
import block.registry;
import usb.class_msc;
import usb.common;
import usb.device_driver;
import usb.fixture;
import usb.mock;
import usb.model;
import usb.plan;
import usb.replay;
import usb.runtime;
import usb.spec;
import util.core;
import util.error;

namespace {
    constexpr usb::u16 kLangs[] = { 0x0409 };
    constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
    constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
    constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm MSC Replay");
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

    void on_ready(void* ctx,
                  usb::class_driver::MscBot* bot,
                  const usb::class_driver::MscConfig* cfg) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (!demo || !bot || !cfg) return;
        demo->ready = true;
        demo->bot = bot;
        demo->cfg = *cfg;
    }

    struct PumpContext {
        usb::class_driver::MscBot* bot{nullptr};
        usb::class_driver::MscConfig cfg{};
    };

    bool pump_in(void* ctx, usb::mock::Session& session, usb::u8 ep) noexcept {
        auto* pump = static_cast<PumpContext*>(ctx);
        if (!pump || !pump->bot) return false;
        if (ep != pump->cfg.ep_in) return false;
        return usb::device::examples::send_msc_in_packet(session.dcd_ops(), &session, *pump->bot, pump->cfg);
    }

    bool has_host_event(std::span<const usb::mock::HostEvent> events,
                        usb::mock::HostEventKind kind) {
        for (const auto& event : events) {
            if (event.kind == kind) {
                return true;
            }
        }
        return false;
    }

    bool has_device_action(std::span<const usb::mock::DeviceAction> actions,
                           usb::mock::DeviceActionKind kind) {
        for (const auto& action : actions) {
            if (action.kind == kind) {
                return true;
            }
        }
        return false;
    }

    bool has_trace_event(std::span<const usb::class_driver::MscTraceEvent> events,
                         usb::class_driver::MscTraceEventKind kind,
                         usb::u8 command = 0xFF) {
        for (const auto& event : events) {
            if (event.kind != kind) {
                continue;
            }
            if (command == 0xFF || event.command == command) {
                return true;
            }
        }
        return false;
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
    device.product_id = 0x0006;
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

    if (!usb::fixture::expect(demo.ready, "msc ready hook not called")) return 1;

    PumpContext pump{demo.bot, demo.cfg};
    session.clear_trace();
    demo.bot->clear_trace();
    if (!usb::fixture::run_replay_file(
            session,
            USB_REPLAY_FIXTURE_PATH,
            usb::replay::Hooks{&pump_in, &pump})) {
        return 1;
    }

    const auto msc_cfg = plan.value().msc.msc_cfg;
    if (!usb::fixture::expect(session.address() == 7, "device address not applied")) return 1;
    if (!usb::fixture::expect(session.configured(), "device not configured")) return 1;
    if (!usb::fixture::expect(session.endpoint_state(msc_cfg.ep_out).opened, "msc bulk out endpoint not opened")) return 1;
    if (!usb::fixture::expect(session.endpoint_state(msc_cfg.ep_in).opened, "msc bulk in endpoint not opened")) return 1;
    if (!usb::fixture::expect(has_host_event(session.host_events(), usb::mock::HostEventKind::setup), "replay setup event missing")) return 1;
    if (!usb::fixture::expect(has_host_event(session.host_events(), usb::mock::HostEventKind::out_packet), "replay out event missing")) return 1;
    if (!usb::fixture::expect(has_device_action(session.device_actions(), usb::mock::DeviceActionKind::send_in), "replay send-in action missing")) return 1;
    if (!usb::fixture::expect(has_trace_event(demo.bot->trace_events(), usb::class_driver::MscTraceEventKind::read_capacity,
                                              static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_capacity_10)),
                              "replay read-capacity trace missing")) return 1;
    if (!usb::fixture::expect(has_trace_event(demo.bot->trace_events(), usb::class_driver::MscTraceEventKind::read10_started,
                                              static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_10)),
                              "replay read10 trace missing")) return 1;
    if (!usb::fixture::expect(has_trace_event(demo.bot->trace_events(), usb::class_driver::MscTraceEventKind::csw_sent),
                              "replay csw trace missing")) return 1;

    std::printf("[OK] usb-msc-replay-smoke passed\n");
    std::printf("[state] address=%u configured=%d resets=%zu cfg_calls=%zu\n",
                static_cast<unsigned>(session.address()),
                session.configured() ? 1 : 0,
                session.reset_count(),
                session.set_configured_count());
    return 0;
}
