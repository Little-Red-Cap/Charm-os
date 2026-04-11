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

    const auto hex_encode_bytes = [] (std::span<const usb::u8> bytes) {
        static constexpr char kHex[] = "0123456789ABCDEF";
        std::string out{};
        out.reserve(bytes.size() * 2);
        for (const auto byte : bytes) {
            out.push_back(kHex[(byte >> 4u) & 0x0Fu]);
            out.push_back(kHex[byte & 0x0Fu]);
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

        std::string invalid_cbw_recovery_boardlog{};
        invalid_cbw_recovery_boardlog.reserve(2048);
        invalid_cbw_recovery_boardlog += "usb: connect on\n";
        invalid_cbw_recovery_boardlog += "usb: reset\n";
        invalid_cbw_recovery_boardlog += "usb: dev_desc size=18 12 01 00 02 00 00 00 40 09 12 06 00 00 01 01 02 03 01\n";
        invalid_cbw_recovery_boardlog += "usb: cfg_desc size=32 09 02 20 00 01 01 00 80 32 09 04 00 00 02 08 06 50 00 07 05 01 02 40 00 00 07 05 81 02 40 00 00\n";
        invalid_cbw_recovery_boardlog += "usb: setup bm=0x80 b=0x06 wValue=0x0100 wIndex=0x0000 wLen=0x0040\n";
        invalid_cbw_recovery_boardlog += "usb: setup bm=0x00 b=0x05 wValue=0x0007 wIndex=0x0000 wLen=0x0000\n";
        invalid_cbw_recovery_boardlog += "usb: setup bm=0x80 b=0x06 wValue=0x0200 wIndex=0x0000 wLen=0x00FF\n";
        invalid_cbw_recovery_boardlog += "usb: setup bm=0x00 b=0x09 wValue=0x0001 wIndex=0x0000 wLen=0x0000\n";
        invalid_cbw_recovery_boardlog += "usb: setup bm=0xA1 b=0xFE wValue=0x0000 wIndex=0x0000 wLen=0x0001\n";
        invalid_cbw_recovery_boardlog += "usb: out ep=0x01 zlp=0 data=";
        invalid_cbw_recovery_boardlog += kInvalidCbw;
        invalid_cbw_recovery_boardlog += "\n";
        invalid_cbw_recovery_boardlog += "usb: stall ep=0x81\n";
        invalid_cbw_recovery_boardlog += "usb: setup bm=0x02 b=0x01 wValue=0x0000 wIndex=0x0081 wLen=0x0000\n";
        invalid_cbw_recovery_boardlog += "usb: in ep=0x81 zlp=0 data=";
        invalid_cbw_recovery_boardlog += kInvalidCsw;
        invalid_cbw_recovery_boardlog += "\n";
        invalid_cbw_recovery_boardlog += "usb: out ep=0x01 zlp=0 data=";
        invalid_cbw_recovery_boardlog += kRecoveryCbw;
        invalid_cbw_recovery_boardlog += "\n";
        invalid_cbw_recovery_boardlog += "usb: in ep=0x81 zlp=0 data=";
        invalid_cbw_recovery_boardlog += kRecoveryReadCapacity;
        invalid_cbw_recovery_boardlog += "\n";

        const auto imported_invalid_cbw_recovery = usb::boardlog::load_text(invalid_cbw_recovery_boardlog);
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
        std::string read10_short_boardlog{};
        read10_short_boardlog.reserve(2048);
        read10_short_boardlog += "usb: connect on\n";
        read10_short_boardlog += "usb: reset\n";
        read10_short_boardlog += "usb: dev_desc size=18 12 01 00 02 00 00 00 40 09 12 06 00 00 01 01 02 03 01\n";
        read10_short_boardlog += "usb: cfg_desc size=32 09 02 20 00 01 01 00 80 32 09 04 00 00 02 08 06 50 00 07 05 01 02 40 00 00 07 05 81 02 40 00 00\n";
        read10_short_boardlog += "usb: setup bm=0x80 b=0x06 wValue=0x0100 wIndex=0x0000 wLen=0x0040\n";
        read10_short_boardlog += "usb: setup bm=0x00 b=0x05 wValue=0x0007 wIndex=0x0000 wLen=0x0000\n";
        read10_short_boardlog += "usb: setup bm=0x80 b=0x06 wValue=0x0200 wIndex=0x0000 wLen=0x00FF\n";
        read10_short_boardlog += "usb: setup bm=0x00 b=0x09 wValue=0x0001 wIndex=0x0000 wLen=0x0000\n";
        read10_short_boardlog += "usb: setup bm=0xA1 b=0xFE wValue=0x0000 wIndex=0x0000 wLen=0x0001\n";
        read10_short_boardlog += "usb: out ep=0x01 zlp=0 data=";
        read10_short_boardlog += kRead10ShortCbw;
        read10_short_boardlog += "\n";
        read10_short_boardlog += "usb: in ep=0x81 zlp=0 data=";
        read10_short_boardlog += hex_encode_bytes(short_read_disk.block_span(0));
        read10_short_boardlog += kRead10ShortCsw;
        read10_short_boardlog += "\n";

        const auto imported_read10_short = usb::boardlog::load_text(read10_short_boardlog);
        if (!imported_read10_short) {
            std::fprintf(stderr,
                         "[ERR] read10-short boardlog load failed line=%zu err=%s\n",
                         imported_read10_short.line,
                         usb::boardlog::error_name(imported_read10_short.error));
            return 1;
        }
        if (!usb::fixture::expect(imported_read10_short.imported_steps == 9, "read10-short imported step count mismatch")) return 1;
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
