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

    const auto fixture_path = [] (std::string_view name) {
        std::string out{USB_BOARDLOG_FIXTURE_DIR};
        if (!out.empty() && out.back() != '/' && out.back() != '\\') {
            out.push_back('/');
        }
        out.append(name);
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
    {
        std::string segmented_out_recovery_boardlog{};
        segmented_out_recovery_boardlog.reserve(6144);
        segmented_out_recovery_boardlog += "usb: connect on\n";
        segmented_out_recovery_boardlog += "usb: reset\n";
        segmented_out_recovery_boardlog += "usb: dev_desc size=18 12 01 00 02 00 00 00 40 09 12 06 00 00 01 01 02 03 01\n";
        segmented_out_recovery_boardlog += "usb: cfg_desc size=32 09 02 20 00 01 01 00 80 32 09 04 00 00 02 08 06 50 00 07 05 01 02 40 00 00 07 05 81 02 40 00 00\n";
        segmented_out_recovery_boardlog += "usb: setup bm=0x80 b=0x06 wValue=0x0100 wIndex=0x0000 wLen=0x0040\n";
        segmented_out_recovery_boardlog += "usb: setup bm=0x00 b=0x05 wValue=0x0007 wIndex=0x0000 wLen=0x0000\n";
        segmented_out_recovery_boardlog += "usb: setup bm=0x80 b=0x06 wValue=0x0200 wIndex=0x0000 wLen=0x00FF\n";
        segmented_out_recovery_boardlog += "usb: setup bm=0x00 b=0x09 wValue=0x0001 wIndex=0x0000 wLen=0x0000\n";
        segmented_out_recovery_boardlog += "usb: setup bm=0xA1 b=0xFE wValue=0x0000 wIndex=0x0000 wLen=0x0001\n";
        segmented_out_recovery_boardlog += "usb: out ep=0x01 zlp=0 data=555342430E0000000004000000000A2A000000000100000100000000000000\n";
        segmented_out_recovery_boardlog += "usb: out ep=0x01 zlp=0 data=";
        segmented_out_recovery_boardlog += repeat_hex_byte(0x5A, 512);
        segmented_out_recovery_boardlog += "\n";
        segmented_out_recovery_boardlog += "usb: out ep=0x01 zlp=0 data=";
        segmented_out_recovery_boardlog += repeat_hex_byte(0xC3, 512);
        segmented_out_recovery_boardlog += "\n";
        segmented_out_recovery_boardlog += "usb: stall ep=0x01\n";
        segmented_out_recovery_boardlog += "usb: setup bm=0x02 b=0x01 wValue=0x0000 wIndex=0x0001 wLen=0x0000\n";
        segmented_out_recovery_boardlog += "usb: in ep=0x81 zlp=0 data=555342530E0000000002000002\n";

        const auto imported_segmented_out_recovery = usb::boardlog::load_text(segmented_out_recovery_boardlog);
        if (!imported_segmented_out_recovery) {
            std::fprintf(stderr,
                         "[ERR] segmented-out recovery boardlog load failed line=%zu err=%s\n",
                         imported_segmented_out_recovery.line,
                         usb::boardlog::error_name(imported_segmented_out_recovery.error));
            return 1;
        }
        if (!usb::fixture::expect(imported_segmented_out_recovery.imported_steps == 13, "segmented-out recovery imported step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out_recovery.skipped_steps == 0, "segmented-out recovery skipped step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out_recovery.trace.steps.size() == 13, "segmented-out recovery trace size mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out_recovery.trace.steps[7].kind == usb::replay::StepKind::out,
                                  "segmented-out recovery cbw step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out_recovery.trace.steps[8].kind == usb::replay::StepKind::out,
                                  "segmented-out recovery first payload step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out_recovery.trace.steps[9].kind == usb::replay::StepKind::out,
                                  "segmented-out recovery second payload step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out_recovery.trace.steps[10].kind == usb::replay::StepKind::stall,
                                  "segmented-out recovery stall step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out_recovery.trace.steps[11].kind == usb::replay::StepKind::clear_stall,
                                  "segmented-out recovery clear-stall step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_segmented_out_recovery.trace.steps[12].kind == usb::replay::StepKind::in,
                                  "segmented-out recovery csw step kind mismatch")) return 1;

        const auto segmented_out_recovery_trace_text = usb::boardlog::to_text(imported_segmented_out_recovery.trace);
        if (!usb::fixture::expect(count_substring(segmented_out_recovery_trace_text, "out ep=01 zlp=0 data=") == 3,
                                  "segmented-out recovery should preserve packet boundaries")) return 1;
        if (!usb::fixture::expect(segmented_out_recovery_trace_text.find("stall ep=01") != std::string::npos,
                                  "segmented-out recovery roundtrip missing stall")) return 1;
        if (!usb::fixture::expect(segmented_out_recovery_trace_text.find("clear_stall ep=01") != std::string::npos,
                                  "segmented-out recovery roundtrip missing clear_stall")) return 1;

        const auto segmented_out_recovery_roundtrip = usb::replay::load_text(segmented_out_recovery_trace_text);
        if (!segmented_out_recovery_roundtrip) {
            std::fprintf(stderr,
                         "[ERR] segmented-out recovery roundtrip parse failed line=%zu err=%s\n",
                         segmented_out_recovery_roundtrip.line,
                         usb::replay::load_error_name(segmented_out_recovery_roundtrip.error));
            return 1;
        }
        if (!usb::fixture::expect(segmented_out_recovery_roundtrip.trace.steps.size() == 13,
                                  "segmented-out recovery roundtrip trace size mismatch")) return 1;

        MemoryDisk recovery_disk{};
        recovery_disk.set_writable(true);
        block::Registry<2> recovery_registry{};
        recovery_registry.init();
        auto recovery_reg = recovery_registry.register_device({"block.sd0", block::cap_id("block.sd0")}, recovery_disk.device);
        if (!recovery_reg) {
            std::fprintf(stderr, "[ERR] recovery registry register failed err=%d\n", static_cast<int>(recovery_reg.error()));
            return 1;
        }

        DemoContext recovery_demo{};
        usb::mock::Session recovery_session{};
        const auto recovery_spec = usb::spec::msc_device(make_device_spec(), make_msc_function(false));
        const auto recovery_model = usb::build(recovery_spec);
        const auto recovery_plan = usb::plan::build(recovery_model);
        if (!recovery_plan) {
            std::fprintf(stderr, "[ERR] recovery plan build failed err=%d\n", static_cast<int>(recovery_plan.error()));
            return 1;
        }

        const auto recovery_runtime = usb::runtime::host_mock(
            recovery_session.dcd_ops(),
            &recovery_session,
            &recovery_session.adapter(),
            usb::runtime::MscReadyHook{&on_ready, &recovery_demo});

        auto recovery_binding = usb::runtime::make(recovery_plan.value(), recovery_registry, recovery_runtime);
        const auto recovery_init = decltype(recovery_binding)::init_trampoline(&recovery_binding);
        if (!recovery_init) {
            std::fprintf(stderr, "[ERR] recovery binding init failed err=%d\n", static_cast<int>(recovery_init.error()));
            return 1;
        }
        if (!usb::fixture::expect(recovery_demo.ready, "recovery segmented runtime ready hook not called")) return 1;

        PumpContext recovery_pump{recovery_demo.bot, recovery_plan.value().msc.msc_cfg};
        const auto recovery_replay = usb::replay::run(
            recovery_session,
            segmented_out_recovery_roundtrip.trace,
            usb::replay::Hooks{&pump_in, &recovery_pump, &pump_stall});
        if (!recovery_replay) {
            std::fprintf(stderr,
                         "[ERR] segmented-out recovery replay failed step=%zu err=%s\n",
                         recovery_replay.step_index,
                         usb::replay::error_name(recovery_replay.error));
            return 1;
        }

        const auto recovery_cfg = recovery_plan.value().msc.msc_cfg;
        if (!usb::fixture::expect(count_host_event(recovery_session.host_events(), usb::mock::HostEventKind::out_packet, recovery_cfg.ep_out) == 3,
                                  "segmented-out recovery host event count mismatch")) return 1;
        if (!usb::fixture::expect(count_host_event(recovery_session.host_events(), usb::mock::HostEventKind::in_complete, recovery_cfg.ep_in) == 1,
                                  "segmented-out recovery csw ack count mismatch")) return 1;
        if (!usb::fixture::expect(has_host_event(recovery_session.host_events(), usb::mock::HostEventKind::clear_stall, recovery_cfg.ep_out),
                                  "segmented-out recovery clear-stall host event missing")) return 1;
        if (!usb::fixture::expect(count_device_action(recovery_session.device_actions(), usb::mock::DeviceActionKind::stall_ep, recovery_cfg.ep_out) == 1,
                                  "segmented-out recovery stall device action count mismatch")) return 1;
        if (!usb::fixture::expect(!recovery_session.endpoint_state(recovery_cfg.ep_out).stalled,
                                  "segmented-out recovery endpoint should be cleared after recovery")) return 1;

        constexpr auto kWrite10Recovery = static_cast<usb::u8>(usb::class_driver::ScsiCmd::write_10);
        const auto* recovery_write10 = find_msc_trace_event(recovery_demo.bot->trace_events(),
                                                            usb::class_driver::MscTraceEventKind::write10_started,
                                                            kWrite10Recovery);
        if (!usb::fixture::expect(recovery_write10 != nullptr, "segmented-out recovery write10 trace missing")) return 1;
        if (!usb::fixture::expect(recovery_write10->transfer_length == 1024, "segmented-out recovery write10 length mismatch")) return 1;
        if (!usb::fixture::expect(recovery_write10->lba == 1, "segmented-out recovery write10 lba mismatch")) return 1;
        if (!usb::fixture::expect(recovery_write10->blocks == 1, "segmented-out recovery write10 block count mismatch")) return 1;

        const auto* recovery_data_out = find_msc_trace_event(recovery_demo.bot->trace_events(),
                                                             usb::class_driver::MscTraceEventKind::data_out_started,
                                                             kWrite10Recovery);
        if (!usb::fixture::expect(recovery_data_out != nullptr, "segmented-out recovery data-out trace missing")) return 1;
        if (!usb::fixture::expect(recovery_data_out->transfer_length == 512, "segmented-out recovery data-out length mismatch")) return 1;
        if (!usb::fixture::expect(recovery_data_out->lba == 1, "segmented-out recovery data-out lba mismatch")) return 1;
        if (!usb::fixture::expect(recovery_data_out->blocks == 1, "segmented-out recovery data-out block count mismatch")) return 1;
        if (!usb::fixture::expect(recovery_data_out->residue == 512, "segmented-out recovery data-out residue mismatch")) return 1;

        const auto* recovery_stall_out = find_msc_trace_event(recovery_demo.bot->trace_events(),
                                                              usb::class_driver::MscTraceEventKind::stall_out_requested,
                                                              kWrite10Recovery);
        if (!usb::fixture::expect(recovery_stall_out != nullptr, "segmented-out recovery stall-out trace missing")) return 1;
        if (!usb::fixture::expect(recovery_stall_out->transfer_length == 1024, "segmented-out recovery stall-out length mismatch")) return 1;
        if (!usb::fixture::expect(recovery_stall_out->residue == 512, "segmented-out recovery stall-out residue mismatch")) return 1;
        if (!usb::fixture::expect(!recovery_stall_out->flag, "segmented-out recovery stall-out direction mismatch")) return 1;

        const auto* recovery_wait_csw = find_msc_trace_event(recovery_demo.bot->trace_events(),
                                                             usb::class_driver::MscTraceEventKind::wait_csw,
                                                             kWrite10Recovery);
        if (!usb::fixture::expect(recovery_wait_csw != nullptr, "segmented-out recovery wait-csw trace missing")) return 1;
        if (!usb::fixture::expect(recovery_wait_csw->transfer_length == 1024, "segmented-out recovery wait-csw length mismatch")) return 1;

        const auto* recovery_phase_error = find_msc_trace_event(recovery_demo.bot->trace_events(),
                                                                usb::class_driver::MscTraceEventKind::phase_error,
                                                                kWrite10Recovery);
        if (!usb::fixture::expect(recovery_phase_error != nullptr, "segmented-out recovery phase-error trace missing")) return 1;
        if (!usb::fixture::expect(recovery_phase_error->residue == 512, "segmented-out recovery phase-error residue mismatch")) return 1;

        const auto* recovery_clear_stall = find_msc_trace_event(recovery_demo.bot->trace_events(),
                                                                usb::class_driver::MscTraceEventKind::clear_stall_seen,
                                                                kWrite10Recovery);
        if (!usb::fixture::expect(recovery_clear_stall != nullptr, "segmented-out recovery clear-stall trace missing")) return 1;
        if (!usb::fixture::expect(!recovery_clear_stall->flag, "segmented-out recovery clear-stall direction mismatch")) return 1;

        const auto* recovery_csw_sent = find_msc_trace_event(recovery_demo.bot->trace_events(),
                                                             usb::class_driver::MscTraceEventKind::csw_sent,
                                                             kWrite10Recovery);
        if (!usb::fixture::expect(recovery_csw_sent != nullptr, "segmented-out recovery csw trace missing")) return 1;
        if (!usb::fixture::expect(recovery_csw_sent->residue == 512, "segmented-out recovery csw residue mismatch")) return 1;
        if (!usb::fixture::expect(recovery_csw_sent->flag, "segmented-out recovery csw phase flag mismatch")) return 1;

        if (!usb::fixture::expect(span_is_filled(recovery_disk.block_span(1), 0x5A), "segmented-out recovery block payload mismatch")) return 1;
        if (!usb::fixture::expect(span_is_filled(recovery_disk.block_span(2), 0x00), "segmented-out recovery extra data should not spill")) return 1;
    }
    {
        constexpr auto kInvalidCbw = "55534243060000000800000080000025000000000000000000000000000000";
        constexpr auto kInvalidCsw = "55534253060000000800000002";
        constexpr auto kRecoveryCbw = "55534243070000000800000080000A25000000000000000000000000000000";
        constexpr auto kRecoveryReadCapacity = "0000000F0000020055534253070000000000000000";

        const auto imported_invalid_cbw_recovery = usb::boardlog::load_file(fixture_path("invalid_cbw_recovery.boardlog"));
        if (!imported_invalid_cbw_recovery) {
            std::fprintf(stderr,
                         "[ERR] invalid-cbw recovery boardlog load failed line=%zu err=%s\n",
                         imported_invalid_cbw_recovery.line,
                         usb::boardlog::error_name(imported_invalid_cbw_recovery.error));
            return 1;
        }
        if (!usb::fixture::expect(imported_invalid_cbw_recovery.imported_steps == 13, "invalid-cbw recovery imported step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_invalid_cbw_recovery.skipped_steps == 0, "invalid-cbw recovery skipped step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_invalid_cbw_recovery.trace.steps.size() == 13, "invalid-cbw recovery trace size mismatch")) return 1;
        if (!usb::fixture::expect(imported_invalid_cbw_recovery.trace.steps[7].kind == usb::replay::StepKind::out,
                                  "invalid-cbw recovery invalid-cbw step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_invalid_cbw_recovery.trace.steps[8].kind == usb::replay::StepKind::stall,
                                  "invalid-cbw recovery stall step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_invalid_cbw_recovery.trace.steps[8].ep == 0x81,
                                  "invalid-cbw recovery stall endpoint mismatch")) return 1;
        if (!usb::fixture::expect(imported_invalid_cbw_recovery.trace.steps[9].kind == usb::replay::StepKind::clear_stall,
                                  "invalid-cbw recovery clear-stall step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_invalid_cbw_recovery.trace.steps[9].ep == 0x81,
                                  "invalid-cbw recovery clear-stall endpoint mismatch")) return 1;
        if (!usb::fixture::expect(imported_invalid_cbw_recovery.trace.steps[10].kind == usb::replay::StepKind::in,
                                  "invalid-cbw recovery phase-error csw step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_invalid_cbw_recovery.trace.steps[11].kind == usb::replay::StepKind::out,
                                  "invalid-cbw recovery recovery-cbw step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_invalid_cbw_recovery.trace.steps[12].kind == usb::replay::StepKind::in,
                                  "invalid-cbw recovery read-capacity step kind mismatch")) return 1;

        const auto invalid_cbw_recovery_trace_text = usb::boardlog::to_text(imported_invalid_cbw_recovery.trace);
        if (!usb::fixture::expect(count_substring(invalid_cbw_recovery_trace_text, "out ep=01 zlp=0 data=") == 2,
                                  "invalid-cbw recovery roundtrip out count mismatch")) return 1;
        if (!usb::fixture::expect(count_substring(invalid_cbw_recovery_trace_text, "in ep=81 zlp=0 data=") == 2,
                                  "invalid-cbw recovery roundtrip in count mismatch")) return 1;
        if (!usb::fixture::expect(invalid_cbw_recovery_trace_text.find("stall ep=81") != std::string::npos,
                                  "invalid-cbw recovery roundtrip missing stall")) return 1;
        if (!usb::fixture::expect(invalid_cbw_recovery_trace_text.find("clear_stall ep=81") != std::string::npos,
                                  "invalid-cbw recovery roundtrip missing clear_stall")) return 1;
        if (!usb::fixture::expect(invalid_cbw_recovery_trace_text.find(kInvalidCsw) != std::string::npos,
                                  "invalid-cbw recovery roundtrip missing phase-error csw")) return 1;
        if (!usb::fixture::expect(invalid_cbw_recovery_trace_text.find(kRecoveryReadCapacity) != std::string::npos,
                                  "invalid-cbw recovery roundtrip missing read-capacity response")) return 1;

        const auto invalid_cbw_recovery_roundtrip = usb::replay::load_text(invalid_cbw_recovery_trace_text);
        if (!invalid_cbw_recovery_roundtrip) {
            std::fprintf(stderr,
                         "[ERR] invalid-cbw recovery roundtrip parse failed line=%zu err=%s\n",
                         invalid_cbw_recovery_roundtrip.line,
                         usb::replay::load_error_name(invalid_cbw_recovery_roundtrip.error));
            return 1;
        }

        MemoryDisk invalid_cbw_disk{};
        block::Registry<2> invalid_cbw_registry{};
        invalid_cbw_registry.init();
        auto invalid_cbw_reg = invalid_cbw_registry.register_device({"block.sd0", block::cap_id("block.sd0")}, invalid_cbw_disk.device);
        if (!invalid_cbw_reg) {
            std::fprintf(stderr, "[ERR] invalid-cbw registry register failed err=%d\n", static_cast<int>(invalid_cbw_reg.error()));
            return 1;
        }

        DemoContext invalid_cbw_demo{};
        usb::mock::Session invalid_cbw_session{};
        const auto invalid_cbw_spec = usb::spec::msc_device(make_device_spec(), make_msc_function(true));
        const auto invalid_cbw_model = usb::build(invalid_cbw_spec);
        const auto invalid_cbw_plan = usb::plan::build(invalid_cbw_model);
        if (!invalid_cbw_plan) {
            std::fprintf(stderr, "[ERR] invalid-cbw plan failed err=%d\n", static_cast<int>(invalid_cbw_plan.error()));
            return 1;
        }

        const auto invalid_cbw_runtime = usb::runtime::host_mock(
            invalid_cbw_session.dcd_ops(),
            &invalid_cbw_session,
            &invalid_cbw_session.adapter(),
            usb::runtime::MscReadyHook{&on_ready, &invalid_cbw_demo});

        auto invalid_cbw_binding = usb::runtime::make(invalid_cbw_plan.value(), invalid_cbw_registry, invalid_cbw_runtime);
        const auto invalid_cbw_init = decltype(invalid_cbw_binding)::init_trampoline(&invalid_cbw_binding);
        if (!invalid_cbw_init) {
            std::fprintf(stderr, "[ERR] invalid-cbw binding init failed err=%d\n", static_cast<int>(invalid_cbw_init.error()));
            return 1;
        }
        if (!usb::fixture::expect(invalid_cbw_demo.ready, "invalid-cbw runtime ready hook not called")) return 1;

        PumpContext invalid_cbw_pump{invalid_cbw_demo.bot, invalid_cbw_plan.value().msc.msc_cfg};
        const auto invalid_cbw_replay = usb::replay::run(
            invalid_cbw_session,
            invalid_cbw_recovery_roundtrip.trace,
            usb::replay::Hooks{&pump_in, &invalid_cbw_pump, &pump_stall});
        if (!invalid_cbw_replay) {
            std::fprintf(stderr,
                         "[ERR] invalid-cbw replay failed step=%zu err=%s\n",
                         invalid_cbw_replay.step_index,
                         usb::replay::error_name(invalid_cbw_replay.error));
            return 1;
        }

        const auto invalid_cbw_cfg = invalid_cbw_plan.value().msc.msc_cfg;
        if (!usb::fixture::expect(count_host_event(invalid_cbw_session.host_events(), usb::mock::HostEventKind::out_packet, invalid_cbw_cfg.ep_out) == 2,
                                  "invalid-cbw host out count mismatch")) return 1;
        if (!usb::fixture::expect(count_host_event(invalid_cbw_session.host_events(), usb::mock::HostEventKind::in_complete, invalid_cbw_cfg.ep_in) == 3,
                                  "invalid-cbw host in-complete count mismatch")) return 1;
        if (!usb::fixture::expect(has_host_event(invalid_cbw_session.host_events(), usb::mock::HostEventKind::clear_stall, invalid_cbw_cfg.ep_in),
                                  "invalid-cbw clear-stall host event missing")) return 1;
        if (!usb::fixture::expect(count_device_action(invalid_cbw_session.device_actions(), usb::mock::DeviceActionKind::stall_ep, invalid_cbw_cfg.ep_in) == 1,
                                  "invalid-cbw stall device action count mismatch")) return 1;
        if (!usb::fixture::expect(!invalid_cbw_session.endpoint_state(invalid_cbw_cfg.ep_in).stalled,
                                  "invalid-cbw bulk in endpoint should be cleared after recovery")) return 1;

        constexpr auto kReadCapacity = static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_capacity_10);
        const auto* invalid_cbw = find_msc_trace_event(invalid_cbw_demo.bot->trace_events(),
                                                       usb::class_driver::MscTraceEventKind::cbw_invalid,
                                                       kReadCapacity);
        if (!usb::fixture::expect(invalid_cbw != nullptr, "invalid-cbw recovery trace missing")) return 1;
        if (!usb::fixture::expect(invalid_cbw->transfer_length == 8, "invalid-cbw recovery transfer length mismatch")) return 1;
        if (!usb::fixture::expect(invalid_cbw->residue == 8, "invalid-cbw recovery residue mismatch")) return 1;
        if (!usb::fixture::expect(invalid_cbw->flag, "invalid-cbw recovery direction mismatch")) return 1;

        const auto* invalid_wait_csw = find_msc_trace_event(invalid_cbw_demo.bot->trace_events(),
                                                            usb::class_driver::MscTraceEventKind::wait_csw,
                                                            kReadCapacity);
        if (!usb::fixture::expect(invalid_wait_csw != nullptr, "invalid-cbw recovery wait-csw trace missing")) return 1;
        if (!usb::fixture::expect(invalid_wait_csw->transfer_length == 8, "invalid-cbw recovery wait-csw length mismatch")) return 1;

        const auto* invalid_phase_error = find_msc_trace_event(invalid_cbw_demo.bot->trace_events(),
                                                               usb::class_driver::MscTraceEventKind::phase_error,
                                                               kReadCapacity);
        if (!usb::fixture::expect(invalid_phase_error != nullptr, "invalid-cbw recovery phase-error trace missing")) return 1;
        if (!usb::fixture::expect(invalid_phase_error->residue == 8, "invalid-cbw recovery phase-error residue mismatch")) return 1;

        const auto* invalid_clear_stall = find_msc_trace_event(invalid_cbw_demo.bot->trace_events(),
                                                               usb::class_driver::MscTraceEventKind::clear_stall_seen,
                                                               kReadCapacity);
        if (!usb::fixture::expect(invalid_clear_stall != nullptr, "invalid-cbw recovery clear-stall trace missing")) return 1;
        if (!usb::fixture::expect(invalid_clear_stall->flag, "invalid-cbw recovery clear-stall endpoint mismatch")) return 1;

        const auto* invalid_csw_sent = find_msc_trace_event(invalid_cbw_demo.bot->trace_events(),
                                                            usb::class_driver::MscTraceEventKind::csw_sent,
                                                            kReadCapacity);
        if (!usb::fixture::expect(invalid_csw_sent != nullptr, "invalid-cbw recovery csw trace missing")) return 1;
        if (!usb::fixture::expect(invalid_csw_sent->residue == 8, "invalid-cbw recovery csw residue mismatch")) return 1;
        if (!usb::fixture::expect(invalid_csw_sent->flag, "invalid-cbw recovery csw phase flag mismatch")) return 1;

        const auto* recovery_read_capacity = find_msc_trace_event(invalid_cbw_demo.bot->trace_events(),
                                                                  usb::class_driver::MscTraceEventKind::read_capacity,
                                                                  kReadCapacity);
        if (!usb::fixture::expect(recovery_read_capacity != nullptr, "invalid-cbw recovery read-capacity trace missing")) return 1;
        if (!usb::fixture::expect(recovery_read_capacity->transfer_length == 8, "invalid-cbw recovery read-capacity transfer length mismatch")) return 1;
        if (!usb::fixture::expect(recovery_read_capacity->lba == 15, "invalid-cbw recovery read-capacity lba mismatch")) return 1;
        if (!usb::fixture::expect(recovery_read_capacity->blocks == 16, "invalid-cbw recovery read-capacity block count mismatch")) return 1;
    }
    {
        constexpr auto kRead10ShortCbw = "555342430B0000000002000080000A28000000000000000200000000000000";
        constexpr auto kRead10ShortCsw = "555342530B0000000002000000";

        MemoryDisk short_read_disk{};
        const auto imported_read10_short = usb::boardlog::load_file(fixture_path("read10_short.boardlog"));
        if (!imported_read10_short) {
            std::fprintf(stderr,
                         "[ERR] read10-short boardlog load failed line=%zu err=%s\n",
                         imported_read10_short.line,
                         usb::boardlog::error_name(imported_read10_short.error));
            return 1;
        }
        if (!usb::fixture::expect(imported_read10_short.imported_steps == 17, "read10-short imported step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_read10_short.skipped_steps == 0, "read10-short skipped step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_read10_short.trace.steps.size() == 9, "read10-short trace size mismatch")) return 1;
        if (!usb::fixture::expect(imported_read10_short.trace.steps[7].kind == usb::replay::StepKind::out,
                                  "read10-short cbw step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_read10_short.trace.steps[8].kind == usb::replay::StepKind::in,
                                  "read10-short data step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_read10_short.trace.steps[8].data.size() == (512 + 13),
                                  "read10-short in transaction length mismatch")) return 1;

        const auto read10_short_trace_text = usb::boardlog::to_text(imported_read10_short.trace);
        if (!usb::fixture::expect(count_substring(read10_short_trace_text, "out ep=01 zlp=0 data=") == 1,
                                  "read10-short roundtrip out count mismatch")) return 1;
        if (!usb::fixture::expect(count_substring(read10_short_trace_text, "in ep=81 zlp=0 data=") == 1,
                                  "read10-short roundtrip in count mismatch")) return 1;
        if (!usb::fixture::expect(read10_short_trace_text.find(kRead10ShortCsw) != std::string::npos,
                                  "read10-short roundtrip missing trailing csw")) return 1;

        const auto read10_short_roundtrip = usb::replay::load_text(read10_short_trace_text);
        if (!read10_short_roundtrip) {
            std::fprintf(stderr,
                         "[ERR] read10-short roundtrip parse failed line=%zu err=%s\n",
                         read10_short_roundtrip.line,
                         usb::replay::load_error_name(read10_short_roundtrip.error));
            return 1;
        }

        block::Registry<2> short_read_registry{};
        short_read_registry.init();
        auto short_read_reg = short_read_registry.register_device({"block.sd0", block::cap_id("block.sd0")}, short_read_disk.device);
        if (!short_read_reg) {
            std::fprintf(stderr, "[ERR] read10-short registry register failed err=%d\n", static_cast<int>(short_read_reg.error()));
            return 1;
        }

        DemoContext short_read_demo{};
        usb::mock::Session short_read_session{};
        const auto short_read_spec = usb::spec::msc_device(make_device_spec(), make_msc_function(false));
        const auto short_read_model = usb::build(short_read_spec);
        const auto short_read_plan = usb::plan::build(short_read_model);
        if (!short_read_plan) {
            std::fprintf(stderr, "[ERR] read10-short plan build failed err=%d\n", static_cast<int>(short_read_plan.error()));
            return 1;
        }

        const auto short_read_runtime = usb::runtime::host_mock(
            short_read_session.dcd_ops(),
            &short_read_session,
            &short_read_session.adapter(),
            usb::runtime::MscReadyHook{&on_ready, &short_read_demo});

        auto short_read_binding = usb::runtime::make(short_read_plan.value(), short_read_registry, short_read_runtime);
        const auto short_read_init = decltype(short_read_binding)::init_trampoline(&short_read_binding);
        if (!short_read_init) {
            std::fprintf(stderr, "[ERR] read10-short binding init failed err=%d\n", static_cast<int>(short_read_init.error()));
            return 1;
        }
        if (!usb::fixture::expect(short_read_demo.ready, "read10-short runtime ready hook not called")) return 1;

        PumpContext short_read_pump{short_read_demo.bot, short_read_plan.value().msc.msc_cfg};
        const auto short_read_replay = usb::replay::run(
            short_read_session,
            read10_short_roundtrip.trace,
            usb::replay::Hooks{&pump_in, &short_read_pump, &pump_stall});
        if (!short_read_replay) {
            std::fprintf(stderr,
                         "[ERR] read10-short replay failed step=%zu err=%s\n",
                         short_read_replay.step_index,
                         usb::replay::error_name(short_read_replay.error));
            return 1;
        }

        const auto short_read_cfg = short_read_plan.value().msc.msc_cfg;
        if (!usb::fixture::expect(count_host_event(short_read_session.host_events(), usb::mock::HostEventKind::out_packet, short_read_cfg.ep_out) == 1,
                                  "read10-short host out count mismatch")) return 1;
        if (!usb::fixture::expect(count_host_event(short_read_session.host_events(), usb::mock::HostEventKind::in_complete, short_read_cfg.ep_in) == 9,
                                  "read10-short host in-complete count mismatch")) return 1;
        if (!usb::fixture::expect(!has_host_event(short_read_session.host_events(), usb::mock::HostEventKind::clear_stall, short_read_cfg.ep_in),
                                  "read10-short should not clear stall")) return 1;
        if (!usb::fixture::expect(count_device_action(short_read_session.device_actions(), usb::mock::DeviceActionKind::stall_ep, short_read_cfg.ep_in) == 0,
                                  "read10-short should not stall bulk in")) return 1;

        constexpr auto kRead10Short = static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_10);
        const auto* short_read_trace = find_msc_trace_event(short_read_demo.bot->trace_events(),
                                                            usb::class_driver::MscTraceEventKind::read10_started,
                                                            kRead10Short);
        if (!usb::fixture::expect(short_read_trace != nullptr, "read10-short trace missing")) return 1;
        if (!usb::fixture::expect(short_read_trace->transfer_length == 512, "read10-short transfer length mismatch")) return 1;
        if (!usb::fixture::expect(short_read_trace->lba == 0, "read10-short lba mismatch")) return 1;
        if (!usb::fixture::expect(short_read_trace->blocks == 2, "read10-short block count mismatch")) return 1;

        const auto* short_data_in = find_msc_trace_event(short_read_demo.bot->trace_events(),
                                                         usb::class_driver::MscTraceEventKind::data_in_started,
                                                         kRead10Short);
        if (!usb::fixture::expect(short_data_in != nullptr, "read10-short data-in trace missing")) return 1;
        if (!usb::fixture::expect(short_data_in->transfer_length == 512, "read10-short data-in length mismatch")) return 1;
        if (!usb::fixture::expect(short_data_in->lba == 0, "read10-short data-in lba mismatch")) return 1;
        if (!usb::fixture::expect(short_data_in->blocks == 2, "read10-short data-in block count mismatch")) return 1;
        if (!usb::fixture::expect(short_data_in->residue == 512, "read10-short data-in residue mismatch")) return 1;

        const auto* short_csw_ready = find_msc_trace_event(short_read_demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::csw_ready,
                                                           kRead10Short);
        if (!usb::fixture::expect(short_csw_ready != nullptr, "read10-short csw-ready trace missing")) return 1;
        if (!usb::fixture::expect(short_csw_ready->residue == 512, "read10-short csw-ready residue mismatch")) return 1;
        if (!usb::fixture::expect(!short_csw_ready->flag, "read10-short csw-ready phase flag mismatch")) return 1;

        const auto* short_csw_sent = find_msc_trace_event(short_read_demo.bot->trace_events(),
                                                          usb::class_driver::MscTraceEventKind::csw_sent,
                                                          kRead10Short);
        if (!usb::fixture::expect(short_csw_sent != nullptr, "read10-short csw trace missing")) return 1;
        if (!usb::fixture::expect(short_csw_sent->residue == 512, "read10-short csw residue mismatch")) return 1;
        if (!usb::fixture::expect(!short_csw_sent->flag, "read10-short csw phase flag mismatch")) return 1;
    }
    {
        constexpr auto kWrite10ReadOnlyCbw = "55534243060000000002000000000A2A000000000000000100000000000000";
        constexpr auto kWrite10ReadOnlyCsw = "55534253060000000000000001";
        constexpr auto kRequestSenseCbw = "55534243070000001200000080000603000000120000000000000000000000";
        constexpr auto kRequestSenseResponse = "700007000000000A0000000027000000000055534253070000000000000000";

        const auto imported_request_sense = usb::boardlog::load_file(fixture_path("request_sense.boardlog"));
        if (!imported_request_sense) {
            std::fprintf(stderr,
                         "[ERR] request-sense boardlog load failed line=%zu err=%s\n",
                         imported_request_sense.line,
                         usb::boardlog::error_name(imported_request_sense.error));
            return 1;
        }
        if (!usb::fixture::expect(imported_request_sense.imported_steps == 11, "request-sense imported step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_request_sense.skipped_steps == 0, "request-sense skipped step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_request_sense.trace.steps.size() == 11, "request-sense trace size mismatch")) return 1;
        if (!usb::fixture::expect(imported_request_sense.trace.steps[7].kind == usb::replay::StepKind::out,
                                  "request-sense write10 step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_request_sense.trace.steps[8].kind == usb::replay::StepKind::in,
                                  "request-sense failed csw step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_request_sense.trace.steps[9].kind == usb::replay::StepKind::out,
                                  "request-sense request-cbw step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_request_sense.trace.steps[10].kind == usb::replay::StepKind::in,
                                  "request-sense response step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_request_sense.trace.steps[10].data.size() == 31,
                                  "request-sense response transaction length mismatch")) return 1;

        const auto request_sense_trace_text = usb::boardlog::to_text(imported_request_sense.trace);
        if (!usb::fixture::expect(count_substring(request_sense_trace_text, "out ep=01 zlp=0 data=") == 2,
                                  "request-sense roundtrip out count mismatch")) return 1;
        if (!usb::fixture::expect(count_substring(request_sense_trace_text, "in ep=81 zlp=0 data=") == 2,
                                  "request-sense roundtrip in count mismatch")) return 1;
        if (!usb::fixture::expect(request_sense_trace_text.find(kWrite10ReadOnlyCsw) != std::string::npos,
                                  "request-sense roundtrip missing failed csw")) return 1;
        if (!usb::fixture::expect(request_sense_trace_text.find(kRequestSenseResponse) != std::string::npos,
                                  "request-sense roundtrip missing sense response")) return 1;

        const auto request_sense_roundtrip = usb::replay::load_text(request_sense_trace_text);
        if (!request_sense_roundtrip) {
            std::fprintf(stderr,
                         "[ERR] request-sense roundtrip parse failed line=%zu err=%s\n",
                         request_sense_roundtrip.line,
                         usb::replay::load_error_name(request_sense_roundtrip.error));
            return 1;
        }

        MemoryDisk request_sense_disk{};
        block::Registry<2> request_sense_registry{};
        request_sense_registry.init();
        auto request_sense_reg = request_sense_registry.register_device({"block.sd0", block::cap_id("block.sd0")}, request_sense_disk.device);
        if (!request_sense_reg) {
            std::fprintf(stderr, "[ERR] request-sense registry register failed err=%d\n", static_cast<int>(request_sense_reg.error()));
            return 1;
        }

        DemoContext request_sense_demo{};
        usb::mock::Session request_sense_session{};
        const auto request_sense_spec = usb::spec::msc_device(make_device_spec(), make_msc_function(true));
        const auto request_sense_model = usb::build(request_sense_spec);
        const auto request_sense_plan = usb::plan::build(request_sense_model);
        if (!request_sense_plan) {
            std::fprintf(stderr, "[ERR] request-sense plan build failed err=%d\n", static_cast<int>(request_sense_plan.error()));
            return 1;
        }

        const auto request_sense_runtime = usb::runtime::host_mock(
            request_sense_session.dcd_ops(),
            &request_sense_session,
            &request_sense_session.adapter(),
            usb::runtime::MscReadyHook{&on_ready, &request_sense_demo});

        auto request_sense_binding = usb::runtime::make(request_sense_plan.value(), request_sense_registry, request_sense_runtime);
        const auto request_sense_init = decltype(request_sense_binding)::init_trampoline(&request_sense_binding);
        if (!request_sense_init) {
            std::fprintf(stderr, "[ERR] request-sense binding init failed err=%d\n", static_cast<int>(request_sense_init.error()));
            return 1;
        }
        if (!usb::fixture::expect(request_sense_demo.ready, "request-sense runtime ready hook not called")) return 1;

        PumpContext request_sense_pump{request_sense_demo.bot, request_sense_plan.value().msc.msc_cfg};
        const auto request_sense_replay = usb::replay::run(
            request_sense_session,
            request_sense_roundtrip.trace,
            usb::replay::Hooks{&pump_in, &request_sense_pump, &pump_stall});
        if (!request_sense_replay) {
            std::fprintf(stderr,
                         "[ERR] request-sense replay failed step=%zu err=%s\n",
                         request_sense_replay.step_index,
                         usb::replay::error_name(request_sense_replay.error));
            return 1;
        }

        const auto request_sense_cfg = request_sense_plan.value().msc.msc_cfg;
        if (!usb::fixture::expect(count_host_event(request_sense_session.host_events(), usb::mock::HostEventKind::out_packet, request_sense_cfg.ep_out) == 2,
                                  "request-sense host out count mismatch")) return 1;
        if (!usb::fixture::expect(count_host_event(request_sense_session.host_events(), usb::mock::HostEventKind::in_complete, request_sense_cfg.ep_in) == 3,
                                  "request-sense host in-complete count mismatch")) return 1;
        if (!usb::fixture::expect(!has_host_event(request_sense_session.host_events(), usb::mock::HostEventKind::clear_stall, request_sense_cfg.ep_in),
                                  "request-sense should not clear stall")) return 1;
        if (!usb::fixture::expect(count_device_action(request_sense_session.device_actions(), usb::mock::DeviceActionKind::stall_ep, request_sense_cfg.ep_in) == 0,
                                  "request-sense should not stall bulk in")) return 1;
        if (!usb::fixture::expect(count_device_action(request_sense_session.device_actions(), usb::mock::DeviceActionKind::stall_ep, request_sense_cfg.ep_out) == 0,
                                  "request-sense should not stall bulk out")) return 1;

        constexpr auto kWrite10ReadOnly = static_cast<usb::u8>(usb::class_driver::ScsiCmd::write_10);
        const auto* request_write10 = find_msc_trace_event(request_sense_demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::write10_started,
                                                           kWrite10ReadOnly);
        if (!usb::fixture::expect(request_write10 != nullptr, "request-sense write10 trace missing")) return 1;
        if (!usb::fixture::expect(request_write10->transfer_length == 512, "request-sense write10 length mismatch")) return 1;
        if (!usb::fixture::expect(request_write10->lba == 0, "request-sense write10 lba mismatch")) return 1;
        if (!usb::fixture::expect(request_write10->blocks == 1, "request-sense write10 block count mismatch")) return 1;

        const auto* request_sense_set = find_msc_trace_event(request_sense_demo.bot->trace_events(),
                                                             usb::class_driver::MscTraceEventKind::sense_set,
                                                             kWrite10ReadOnly);
        if (!usb::fixture::expect(request_sense_set != nullptr, "request-sense sense-set trace missing")) return 1;
        if (!usb::fixture::expect(request_sense_set->sense_key == 0x07, "request-sense sense key mismatch")) return 1;
        if (!usb::fixture::expect(request_sense_set->sense_asc == 0x27, "request-sense sense asc mismatch")) return 1;
        if (!usb::fixture::expect(request_sense_set->sense_ascq == 0x00, "request-sense sense ascq mismatch")) return 1;
        if (!usb::fixture::expect(request_sense_set->transfer_length == 512, "request-sense sense transfer length mismatch")) return 1;

        const auto* request_write10_csw = find_msc_trace_event(request_sense_demo.bot->trace_events(),
                                                               usb::class_driver::MscTraceEventKind::csw_sent,
                                                               kWrite10ReadOnly);
        if (!usb::fixture::expect(request_write10_csw != nullptr, "request-sense write10 csw trace missing")) return 1;
        if (!usb::fixture::expect(request_write10_csw->residue == 0, "request-sense write10 csw residue mismatch")) return 1;
        if (!usb::fixture::expect(!request_write10_csw->flag, "request-sense write10 csw phase flag mismatch")) return 1;

        if (!usb::fixture::expect(find_msc_trace_event(request_sense_demo.bot->trace_events(),
                                                       usb::class_driver::MscTraceEventKind::wait_csw,
                                                       kWrite10ReadOnly) == nullptr,
                                  "request-sense write10 should not wait csw")) return 1;

        constexpr auto kRequestSense = static_cast<usb::u8>(usb::class_driver::ScsiCmd::request_sense);
        const auto* request_sense_csw_ready = find_msc_trace_event(request_sense_demo.bot->trace_events(),
                                                                   usb::class_driver::MscTraceEventKind::csw_ready,
                                                                   kRequestSense);
        if (!usb::fixture::expect(request_sense_csw_ready != nullptr, "request-sense csw-ready trace missing")) return 1;
        if (!usb::fixture::expect(request_sense_csw_ready->residue == 0, "request-sense csw-ready residue mismatch")) return 1;
        if (!usb::fixture::expect(!request_sense_csw_ready->flag, "request-sense csw-ready phase flag mismatch")) return 1;

        const auto* request_sense_csw_sent = find_msc_trace_event(request_sense_demo.bot->trace_events(),
                                                                  usb::class_driver::MscTraceEventKind::csw_sent,
                                                                  kRequestSense);
        if (!usb::fixture::expect(request_sense_csw_sent != nullptr, "request-sense csw trace missing")) return 1;
        if (!usb::fixture::expect(request_sense_csw_sent->residue == 0, "request-sense csw residue mismatch")) return 1;
        if (!usb::fixture::expect(!request_sense_csw_sent->flag, "request-sense csw phase flag mismatch")) return 1;
    }
    {
        constexpr auto kReadCapacityCbw = "555342430A0000000A00000080000A25000000000000000000000000000000";
        constexpr auto kReadCapacityResponse = "0000000F00000200555342530A0000000200000000";

        const auto imported_read_capacity = usb::boardlog::load_file(fixture_path("read_capacity_residue.boardlog"));
        if (!imported_read_capacity) {
            std::fprintf(stderr,
                         "[ERR] read-capacity boardlog load failed line=%zu err=%s\n",
                         imported_read_capacity.line,
                         usb::boardlog::error_name(imported_read_capacity.error));
            return 1;
        }
        if (!usb::fixture::expect(imported_read_capacity.imported_steps == 9, "read-capacity imported step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_read_capacity.skipped_steps == 0, "read-capacity skipped step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_read_capacity.trace.steps.size() == 9, "read-capacity trace size mismatch")) return 1;
        if (!usb::fixture::expect(imported_read_capacity.trace.steps[7].kind == usb::replay::StepKind::out,
                                  "read-capacity cbw step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_read_capacity.trace.steps[8].kind == usb::replay::StepKind::in,
                                  "read-capacity response step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_read_capacity.trace.steps[8].data.size() == 21,
                                  "read-capacity response transaction length mismatch")) return 1;

        const auto read_capacity_trace_text = usb::boardlog::to_text(imported_read_capacity.trace);
        if (!usb::fixture::expect(count_substring(read_capacity_trace_text, "out ep=01 zlp=0 data=") == 1,
                                  "read-capacity roundtrip out count mismatch")) return 1;
        if (!usb::fixture::expect(count_substring(read_capacity_trace_text, "in ep=81 zlp=0 data=") == 1,
                                  "read-capacity roundtrip in count mismatch")) return 1;
        if (!usb::fixture::expect(read_capacity_trace_text.find(kReadCapacityResponse) != std::string::npos,
                                  "read-capacity roundtrip missing response")) return 1;

        const auto read_capacity_roundtrip = usb::replay::load_text(read_capacity_trace_text);
        if (!read_capacity_roundtrip) {
            std::fprintf(stderr,
                         "[ERR] read-capacity roundtrip parse failed line=%zu err=%s\n",
                         read_capacity_roundtrip.line,
                         usb::replay::load_error_name(read_capacity_roundtrip.error));
            return 1;
        }

        MemoryDisk read_capacity_disk{};
        block::Registry<2> read_capacity_registry{};
        read_capacity_registry.init();
        auto read_capacity_reg = read_capacity_registry.register_device({"block.sd0", block::cap_id("block.sd0")}, read_capacity_disk.device);
        if (!read_capacity_reg) {
            std::fprintf(stderr, "[ERR] read-capacity registry register failed err=%d\n", static_cast<int>(read_capacity_reg.error()));
            return 1;
        }

        DemoContext read_capacity_demo{};
        usb::mock::Session read_capacity_session{};
        const auto read_capacity_spec = usb::spec::msc_device(make_device_spec(), make_msc_function(false));
        const auto read_capacity_model = usb::build(read_capacity_spec);
        const auto read_capacity_plan = usb::plan::build(read_capacity_model);
        if (!read_capacity_plan) {
            std::fprintf(stderr, "[ERR] read-capacity plan build failed err=%d\n", static_cast<int>(read_capacity_plan.error()));
            return 1;
        }

        const auto read_capacity_runtime = usb::runtime::host_mock(
            read_capacity_session.dcd_ops(),
            &read_capacity_session,
            &read_capacity_session.adapter(),
            usb::runtime::MscReadyHook{&on_ready, &read_capacity_demo});

        auto read_capacity_binding = usb::runtime::make(read_capacity_plan.value(), read_capacity_registry, read_capacity_runtime);
        const auto read_capacity_init = decltype(read_capacity_binding)::init_trampoline(&read_capacity_binding);
        if (!read_capacity_init) {
            std::fprintf(stderr, "[ERR] read-capacity binding init failed err=%d\n", static_cast<int>(read_capacity_init.error()));
            return 1;
        }
        if (!usb::fixture::expect(read_capacity_demo.ready, "read-capacity runtime ready hook not called")) return 1;

        PumpContext read_capacity_pump{read_capacity_demo.bot, read_capacity_plan.value().msc.msc_cfg};
        const auto read_capacity_replay = usb::replay::run(
            read_capacity_session,
            read_capacity_roundtrip.trace,
            usb::replay::Hooks{&pump_in, &read_capacity_pump, &pump_stall});
        if (!read_capacity_replay) {
            std::fprintf(stderr,
                         "[ERR] read-capacity replay failed step=%zu err=%s\n",
                         read_capacity_replay.step_index,
                         usb::replay::error_name(read_capacity_replay.error));
            return 1;
        }

        const auto read_capacity_cfg = read_capacity_plan.value().msc.msc_cfg;
        if (!usb::fixture::expect(count_host_event(read_capacity_session.host_events(), usb::mock::HostEventKind::out_packet, read_capacity_cfg.ep_out) == 1,
                                  "read-capacity host out count mismatch")) return 1;
        if (!usb::fixture::expect(count_host_event(read_capacity_session.host_events(), usb::mock::HostEventKind::in_complete, read_capacity_cfg.ep_in) == 2,
                                  "read-capacity host in-complete count mismatch")) return 1;
        if (!usb::fixture::expect(!has_host_event(read_capacity_session.host_events(), usb::mock::HostEventKind::clear_stall, read_capacity_cfg.ep_in),
                                  "read-capacity should not clear stall")) return 1;
        if (!usb::fixture::expect(count_device_action(read_capacity_session.device_actions(), usb::mock::DeviceActionKind::stall_ep, read_capacity_cfg.ep_in) == 0,
                                  "read-capacity should not stall bulk in")) return 1;

        constexpr auto kReadCapacity = static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_capacity_10);
        const auto* read_capacity_trace = find_msc_trace_event(read_capacity_demo.bot->trace_events(),
                                                               usb::class_driver::MscTraceEventKind::read_capacity,
                                                               kReadCapacity);
        if (!usb::fixture::expect(read_capacity_trace != nullptr, "read-capacity trace missing")) return 1;
        if (!usb::fixture::expect(read_capacity_trace->transfer_length == 10, "read-capacity transfer length mismatch")) return 1;
        if (!usb::fixture::expect(read_capacity_trace->lba == 15, "read-capacity lba mismatch")) return 1;
        if (!usb::fixture::expect(read_capacity_trace->blocks == 16, "read-capacity block count mismatch")) return 1;

        const auto* read_capacity_csw_ready = find_msc_trace_event(read_capacity_demo.bot->trace_events(),
                                                                   usb::class_driver::MscTraceEventKind::csw_ready,
                                                                   kReadCapacity);
        if (!usb::fixture::expect(read_capacity_csw_ready != nullptr, "read-capacity csw-ready trace missing")) return 1;
        if (!usb::fixture::expect(read_capacity_csw_ready->residue == 2, "read-capacity csw-ready residue mismatch")) return 1;
        if (!usb::fixture::expect(!read_capacity_csw_ready->flag, "read-capacity csw-ready phase flag mismatch")) return 1;

        const auto* read_capacity_csw_sent = find_msc_trace_event(read_capacity_demo.bot->trace_events(),
                                                                  usb::class_driver::MscTraceEventKind::csw_sent,
                                                                  kReadCapacity);
        if (!usb::fixture::expect(read_capacity_csw_sent != nullptr, "read-capacity csw trace missing")) return 1;
        if (!usb::fixture::expect(read_capacity_csw_sent->residue == 2, "read-capacity csw residue mismatch")) return 1;
        if (!usb::fixture::expect(!read_capacity_csw_sent->flag, "read-capacity csw phase flag mismatch")) return 1;
    }
    {
        constexpr auto kRead10ZeroLenCbw = "55534243080000000000000080000A28000000000000000100000000000000";
        constexpr auto kRead10ZeroLenCsw = "55534253080000000000000002";
        constexpr auto kRead10ZeroLenRequestSenseCbw = "55534243090000001200000080000603000000120000000000000000000000";
        constexpr auto kRead10ZeroLenRequestSenseResp = "700005000000000A0000000020000000000055534253090000000000000000";

        const auto imported_zero_len = usb::boardlog::load_file(fixture_path("read10_zero_len_recovery.boardlog"));
        if (!imported_zero_len) {
            std::fprintf(stderr,
                         "[ERR] read10-zero-len boardlog load failed line=%zu err=%s\n",
                         imported_zero_len.line,
                         usb::boardlog::error_name(imported_zero_len.error));
            return 1;
        }
        if (!usb::fixture::expect(imported_zero_len.imported_steps == 13, "read10-zero-len imported step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_zero_len.skipped_steps == 0, "read10-zero-len skipped step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_zero_len.trace.steps.size() == 13, "read10-zero-len trace size mismatch")) return 1;
        if (!usb::fixture::expect(imported_zero_len.trace.steps[7].kind == usb::replay::StepKind::out,
                                  "read10-zero-len cbw step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_zero_len.trace.steps[8].kind == usb::replay::StepKind::stall,
                                  "read10-zero-len stall step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_zero_len.trace.steps[8].ep == 0x81,
                                  "read10-zero-len stall endpoint mismatch")) return 1;
        if (!usb::fixture::expect(imported_zero_len.trace.steps[9].kind == usb::replay::StepKind::clear_stall,
                                  "read10-zero-len clear-stall step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_zero_len.trace.steps[10].kind == usb::replay::StepKind::in,
                                  "read10-zero-len csw step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_zero_len.trace.steps[11].kind == usb::replay::StepKind::out,
                                  "read10-zero-len request-sense step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_zero_len.trace.steps[12].kind == usb::replay::StepKind::in,
                                  "read10-zero-len request-sense response step kind mismatch")) return 1;

        const auto zero_len_trace_text = usb::boardlog::to_text(imported_zero_len.trace);
        if (!usb::fixture::expect(count_substring(zero_len_trace_text, "out ep=01 zlp=0 data=") == 2,
                                  "read10-zero-len roundtrip out count mismatch")) return 1;
        if (!usb::fixture::expect(count_substring(zero_len_trace_text, "in ep=81 zlp=0 data=") == 2,
                                  "read10-zero-len roundtrip in count mismatch")) return 1;
        if (!usb::fixture::expect(zero_len_trace_text.find("stall ep=81") != std::string::npos,
                                  "read10-zero-len roundtrip missing stall")) return 1;
        if (!usb::fixture::expect(zero_len_trace_text.find("clear_stall ep=81") != std::string::npos,
                                  "read10-zero-len roundtrip missing clear_stall")) return 1;
        if (!usb::fixture::expect(zero_len_trace_text.find(kRead10ZeroLenRequestSenseResp) != std::string::npos,
                                  "read10-zero-len roundtrip missing request-sense response")) return 1;

        const auto zero_len_roundtrip = usb::replay::load_text(zero_len_trace_text);
        if (!zero_len_roundtrip) {
            std::fprintf(stderr,
                         "[ERR] read10-zero-len roundtrip parse failed line=%zu err=%s\n",
                         zero_len_roundtrip.line,
                         usb::replay::load_error_name(zero_len_roundtrip.error));
            return 1;
        }

        MemoryDisk zero_len_disk{};
        block::Registry<2> zero_len_registry{};
        zero_len_registry.init();
        auto zero_len_reg = zero_len_registry.register_device({"block.sd0", block::cap_id("block.sd0")}, zero_len_disk.device);
        if (!zero_len_reg) {
            std::fprintf(stderr, "[ERR] read10-zero-len registry register failed err=%d\n", static_cast<int>(zero_len_reg.error()));
            return 1;
        }

        DemoContext zero_len_demo{};
        usb::mock::Session zero_len_session{};
        const auto zero_len_spec = usb::spec::msc_device(make_device_spec(), make_msc_function(false));
        const auto zero_len_model = usb::build(zero_len_spec);
        const auto zero_len_plan = usb::plan::build(zero_len_model);
        if (!zero_len_plan) {
            std::fprintf(stderr, "[ERR] read10-zero-len plan build failed err=%d\n", static_cast<int>(zero_len_plan.error()));
            return 1;
        }

        const auto zero_len_runtime = usb::runtime::host_mock(
            zero_len_session.dcd_ops(),
            &zero_len_session,
            &zero_len_session.adapter(),
            usb::runtime::MscReadyHook{&on_ready, &zero_len_demo});

        auto zero_len_binding = usb::runtime::make(zero_len_plan.value(), zero_len_registry, zero_len_runtime);
        const auto zero_len_init = decltype(zero_len_binding)::init_trampoline(&zero_len_binding);
        if (!zero_len_init) {
            std::fprintf(stderr, "[ERR] read10-zero-len binding init failed err=%d\n", static_cast<int>(zero_len_init.error()));
            return 1;
        }
        if (!usb::fixture::expect(zero_len_demo.ready, "read10-zero-len runtime ready hook not called")) return 1;

        PumpContext zero_len_pump{zero_len_demo.bot, zero_len_plan.value().msc.msc_cfg};
        const auto zero_len_replay = usb::replay::run(
            zero_len_session,
            zero_len_roundtrip.trace,
            usb::replay::Hooks{&pump_in, &zero_len_pump, &pump_stall});
        if (!zero_len_replay) {
            std::fprintf(stderr,
                         "[ERR] read10-zero-len replay failed step=%zu err=%s\n",
                         zero_len_replay.step_index,
                         usb::replay::error_name(zero_len_replay.error));
            return 1;
        }

        const auto zero_len_cfg = zero_len_plan.value().msc.msc_cfg;
        if (!usb::fixture::expect(count_host_event(zero_len_session.host_events(), usb::mock::HostEventKind::out_packet, zero_len_cfg.ep_out) == 2,
                                  "read10-zero-len host out count mismatch")) return 1;
        if (!usb::fixture::expect(count_host_event(zero_len_session.host_events(), usb::mock::HostEventKind::in_complete, zero_len_cfg.ep_in) == 3,
                                  "read10-zero-len host in-complete count mismatch")) return 1;
        if (!usb::fixture::expect(has_host_event(zero_len_session.host_events(), usb::mock::HostEventKind::clear_stall, zero_len_cfg.ep_in),
                                  "read10-zero-len clear-stall host event missing")) return 1;
        if (!usb::fixture::expect(count_device_action(zero_len_session.device_actions(), usb::mock::DeviceActionKind::stall_ep, zero_len_cfg.ep_in) == 1,
                                  "read10-zero-len stall device action count mismatch")) return 1;
        if (!usb::fixture::expect(!zero_len_session.endpoint_state(zero_len_cfg.ep_in).stalled,
                                  "read10-zero-len bulk in endpoint should be cleared after recovery")) return 1;

        constexpr auto kRead10ZeroLen = static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_10);
        const auto* zero_len_read10 = find_msc_trace_event(zero_len_demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::read10_started,
                                                           kRead10ZeroLen);
        if (!usb::fixture::expect(zero_len_read10 != nullptr, "read10-zero-len read10 trace missing")) return 1;
        if (!usb::fixture::expect(zero_len_read10->transfer_length == 0, "read10-zero-len transfer length mismatch")) return 1;
        if (!usb::fixture::expect(zero_len_read10->lba == 0, "read10-zero-len lba mismatch")) return 1;
        if (!usb::fixture::expect(zero_len_read10->blocks == 1, "read10-zero-len block count mismatch")) return 1;

        const auto* zero_len_sense = find_msc_trace_event(zero_len_demo.bot->trace_events(),
                                                          usb::class_driver::MscTraceEventKind::sense_set,
                                                          kRead10ZeroLen);
        if (!usb::fixture::expect(zero_len_sense != nullptr, "read10-zero-len sense trace missing")) return 1;
        if (!usb::fixture::expect(zero_len_sense->sense_key == 0x05, "read10-zero-len sense key mismatch")) return 1;
        if (!usb::fixture::expect(zero_len_sense->sense_asc == 0x20, "read10-zero-len sense asc mismatch")) return 1;
        if (!usb::fixture::expect(zero_len_sense->sense_ascq == 0x00, "read10-zero-len sense ascq mismatch")) return 1;
        if (!usb::fixture::expect(zero_len_sense->transfer_length == 0, "read10-zero-len sense transfer length mismatch")) return 1;

        const auto* zero_len_stall_in = find_msc_trace_event(zero_len_demo.bot->trace_events(),
                                                             usb::class_driver::MscTraceEventKind::stall_in_requested,
                                                             kRead10ZeroLen);
        if (!usb::fixture::expect(zero_len_stall_in != nullptr, "read10-zero-len stall-in trace missing")) return 1;
        if (!usb::fixture::expect(zero_len_stall_in->residue == 0, "read10-zero-len stall-in residue mismatch")) return 1;
        if (!usb::fixture::expect(zero_len_stall_in->flag, "read10-zero-len stall-in flag mismatch")) return 1;

        const auto* zero_len_wait_csw = find_msc_trace_event(zero_len_demo.bot->trace_events(),
                                                             usb::class_driver::MscTraceEventKind::wait_csw,
                                                             kRead10ZeroLen);
        if (!usb::fixture::expect(zero_len_wait_csw != nullptr, "read10-zero-len wait-csw trace missing")) return 1;
        if (!usb::fixture::expect(zero_len_wait_csw->transfer_length == 0, "read10-zero-len wait-csw length mismatch")) return 1;

        const auto* zero_len_phase_error = find_msc_trace_event(zero_len_demo.bot->trace_events(),
                                                                usb::class_driver::MscTraceEventKind::phase_error,
                                                                kRead10ZeroLen);
        if (!usb::fixture::expect(zero_len_phase_error != nullptr, "read10-zero-len phase-error trace missing")) return 1;
        if (!usb::fixture::expect(zero_len_phase_error->residue == 0, "read10-zero-len phase-error residue mismatch")) return 1;

        const auto* zero_len_clear_stall = find_msc_trace_event(zero_len_demo.bot->trace_events(),
                                                                usb::class_driver::MscTraceEventKind::clear_stall_seen,
                                                                kRead10ZeroLen);
        if (!usb::fixture::expect(zero_len_clear_stall != nullptr, "read10-zero-len clear-stall trace missing")) return 1;
        if (!usb::fixture::expect(zero_len_clear_stall->flag, "read10-zero-len clear-stall flag mismatch")) return 1;

        const auto* zero_len_csw_sent = find_msc_trace_event(zero_len_demo.bot->trace_events(),
                                                             usb::class_driver::MscTraceEventKind::csw_sent,
                                                             kRead10ZeroLen);
        if (!usb::fixture::expect(zero_len_csw_sent != nullptr, "read10-zero-len csw trace missing")) return 1;
        if (!usb::fixture::expect(zero_len_csw_sent->residue == 0, "read10-zero-len csw residue mismatch")) return 1;
        if (!usb::fixture::expect(zero_len_csw_sent->flag, "read10-zero-len csw phase flag mismatch")) return 1;

        constexpr auto kRead10ZeroLenRequestSense = static_cast<usb::u8>(usb::class_driver::ScsiCmd::request_sense);
        const auto* zero_len_request_sense_csw = find_msc_trace_event(zero_len_demo.bot->trace_events(),
                                                                      usb::class_driver::MscTraceEventKind::csw_sent,
                                                                      kRead10ZeroLenRequestSense);
        if (!usb::fixture::expect(zero_len_request_sense_csw != nullptr, "read10-zero-len request-sense csw trace missing")) return 1;
        if (!usb::fixture::expect(zero_len_request_sense_csw->residue == 0, "read10-zero-len request-sense csw residue mismatch")) return 1;
        if (!usb::fixture::expect(!zero_len_request_sense_csw->flag, "read10-zero-len request-sense csw phase flag mismatch")) return 1;
    }
    {
        constexpr auto kRead10OverrunCbw = "555342430C0000000004000080000A28000000000000000100000000000000";
        constexpr auto kRead10OverrunCsw = "555342530C0000000002000002";

        MemoryDisk overrun_disk{};
        const auto imported_overrun = usb::boardlog::load_file(fixture_path("read10_overrun_recovery.boardlog"));
        if (!imported_overrun) {
            std::fprintf(stderr,
                         "[ERR] read10-overrun boardlog load failed line=%zu err=%s\n",
                         imported_overrun.line,
                         usb::boardlog::error_name(imported_overrun.error));
            return 1;
        }
        if (!usb::fixture::expect(imported_overrun.imported_steps == 19, "read10-overrun imported step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_overrun.skipped_steps == 0, "read10-overrun skipped step count mismatch")) return 1;
        if (!usb::fixture::expect(imported_overrun.trace.steps.size() == 11, "read10-overrun trace size mismatch")) return 1;
        if (!usb::fixture::expect(imported_overrun.trace.steps[7].kind == usb::replay::StepKind::out,
                                  "read10-overrun cbw step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_overrun.trace.steps[8].kind == usb::replay::StepKind::stall,
                                  "read10-overrun stall step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_overrun.trace.steps[8].ep == 0x81,
                                  "read10-overrun stall endpoint mismatch")) return 1;
        if (!usb::fixture::expect(imported_overrun.trace.steps[9].kind == usb::replay::StepKind::clear_stall,
                                  "read10-overrun clear-stall step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_overrun.trace.steps[10].kind == usb::replay::StepKind::in,
                                  "read10-overrun response step kind mismatch")) return 1;
        if (!usb::fixture::expect(imported_overrun.trace.steps[10].data.size() == (512 + 13),
                                  "read10-overrun response transaction length mismatch")) return 1;

        const auto overrun_trace_text = usb::boardlog::to_text(imported_overrun.trace);
        if (!usb::fixture::expect(count_substring(overrun_trace_text, "out ep=01 zlp=0 data=") == 1,
                                  "read10-overrun roundtrip out count mismatch")) return 1;
        if (!usb::fixture::expect(count_substring(overrun_trace_text, "in ep=81 zlp=0 data=") == 1,
                                  "read10-overrun roundtrip in count mismatch")) return 1;
        if (!usb::fixture::expect(overrun_trace_text.find("stall ep=81") != std::string::npos,
                                  "read10-overrun roundtrip missing stall")) return 1;
        if (!usb::fixture::expect(overrun_trace_text.find("clear_stall ep=81") != std::string::npos,
                                  "read10-overrun roundtrip missing clear_stall")) return 1;
        if (!usb::fixture::expect(overrun_trace_text.find(kRead10OverrunCsw) != std::string::npos,
                                  "read10-overrun roundtrip missing phase-error csw")) return 1;

        const auto overrun_roundtrip = usb::replay::load_text(overrun_trace_text);
        if (!overrun_roundtrip) {
            std::fprintf(stderr,
                         "[ERR] read10-overrun roundtrip parse failed line=%zu err=%s\n",
                         overrun_roundtrip.line,
                         usb::replay::load_error_name(overrun_roundtrip.error));
            return 1;
        }

        block::Registry<2> overrun_registry{};
        overrun_registry.init();
        auto overrun_reg = overrun_registry.register_device({"block.sd0", block::cap_id("block.sd0")}, overrun_disk.device);
        if (!overrun_reg) {
            std::fprintf(stderr, "[ERR] read10-overrun registry register failed err=%d\n", static_cast<int>(overrun_reg.error()));
            return 1;
        }

        DemoContext overrun_demo{};
        usb::mock::Session overrun_session{};
        const auto overrun_spec = usb::spec::msc_device(make_device_spec(), make_msc_function(false));
        const auto overrun_model = usb::build(overrun_spec);
        const auto overrun_plan = usb::plan::build(overrun_model);
        if (!overrun_plan) {
            std::fprintf(stderr, "[ERR] read10-overrun plan build failed err=%d\n", static_cast<int>(overrun_plan.error()));
            return 1;
        }

        const auto overrun_runtime = usb::runtime::host_mock(
            overrun_session.dcd_ops(),
            &overrun_session,
            &overrun_session.adapter(),
            usb::runtime::MscReadyHook{&on_ready, &overrun_demo});

        auto overrun_binding = usb::runtime::make(overrun_plan.value(), overrun_registry, overrun_runtime);
        const auto overrun_init = decltype(overrun_binding)::init_trampoline(&overrun_binding);
        if (!overrun_init) {
            std::fprintf(stderr, "[ERR] read10-overrun binding init failed err=%d\n", static_cast<int>(overrun_init.error()));
            return 1;
        }
        if (!usb::fixture::expect(overrun_demo.ready, "read10-overrun runtime ready hook not called")) return 1;

        PumpContext overrun_pump{overrun_demo.bot, overrun_plan.value().msc.msc_cfg};
        const auto overrun_replay = usb::replay::run(
            overrun_session,
            overrun_roundtrip.trace,
            usb::replay::Hooks{&pump_in, &overrun_pump, &pump_stall});
        if (!overrun_replay) {
            std::fprintf(stderr,
                         "[ERR] read10-overrun replay failed step=%zu err=%s\n",
                         overrun_replay.step_index,
                         usb::replay::error_name(overrun_replay.error));
            return 1;
        }

        const auto overrun_cfg = overrun_plan.value().msc.msc_cfg;
        if (!usb::fixture::expect(count_host_event(overrun_session.host_events(), usb::mock::HostEventKind::out_packet, overrun_cfg.ep_out) == 1,
                                  "read10-overrun host out count mismatch")) return 1;
        if (!usb::fixture::expect(count_host_event(overrun_session.host_events(), usb::mock::HostEventKind::in_complete, overrun_cfg.ep_in) == 9,
                                  "read10-overrun host in-complete count mismatch")) return 1;
        if (!usb::fixture::expect(has_host_event(overrun_session.host_events(), usb::mock::HostEventKind::clear_stall, overrun_cfg.ep_in),
                                  "read10-overrun clear-stall host event missing")) return 1;
        if (!usb::fixture::expect(count_device_action(overrun_session.device_actions(), usb::mock::DeviceActionKind::stall_ep, overrun_cfg.ep_in) == 1,
                                  "read10-overrun stall device action count mismatch")) return 1;
        if (!usb::fixture::expect(!overrun_session.endpoint_state(overrun_cfg.ep_in).stalled,
                                  "read10-overrun bulk in endpoint should be cleared after recovery")) return 1;

        constexpr auto kRead10Overrun = static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_10);
        const auto* overrun_read10 = find_msc_trace_event(overrun_demo.bot->trace_events(),
                                                          usb::class_driver::MscTraceEventKind::read10_started,
                                                          kRead10Overrun);
        if (!usb::fixture::expect(overrun_read10 != nullptr, "read10-overrun trace missing")) return 1;
        if (!usb::fixture::expect(overrun_read10->transfer_length == 1024, "read10-overrun transfer length mismatch")) return 1;
        if (!usb::fixture::expect(overrun_read10->lba == 0, "read10-overrun lba mismatch")) return 1;
        if (!usb::fixture::expect(overrun_read10->blocks == 1, "read10-overrun block count mismatch")) return 1;

        const auto* overrun_data_in = find_msc_trace_event(overrun_demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::data_in_started,
                                                           kRead10Overrun);
        if (!usb::fixture::expect(overrun_data_in != nullptr, "read10-overrun data-in trace missing")) return 1;
        if (!usb::fixture::expect(overrun_data_in->transfer_length == 512, "read10-overrun data-in length mismatch")) return 1;
        if (!usb::fixture::expect(overrun_data_in->residue == 512, "read10-overrun data-in residue mismatch")) return 1;

        const auto* overrun_stall_in = find_msc_trace_event(overrun_demo.bot->trace_events(),
                                                            usb::class_driver::MscTraceEventKind::stall_in_requested,
                                                            kRead10Overrun);
        if (!usb::fixture::expect(overrun_stall_in != nullptr, "read10-overrun stall-in trace missing")) return 1;
        if (!usb::fixture::expect(overrun_stall_in->transfer_length == 1024, "read10-overrun stall-in length mismatch")) return 1;
        if (!usb::fixture::expect(overrun_stall_in->residue == 512, "read10-overrun stall-in residue mismatch")) return 1;
        if (!usb::fixture::expect(overrun_stall_in->flag, "read10-overrun stall-in flag mismatch")) return 1;

        const auto* overrun_wait_csw = find_msc_trace_event(overrun_demo.bot->trace_events(),
                                                            usb::class_driver::MscTraceEventKind::wait_csw,
                                                            kRead10Overrun);
        if (!usb::fixture::expect(overrun_wait_csw != nullptr, "read10-overrun wait-csw trace missing")) return 1;
        if (!usb::fixture::expect(overrun_wait_csw->transfer_length == 1024, "read10-overrun wait-csw length mismatch")) return 1;

        const auto* overrun_phase_error = find_msc_trace_event(overrun_demo.bot->trace_events(),
                                                               usb::class_driver::MscTraceEventKind::phase_error,
                                                               kRead10Overrun);
        if (!usb::fixture::expect(overrun_phase_error != nullptr, "read10-overrun phase-error trace missing")) return 1;
        if (!usb::fixture::expect(overrun_phase_error->residue == 512, "read10-overrun phase-error residue mismatch")) return 1;

        const auto* overrun_clear_stall = find_msc_trace_event(overrun_demo.bot->trace_events(),
                                                               usb::class_driver::MscTraceEventKind::clear_stall_seen,
                                                               kRead10Overrun);
        if (!usb::fixture::expect(overrun_clear_stall != nullptr, "read10-overrun clear-stall trace missing")) return 1;
        if (!usb::fixture::expect(overrun_clear_stall->flag, "read10-overrun clear-stall flag mismatch")) return 1;

        const auto* overrun_csw_sent = find_msc_trace_event(overrun_demo.bot->trace_events(),
                                                            usb::class_driver::MscTraceEventKind::csw_sent,
                                                            kRead10Overrun);
        if (!usb::fixture::expect(overrun_csw_sent != nullptr, "read10-overrun csw trace missing")) return 1;
        if (!usb::fixture::expect(overrun_csw_sent->residue == 512, "read10-overrun csw residue mismatch")) return 1;
        if (!usb::fixture::expect(overrun_csw_sent->flag, "read10-overrun csw phase flag mismatch")) return 1;
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
