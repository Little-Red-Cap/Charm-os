#include <algorithm>
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
            device.erase = nullptr;
            device.flush = nullptr;
            device.block_size = block_size;
            device.block_count = block_count;
            set_writable(false);
        }

        void set_writable(bool writable) noexcept {
            device.write = writable ? &MemoryDisk::write_cb : nullptr;
            device.caps = block::to_bits(block::Caps::read)
                        | (writable ? block::to_bits(block::Caps::write) : 0u);
        }

        [[nodiscard]] std::span<const usb::u8> block_span(std::size_t lba) const noexcept {
            if (lba >= block_count) {
                return {};
            }
            return std::span<const usb::u8>(bytes.data() + (lba * block_size), block_size);
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

        static block::Status write_cb(void* ctx, util::u64 lba, std::span<const util::u8> data) noexcept {
            auto* self = static_cast<MemoryDisk*>(ctx);
            if (!self || data.empty() || (data.size() % block_size) != 0) {
                return {util::Errc::invalid_arg};
            }
            const auto blocks = static_cast<util::u64>(data.size() / block_size);
            if (lba + blocks > block_count) {
                return {util::Errc::invalid_arg};
            }
            const auto offset = static_cast<std::size_t>(lba * block_size);
            std::memcpy(self->bytes.data() + offset, data.data(), data.size());
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

    std::size_t count_host_zlp_ack(std::span<const usb::mock::HostEvent> events,
                                   usb::u8 ep = 0xFF) {
        std::size_t count = 0;
        for (const auto& event : events) {
            if (event.kind != usb::mock::HostEventKind::in_complete) {
                continue;
            }
            if (!event.flag) {
                continue;
            }
            if (ep == 0xFF || event.ep == ep) {
                ++count;
            }
        }
        return count;
    }

    std::size_t count_substring(std::string_view text, std::string_view needle) {
        if (needle.empty()) {
            return 0;
        }
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = text.find(needle, pos)) != std::string_view::npos) {
            ++count;
            pos += needle.size();
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

    bool span_is_filled(std::span<const usb::u8> bytes, usb::u8 value) {
        for (const auto byte : bytes) {
            if (byte != value) {
                return false;
            }
        }
        return true;
    }
}

int main() {
    {
        constexpr std::string_view kZlpBoardlog =
            "usb: connect on\n"
            "usb: reset\n"
            "usb: out ep=0x01 zlp=1 data=-\n"
            "usb: in ep=0x81 zlp=1 data=-\n";

        const auto imported_zlp = usb::boardlog::load_text(kZlpBoardlog);
        if (!imported_zlp) {
            std::fprintf(stderr,
                         "[ERR] zlp boardlog load failed line=%zu err=%s\n",
                         imported_zlp.line,
                         usb::boardlog::error_name(imported_zlp.error));
            return 1;
        }
        if (!usb::fixture::expect(imported_zlp.imported_steps == 4, "zlp boardlog imported step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_zlp.trace.steps.size() == 4, "zlp boardlog trace size mismatch")) return 1;
        if (!usb::fixture::expect(imported_zlp.trace.steps[2].kind == usb::replay::StepKind::out,
                                  "zlp boardlog out step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_zlp.trace.steps[2].flag,
                                  "zlp boardlog out step should carry zlp flag")) return 1;
        if (!usb::fixture::expect(imported_zlp.trace.steps[2].data.empty(),
                                  "zlp boardlog out step should have empty data")) return 1;
        if (!usb::fixture::expect(imported_zlp.trace.steps[3].kind == usb::replay::StepKind::in,
                                  "zlp boardlog in step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_zlp.trace.steps[3].flag,
                                  "zlp boardlog in step should carry zlp flag")) return 1;
        if (!usb::fixture::expect(imported_zlp.trace.steps[3].data.empty(),
                                  "zlp boardlog in step should have empty data")) return 1;

        const auto zlp_trace_text = usb::boardlog::to_text(imported_zlp.trace);
        if (!usb::fixture::expect(zlp_trace_text.find("out ep=01 zlp=1 data=-") != std::string::npos,
                                  "zlp boardlog roundtrip missing out zlp")) return 1;
        if (!usb::fixture::expect(zlp_trace_text.find("in ep=81 zlp=1 data=-") != std::string::npos,
                                  "zlp boardlog roundtrip missing in zlp")) return 1;
    }
    {
        constexpr std::string_view kSegmentedInBoardlog =
            "usb: connect on\n"
            "usb: reset\n"
            "usb: in ep=0x81 zlp=0 data=01020304\n"
            "usb: in ep=0x81 zlp=0 data=05060708\n"
            "usb: in ep=0x81 zlp=1 data=-\n";

        const auto imported_segmented = usb::boardlog::load_text(kSegmentedInBoardlog);
        if (!imported_segmented) {
            std::fprintf(stderr,
                         "[ERR] segmented boardlog load failed line=%zu err=%s\n",
                         imported_segmented.line,
                         usb::boardlog::error_name(imported_segmented.error));
            return 1;
        }
        if (!usb::fixture::expect(imported_segmented.imported_steps == 5, "segmented boardlog imported step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented.trace.steps.size() == 3, "segmented boardlog trace size mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented.trace.steps[2].kind == usb::replay::StepKind::in,
                                  "segmented boardlog merged step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented.trace.steps[2].ep == 0x81,
                                  "segmented boardlog merged endpoint mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented.trace.steps[2].flag,
                                  "segmented boardlog merged step should preserve terminal zlp")) return 1;
        constexpr usb::u8 kSegmentedPayload[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        const auto segmented_data = std::span<const usb::u8>(kSegmentedPayload, sizeof(kSegmentedPayload));
        if (!usb::fixture::expect(imported_segmented.trace.steps[2].data.size() == segmented_data.size(),
                                  "segmented boardlog merged payload size mismatch")) return 1;
        if (!usb::fixture::expect(std::equal(imported_segmented.trace.steps[2].data.begin(),
                                            imported_segmented.trace.steps[2].data.end(),
                                            segmented_data.begin(),
                                            segmented_data.end()),
                                  "segmented boardlog merged payload mismatch")) return 1;

        const auto segmented_trace_text = usb::boardlog::to_text(imported_segmented.trace);
        if (!usb::fixture::expect(segmented_trace_text.find("in ep=81 zlp=1 data=0102030405060708") != std::string::npos,
                                  "segmented boardlog roundtrip missing merged in transaction")) return 1;
    }

    const auto repeat_hex_byte = [] (usb::u8 value, std::size_t count) {
        static constexpr char kHex[] = "0123456789ABCDEF";
        std::string out{};
        out.reserve(count * 2);
        for (std::size_t i = 0; i < count; ++i) {
            out.push_back(kHex[(value >> 4u) & 0x0Fu]);
            out.push_back(kHex[value & 0x0Fu]);
        }
        return out;
    };

    const auto make_device_spec = []() {
        usb::spec::DeviceSpec device{};
        device.vendor_id = 0x1209;
        device.product_id = 0x0006;
        device.i_manufacturer = 1;
        device.i_product = 2;
        device.i_serial = 3;
        device.strings = std::span<const std::span<const usb::u8>>(kStrings.entries.data(), kStrings.entries.size());
        return device;
    };

    const auto make_msc_function = [] (bool read_only) {
        usb::spec::MscFunctionSpec msc{};
        msc.cap_name = "usb.msc0";
        msc.block_cap = "block.sd0";
        msc.vendor = "Charm";
        msc.product = "MockDisk";
        msc.revision = "1.00";
        msc.read_only = read_only;
        return msc;
    };

    {
        std::string segmented_out_boardlog{};
        segmented_out_boardlog.reserve(4096);
        segmented_out_boardlog += "usb: connect on\n";
        segmented_out_boardlog += "usb: reset\n";
        segmented_out_boardlog += "usb: dev_desc size=18 12 01 00 02 00 00 00 40 09 12 06 00 00 01 01 02 03 01\n";
        segmented_out_boardlog += "usb: cfg_desc size=32 09 02 20 00 01 01 00 80 32 09 04 00 00 02 08 06 50 00 07 05 01 02 40 00 00 07 05 81 02 40 00 00\n";
        segmented_out_boardlog += "usb: setup bm=0x80 b=0x06 wValue=0x0100 wIndex=0x0000 wLen=0x0040\n";
        segmented_out_boardlog += "usb: setup bm=0x00 b=0x05 wValue=0x0007 wIndex=0x0000 wLen=0x0000\n";
        segmented_out_boardlog += "usb: setup bm=0x80 b=0x06 wValue=0x0200 wIndex=0x0000 wLen=0x00FF\n";
        segmented_out_boardlog += "usb: setup bm=0x00 b=0x09 wValue=0x0001 wIndex=0x0000 wLen=0x0000\n";
        segmented_out_boardlog += "usb: setup bm=0xA1 b=0xFE wValue=0x0000 wIndex=0x0000 wLen=0x0001\n";
        segmented_out_boardlog += "usb: out ep=0x01 zlp=0 data=555342430D0000000002000000000A2A000000000100000200000000000000\n";
        segmented_out_boardlog += "usb: out ep=0x01 zlp=0 data=";
        segmented_out_boardlog += repeat_hex_byte(0xA5, 256);
        segmented_out_boardlog += "\n";
        segmented_out_boardlog += "usb: out ep=0x01 zlp=0 data=";
        segmented_out_boardlog += repeat_hex_byte(0xA5, 256);
        segmented_out_boardlog += "\n";
        segmented_out_boardlog += "usb: in ep=0x81 zlp=0 data=555342530D0000000002000000\n";

        const auto imported_segmented_out = usb::boardlog::load_text(segmented_out_boardlog);
        if (!imported_segmented_out) {
            std::fprintf(stderr,
                         "[ERR] segmented-out boardlog load failed line=%zu err=%s\n",
                         imported_segmented_out.line,
                         usb::boardlog::error_name(imported_segmented_out.error));
            return 1;
        }
        if (!usb::fixture::expect(imported_segmented_out.imported_steps == 11, "segmented-out boardlog imported step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out.skipped_steps == 0, "segmented-out boardlog skipped step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out.trace.steps.size() == 11, "segmented-out boardlog trace size mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out.trace.steps[7].kind == usb::replay::StepKind::out,
                                  "segmented-out boardlog cbw step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out.trace.steps[8].kind == usb::replay::StepKind::out,
                                  "segmented-out boardlog first payload step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out.trace.steps[9].kind == usb::replay::StepKind::out,
                                  "segmented-out boardlog second payload step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out.trace.steps[10].kind == usb::replay::StepKind::in,
                                  "segmented-out boardlog csw step kind mismatch")) return 1;

        const auto segmented_out_trace_text = usb::boardlog::to_text(imported_segmented_out.trace);
        if (!usb::fixture::expect(count_substring(segmented_out_trace_text, "out ep=01 zlp=0 data=") == 3,
                                  "segmented-out boardlog should preserve packet boundaries")) return 1;
        const auto segmented_out_roundtrip = usb::replay::load_text(segmented_out_trace_text);
        if (!segmented_out_roundtrip) {
            std::fprintf(stderr,
                         "[ERR] segmented-out roundtrip parse failed line=%zu err=%s\n",
                         segmented_out_roundtrip.line,
                         usb::replay::load_error_name(segmented_out_roundtrip.error));
            return 1;
        }
        if (!usb::fixture::expect(segmented_out_roundtrip.trace.steps.size() == 11,
                                  "segmented-out roundtrip trace size mismatch")) return 1;

        MemoryDisk write_disk{};
        write_disk.set_writable(true);
        block::Registry<2> write_registry{};
        write_registry.init();
        auto write_reg = write_registry.register_device({"block.sd0", block::cap_id("block.sd0")}, write_disk.device);
        if (!write_reg) {
            std::fprintf(stderr, "[ERR] write registry register failed err=%d\n", static_cast<int>(write_reg.error()));
            return 1;
        }

        DemoContext write_demo{};
        usb::mock::Session write_session{};
        const auto write_spec = usb::spec::msc_device(make_device_spec(), make_msc_function(false));
        const auto write_model = usb::build(write_spec);
        const auto write_plan = usb::plan::build(write_model);
        if (!write_plan) {
            std::fprintf(stderr, "[ERR] write plan build failed err=%d\n", static_cast<int>(write_plan.error()));
            return 1;
        }

        const auto write_runtime = usb::runtime::host_mock(
            write_session.dcd_ops(),
            &write_session,
            &write_session.adapter(),
            usb::runtime::MscReadyHook{&on_ready, &write_demo});

        auto write_binding = usb::runtime::make(write_plan.value(), write_registry, write_runtime);
        const auto write_init = decltype(write_binding)::init_trampoline(&write_binding);
        if (!write_init) {
            std::fprintf(stderr, "[ERR] write binding init failed err=%d\n", static_cast<int>(write_init.error()));
            return 1;
        }
        if (!usb::fixture::expect(write_demo.ready, "write segmented runtime ready hook not called")) return 1;

        PumpContext write_pump{write_demo.bot, write_plan.value().msc.msc_cfg};
        const auto write_replay = usb::replay::run(
            write_session,
            segmented_out_roundtrip.trace,
            usb::replay::Hooks{&pump_in, &write_pump, &pump_stall});
        if (!write_replay) {
            std::fprintf(stderr,
                         "[ERR] segmented-out replay failed step=%zu err=%s\n",
                         write_replay.step_index,
                         usb::replay::error_name(write_replay.error));
            return 1;
        }

        const auto write_cfg = write_plan.value().msc.msc_cfg;
        if (!usb::fixture::expect(count_host_event(write_session.host_events(), usb::mock::HostEventKind::out_packet, write_cfg.ep_out) == 3,
                                  "segmented-out host event count mismatch")) return 1;
        if (!usb::fixture::expect(count_host_event(write_session.host_events(), usb::mock::HostEventKind::in_complete, write_cfg.ep_in) == 1,
                                  "segmented-out csw ack count mismatch")) return 1;
        if (!usb::fixture::expect(!has_host_event(write_session.host_events(), usb::mock::HostEventKind::clear_stall, write_cfg.ep_out),
                                  "segmented-out write should not clear stall")) return 1;

        constexpr auto kWrite10 = static_cast<usb::u8>(usb::class_driver::ScsiCmd::write_10);
        const auto* write10 = find_msc_trace_event(write_demo.bot->trace_events(),
                                                   usb::class_driver::MscTraceEventKind::write10_started,
                                                   kWrite10);
        if (!usb::fixture::expect(write10 != nullptr, "segmented-out write10 trace missing")) return 1;
        if (!usb::fixture::expect(write10->transfer_length == 512, "segmented-out write10 length mismatch")) return 1;
        if (!usb::fixture::expect(write10->lba == 1, "segmented-out write10 lba mismatch")) return 1;
        if (!usb::fixture::expect(write10->blocks == 2, "segmented-out write10 block count mismatch")) return 1;

        const auto* data_out = find_msc_trace_event(write_demo.bot->trace_events(),
                                                    usb::class_driver::MscTraceEventKind::data_out_started,
                                                    kWrite10);
        if (!usb::fixture::expect(data_out != nullptr, "segmented-out data-out trace missing")) return 1;
        if (!usb::fixture::expect(data_out->transfer_length == 512, "segmented-out data-out length mismatch")) return 1;
        if (!usb::fixture::expect(data_out->lba == 1, "segmented-out data-out lba mismatch")) return 1;
        if (!usb::fixture::expect(data_out->blocks == 2, "segmented-out data-out block count mismatch")) return 1;
        if (!usb::fixture::expect(data_out->residue == 512, "segmented-out data-out residue mismatch")) return 1;

        const auto* csw_sent = find_msc_trace_event(write_demo.bot->trace_events(),
                                                    usb::class_driver::MscTraceEventKind::csw_sent,
                                                    kWrite10);
        if (!usb::fixture::expect(csw_sent != nullptr, "segmented-out csw trace missing")) return 1;
        if (!usb::fixture::expect(csw_sent->residue == 512, "segmented-out csw residue mismatch")) return 1;
        if (!usb::fixture::expect(!csw_sent->flag, "segmented-out csw phase flag mismatch")) return 1;

        if (!usb::fixture::expect(span_is_filled(write_disk.block_span(1), 0xA5), "segmented-out block payload mismatch")) return 1;
        if (!usb::fixture::expect(span_is_filled(write_disk.block_span(2), 0x00), "segmented-out trailing block should remain empty")) return 1;
    }

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
    if (!usb::fixture::expect(imported.trace.steps[3].kind == usb::replay::StepKind::control_out,
                              "boardlog set-address step should be control_out")) return 1;
    if (!usb::fixture::expect(imported.trace.steps[3].flag,
                              "boardlog set-address step should expect zlp")) return 1;
    if (!usb::fixture::expect(imported.trace.steps[5].kind == usb::replay::StepKind::control_out,
                              "boardlog set-configuration step should be control_out")) return 1;
    if (!usb::fixture::expect(imported.trace.steps[5].flag,
                              "boardlog set-configuration step should expect zlp")) return 1;
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
    if (!usb::fixture::expect(trace_text.find("control_out bm=00 b=05 wv=0007 wi=0000 wl=0000 zlp=1 data=-") != std::string::npos,
                              "roundtrip trace missing set-address zlp")) return 1;
    if (!usb::fixture::expect(trace_text.find("control_out bm=00 b=09 wv=0001 wi=0000 wl=0000 zlp=1 data=-") != std::string::npos,
                              "roundtrip trace missing set-configuration zlp")) return 1;
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
    if (!usb::fixture::expect(count_device_action(session.device_actions(), usb::mock::DeviceActionKind::send_zlp, 0x80) == 3,
                              "unexpected ep0 send-zlp device action count")) return 1;
    if (!usb::fixture::expect(has_host_event(session.host_events(), usb::mock::HostEventKind::connect),
                              "connect host event missing")) return 1;
    if (!usb::fixture::expect(has_host_event(session.host_events(), usb::mock::HostEventKind::reset),
                              "reset host event missing")) return 1;
    if (!usb::fixture::expect(count_host_zlp_ack(session.host_events(), 0x80) == 3,
                              "unexpected ep0 zlp ack host event count")) return 1;
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
