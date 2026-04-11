#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>

import block.device;
import block.registry;
import usb.boardlog;
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

    bool pump_stall(void* ctx, usb::mock::Session& session, usb::u8 ep) noexcept {
        auto* pump = static_cast<PumpContext*>(ctx);
        if (!pump || !pump->bot || !session.dcd_ops().ep.stall) return false;

        if (ep == pump->cfg.ep_in) {
            return pump->bot->take_stall_in() && session.dcd_ops().ep.stall(&session, ep);
        }
        if (ep == pump->cfg.ep_out) {
            return pump->bot->take_stall_out() && session.dcd_ops().ep.stall(&session, ep);
        }
        return false;
    }

    bool has_host_event(std::span<const usb::mock::HostEvent> events,
                        usb::mock::HostEventKind kind,
                        usb::u8 ep = 0xFF) {
        for (const auto& event : events) {
            if (event.kind != kind) {
                continue;
            }
            if (ep == 0xFF || event.ep == ep) {
                return true;
            }
        }
        return false;
    }

    std::size_t count_host_event(std::span<const usb::mock::HostEvent> events,
                                 usb::mock::HostEventKind kind,
                                 usb::u8 ep = 0xFF) {
        std::size_t count = 0;
        for (const auto& event : events) {
            if (event.kind != kind) {
                continue;
            }
            if (ep == 0xFF || event.ep == ep) {
                ++count;
            }
        }
        return count;
    }

    std::size_t count_device_action(std::span<const usb::mock::DeviceAction> actions,
                                    usb::mock::DeviceActionKind kind,
                                    usb::u8 ep = 0xFF) {
        std::size_t count = 0;
        for (const auto& action : actions) {
            if (action.kind != kind) {
                continue;
            }
            if (ep == 0xFF || action.ep == ep) {
                ++count;
            }
        }
        return count;
    }

    const usb::class_driver::MscTraceEvent* find_msc_trace_event(std::span<const usb::class_driver::MscTraceEvent> events,
                                                                 usb::class_driver::MscTraceEventKind kind,
                                                                 usb::u8 command = 0xFF) {
        for (const auto& event : events) {
            if (event.kind != kind) {
                continue;
            }
            if (command == 0xFF || event.command == command) {
                return &event;
            }
        }
        return nullptr;
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

    const auto imported = usb::boardlog::load_file(USB_BOARD_LOG_FIXTURE_PATH);
    if (!imported) {
        std::fprintf(stderr,
                     "[ERR] boardlog load failed line=%zu err=%s\n",
                     imported.line,
                     usb::boardlog::error_name(imported.error));
        return 1;
    }
    if (!usb::fixture::expect(imported.imported_steps == 13, "unexpected imported step count")) return 1;
    if (!usb::fixture::expect(imported.skipped_steps == 0, "unexpected skipped step count")) return 1;
    if (!usb::fixture::expect(imported.trace.steps.size() == 13, "unexpected trace step size")) return 1;
    if (!usb::fixture::expect(imported.trace.steps[0].kind == usb::replay::StepKind::connect,
                              "boardlog first step should be connect")) return 1;
    if (!usb::fixture::expect(imported.trace.steps[0].flag,
                              "boardlog connect step should be true")) return 1;
    if (!usb::fixture::expect(imported.trace.steps[1].kind == usb::replay::StepKind::reset,
                              "boardlog second step should be reset")) return 1;
    if (!usb::fixture::expect(imported.trace.steps[7].kind == usb::replay::StepKind::out,
                              "boardlog first bulk step should be out")) return 1;
    if (!usb::fixture::expect(imported.trace.steps[8].kind == usb::replay::StepKind::stall,
                              "boardlog stall step should be imported")) return 1;
    if (!usb::fixture::expect(imported.trace.steps[9].kind == usb::replay::StepKind::clear_stall,
                              "boardlog recovery step should be clear_stall")) return 1;
    if (!usb::fixture::expect(imported.trace.steps[10].kind == usb::replay::StepKind::in,
                              "boardlog csw step should be in")) return 1;
    if (!usb::fixture::expect(imported.trace.steps[11].kind == usb::replay::StepKind::out,
                              "boardlog request-sense step should be out")) return 1;
    if (!usb::fixture::expect(imported.trace.steps[12].kind == usb::replay::StepKind::in,
                              "boardlog sense-response step should be in")) return 1;

    const auto trace_text = usb::boardlog::to_text(imported.trace);
    if (!usb::fixture::expect(trace_text.find("connect true") != std::string::npos,
                              "roundtrip trace missing connect")) return 1;
    if (!usb::fixture::expect(trace_text.find("reset") != std::string::npos,
                              "roundtrip trace missing reset")) return 1;
    if (!usb::fixture::expect(trace_text.find("out ep=01") != std::string::npos,
                              "roundtrip trace missing bulk out")) return 1;
    if (!usb::fixture::expect(trace_text.find("in ep=81") != std::string::npos,
                              "roundtrip trace missing bulk in")) return 1;
    if (!usb::fixture::expect(trace_text.find("stall ep=01") != std::string::npos,
                              "roundtrip trace missing stall")) return 1;
    if (!usb::fixture::expect(trace_text.find("clear_stall ep=01") != std::string::npos,
                              "roundtrip trace missing clear_stall")) return 1;
    const auto roundtrip = usb::replay::load_text(trace_text);
    if (!roundtrip) {
        std::fprintf(stderr,
                     "[ERR] roundtrip trace parse failed line=%zu err=%s\n",
                     roundtrip.line,
                     usb::replay::load_error_name(roundtrip.error));
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

    PumpContext pump{demo.bot, plan.value().msc.msc_cfg};
    const auto replay = usb::replay::run(session, roundtrip.trace, usb::replay::Hooks{&pump_in, &pump, &pump_stall});
    if (!replay) {
        std::fprintf(stderr,
                     "[ERR] imported replay failed step=%zu err=%s\n",
                     replay.step_index,
                     usb::replay::error_name(replay.error));
        return 1;
    }

    const auto msc_cfg = plan.value().msc.msc_cfg;
    if (!usb::fixture::expect(session.address() == 7, "device address not applied")) return 1;
    if (!usb::fixture::expect(session.configured(), "device not configured")) return 1;
    if (!usb::fixture::expect(session.endpoint_state(msc_cfg.ep_out).opened, "msc bulk out endpoint not opened")) return 1;
    if (!usb::fixture::expect(session.endpoint_state(msc_cfg.ep_in).opened, "msc bulk in endpoint not opened")) return 1;
    if (!usb::fixture::expect(!session.endpoint_state(msc_cfg.ep_out).stalled, "msc bulk out endpoint should be cleared after recovery")) return 1;
    if (!usb::fixture::expect(has_host_event(session.host_events(), usb::mock::HostEventKind::connect),
                              "connect host event missing")) return 1;
    if (!usb::fixture::expect(has_host_event(session.host_events(), usb::mock::HostEventKind::reset),
                              "reset host event missing")) return 1;
    if (!usb::fixture::expect(count_device_action(session.device_actions(), usb::mock::DeviceActionKind::stall_ep, msc_cfg.ep_out) == 1,
                              "unexpected bulk stall device action count")) return 1;
    if (!usb::fixture::expect(count_host_event(session.host_events(), usb::mock::HostEventKind::out_packet, msc_cfg.ep_out) == 2,
                              "unexpected bulk out host event count")) return 1;
    if (!usb::fixture::expect(count_host_event(session.host_events(), usb::mock::HostEventKind::in_complete, msc_cfg.ep_in) == 3,
                              "unexpected bulk in completion count")) return 1;
    if (!usb::fixture::expect(has_host_event(session.host_events(), usb::mock::HostEventKind::clear_stall, msc_cfg.ep_out),
                              "clear-stall host event missing")) return 1;

    constexpr auto kRead10 = static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_10);
    const auto* wait_csw = find_msc_trace_event(demo.bot->trace_events(),
                                                usb::class_driver::MscTraceEventKind::wait_csw,
                                                kRead10);
    if (!usb::fixture::expect(wait_csw != nullptr, "boardlog recovery wait-csw trace missing")) return 1;
    if (!usb::fixture::expect(wait_csw->transfer_length == 512, "boardlog recovery wait-csw length mismatch")) return 1;

    const auto* clear_stall = find_msc_trace_event(demo.bot->trace_events(),
                                                   usb::class_driver::MscTraceEventKind::clear_stall_seen,
                                                   kRead10);
    if (!usb::fixture::expect(clear_stall != nullptr, "boardlog recovery clear-stall trace missing")) return 1;
    if (!usb::fixture::expect(!clear_stall->flag, "boardlog recovery clear-stall direction mismatch")) return 1;

    const auto* sense = find_msc_trace_event(demo.bot->trace_events(),
                                             usb::class_driver::MscTraceEventKind::sense_set,
                                             kRead10);
    if (!usb::fixture::expect(sense != nullptr, "boardlog recovery sense trace missing")) return 1;
    if (!usb::fixture::expect(sense->sense_key == 0x05, "boardlog recovery sense key mismatch")) return 1;
    if (!usb::fixture::expect(sense->sense_asc == 0x20, "boardlog recovery sense asc mismatch")) return 1;
    if (!usb::fixture::expect(sense->sense_ascq == 0x00, "boardlog recovery sense ascq mismatch")) return 1;

    const auto* csw_sent = find_msc_trace_event(demo.bot->trace_events(),
                                                usb::class_driver::MscTraceEventKind::csw_sent,
                                                kRead10);
    if (!usb::fixture::expect(csw_sent != nullptr, "boardlog recovery csw trace missing")) return 1;
    if (!usb::fixture::expect(csw_sent->residue == 512, "boardlog recovery csw residue mismatch")) return 1;
    if (!usb::fixture::expect(csw_sent->flag, "boardlog recovery csw phase flag mismatch")) return 1;

    std::printf("[OK] usb-msc-boardlog-import-smoke passed\n");
    std::printf("[state] imported=%zu skipped=%zu address=%u configured=%d\n",
                imported.imported_steps,
                imported.skipped_steps,
                static_cast<unsigned>(session.address()),
                session.configured() ? 1 : 0);
    return 0;
}
