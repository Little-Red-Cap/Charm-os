#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

import block.device;
import block.registry;
import usb.class_cdc;
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
    constexpr auto kCdcProductStr = usb::make_ascii_string_descriptor("Charm CDC Replay");
    constexpr auto kMscProductStr = usb::make_ascii_string_descriptor("Charm MSC Replay");
    constexpr auto kMscCdcProductStr = usb::make_ascii_string_descriptor("Charm MSC+CDC Replay");
    constexpr auto kSerialStr = usb::make_ascii_string_descriptor("0001");

    static const usb::StringTable<4> kCdcStrings{
        std::array<std::span<const usb::u8>, 4>{
            std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
            std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
            std::span<const usb::u8>(kCdcProductStr.data(), kCdcProductStr.size()),
            std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
        }
    };

    static const usb::StringTable<4> kMscStrings{
        std::array<std::span<const usb::u8>, 4>{
            std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
            std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
            std::span<const usb::u8>(kMscProductStr.data(), kMscProductStr.size()),
            std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
        }
    };

    static const usb::StringTable<4> kMscCdcStrings{
        std::array<std::span<const usb::u8>, 4>{
            std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
            std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
            std::span<const usb::u8>(kMscCdcProductStr.data(), kMscCdcProductStr.size()),
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

    struct CdcDemoContext {
        std::array<usb::u8, 256> tx{};
        std::array<usb::u8, 256> rx{};
        std::size_t tx_len{0};
        std::size_t rx_len{0};
        usb::class_driver::CdcLineCoding last_line_coding{};
        usb::u16 control_line_state{0};
        bool ready{false};
    };

    std::span<usb::u8> cdc_tx_buffer(void* ctx) noexcept {
        auto* demo = static_cast<CdcDemoContext*>(ctx);
        return demo ? std::span<usb::u8>(demo->tx.data(), demo->tx.size()) : std::span<usb::u8>{};
    }

    std::span<usb::u8> cdc_rx_buffer(void* ctx) noexcept {
        auto* demo = static_cast<CdcDemoContext*>(ctx);
        return demo ? std::span<usb::u8>(demo->rx.data(), demo->rx.size()) : std::span<usb::u8>{};
    }

    std::size_t cdc_tx_length(void* ctx) noexcept {
        auto* demo = static_cast<CdcDemoContext*>(ctx);
        return demo ? demo->tx_len : 0;
    }

    void cdc_on_rx_done(void* ctx, std::size_t len) noexcept {
        auto* demo = static_cast<CdcDemoContext*>(ctx);
        if (demo) demo->rx_len = len;
    }

    void cdc_on_tx_done(void* ctx, std::size_t len) noexcept {
        auto* demo = static_cast<CdcDemoContext*>(ctx);
        if (!demo) return;
        if (len >= demo->tx_len) {
            demo->tx_len = 0;
            return;
        }
        std::memmove(demo->tx.data(), demo->tx.data() + len, demo->tx_len - len);
        demo->tx_len -= len;
    }

    void cdc_on_line_coding(void* ctx, const usb::class_driver::CdcLineCoding& coding) noexcept {
        auto* demo = static_cast<CdcDemoContext*>(ctx);
        if (demo) demo->last_line_coding = coding;
    }

    void cdc_on_control_line(void* ctx, usb::u16 value) noexcept {
        auto* demo = static_cast<CdcDemoContext*>(ctx);
        if (demo) demo->control_line_state = value;
    }

    void cdc_on_ready(void* ctx,
                      usb::class_driver::CdcAcm*,
                      const usb::class_driver::CdcConfig*) noexcept {
        auto* demo = static_cast<CdcDemoContext*>(ctx);
        if (demo) demo->ready = true;
    }

    struct CdcPumpContext {
        usb::class_driver::CdcAcm* cdc{nullptr};
    };

    bool cdc_pump_in(void* ctx, usb::mock::Session& session, usb::u8 ep) noexcept {
        auto* pump = static_cast<CdcPumpContext*>(ctx);
        if (!pump || !pump->cdc) return false;
        if (ep != pump->cdc->config().ep_in) return false;
        return usb::device::examples::send_cdc_in_packet(
            session.dcd_ops(), &session, *pump->cdc, pump->cdc->config().ep_mps);
    }

    struct MscDemoContext {
        bool ready{false};
        usb::class_driver::MscBot* bot{nullptr};
        usb::class_driver::MscConfig cfg{};
    };

    void msc_on_ready(void* ctx,
                      usb::class_driver::MscBot* bot,
                      const usb::class_driver::MscConfig* cfg) noexcept {
        auto* demo = static_cast<MscDemoContext*>(ctx);
        if (!demo || !bot || !cfg) return;
        demo->ready = true;
        demo->bot = bot;
        demo->cfg = *cfg;
    }

    struct MscPumpContext {
        usb::class_driver::MscBot* bot{nullptr};
        usb::class_driver::MscConfig cfg{};
    };

    bool msc_pump_in(void* ctx, usb::mock::Session& session, usb::u8 ep) noexcept {
        auto* pump = static_cast<MscPumpContext*>(ctx);
        if (!pump || !pump->bot) return false;
        if (ep != pump->cfg.ep_in) return false;
        return usb::device::examples::send_msc_in_packet(session.dcd_ops(), &session, *pump->bot, pump->cfg);
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

    struct MscCdcPumpContext {
        usb::class_driver::MscBot* bot{nullptr};
        usb::class_driver::MscConfig msc_cfg{};
        usb::class_driver::CdcAcm* cdc{nullptr};
        usb::class_driver::CdcConfig cdc_cfg{};
        bool notify_sent{false};
    };

    bool msc_cdc_pump_in(void* ctx, usb::mock::Session& session, usb::u8 ep) noexcept {
        auto* pump = static_cast<MscCdcPumpContext*>(ctx);
        if (!pump) return false;

        if (pump->cdc) {
            if (ep == pump->cdc_cfg.ep_notify) {
                if (pump->notify_sent) return false;
                const auto ok = usb::device::examples::send_cdc_serial_state(
                    session.dcd_ops(), &session, *pump->cdc, 0x0003);
                if (ok) {
                    pump->notify_sent = true;
                }
                return ok;
            }
            if (ep == pump->cdc_cfg.ep_in) {
                return usb::device::examples::send_cdc_in_packet(
                    session.dcd_ops(), &session, *pump->cdc, pump->cdc_cfg.ep_mps);
            }
        }

        if (pump->bot && ep == pump->msc_cfg.ep_in) {
            return usb::device::examples::send_msc_in_packet(
                session.dcd_ops(), &session, *pump->bot, pump->msc_cfg);
        }

        return false;
    }

    bool run_cdc_case(std::string_view trace_path, std::FILE* stream) noexcept {
        CdcDemoContext demo{};
        usb::mock::Session session{};

        usb::class_driver::CdcOps cdc_ops{};
        cdc_ops.tx_buffer = &cdc_tx_buffer;
        cdc_ops.rx_buffer = &cdc_rx_buffer;
        cdc_ops.tx_length = &cdc_tx_length;
        cdc_ops.on_rx_done = &cdc_on_rx_done;
        cdc_ops.on_tx_done = &cdc_on_tx_done;
        cdc_ops.on_line_coding = &cdc_on_line_coding;
        cdc_ops.on_control_line = &cdc_on_control_line;

        usb::spec::DeviceSpec device{};
        device.vendor_id = 0x1209;
        device.product_id = 0x0005;
        device.i_manufacturer = 1;
        device.i_product = 2;
        device.i_serial = 3;
        device.strings = std::span<const std::span<const usb::u8>>(kCdcStrings.entries.data(), kCdcStrings.entries.size());

        usb::spec::CdcFunctionSpec cdc{};
        cdc.cap_name = "usb.cdc0";

        const auto spec = usb::spec::cdc_device(device, cdc);
        const auto model = usb::build(spec);
        const auto plan = usb::plan::build(model);
        if (!plan) {
            std::fprintf(stream, "[ERR] plan build failed err=%d\n", static_cast<int>(plan.error()));
            return false;
        }

        const auto runtime = usb::runtime::host_mock(
            session.dcd_ops(),
            &session,
            &session.adapter(),
            {},
            usb::runtime::CdcRuntimeConfig{&demo, cdc_ops, usb::runtime::CdcReadyHook{&cdc_on_ready, &demo}});

        auto binding = usb::runtime::make(plan.value(), runtime);
        const auto init = decltype(binding)::init_trampoline(&binding);
        if (!init) {
            std::fprintf(stream, "[ERR] binding init failed err=%d\n", static_cast<int>(init.error()));
            return false;
        }

        if (!usb::fixture::expect(demo.ready, "cdc ready hook not called", stream)) return false;
        if (!usb::fixture::expect(binding.cdc.has_value(), "cdc instance missing", stream)) return false;

        constexpr char kPing[] = "ping";
        constexpr char kPong[] = "pong";
        std::memcpy(demo.tx.data(), kPong, 4);
        demo.tx_len = 4;

        CdcPumpContext pump{&(*binding.cdc)};
        if (!usb::fixture::run_replay_file(session, trace_path, usb::replay::Hooks{&cdc_pump_in, &pump}, stream)) {
            return false;
        }

        const auto cdc_cfg = plan.value().cdc.cdc_cfg;
        if (!usb::fixture::expect(session.address() == 5, "device address not applied", stream)) return false;
        if (!usb::fixture::expect(session.configured(), "device not configured", stream)) return false;
        if (!usb::fixture::expect(session.endpoint_state(cdc_cfg.ep_notify).opened, "cdc notify endpoint not opened", stream)) return false;
        if (!usb::fixture::expect(session.endpoint_state(cdc_cfg.ep_out).opened, "cdc bulk out endpoint not opened", stream)) return false;
        if (!usb::fixture::expect(session.endpoint_state(cdc_cfg.ep_in).opened, "cdc bulk in endpoint not opened", stream)) return false;
        if (!usb::fixture::expect(demo.last_line_coding.baud == 921600, "line coding callback mismatch", stream)) return false;
        if (!usb::fixture::expect(demo.rx_len == 4, "cdc bulk out length mismatch", stream)) return false;
        if (!usb::fixture::expect(std::memcmp(demo.rx.data(), kPing, 4) == 0, "cdc bulk out payload mismatch", stream)) return false;
        if (!usb::fixture::expect(demo.tx_len == 0, "cdc tx length not drained", stream)) return false;
        return true;
    }

    bool run_msc_case(std::string_view case_name,
                      std::string_view trace_path,
                      std::FILE* stream) noexcept {
        MemoryDisk disk{};
        block::Registry<2> registry{};
        registry.init();
        auto reg = registry.register_device({"block.sd0", block::cap_id("block.sd0")}, disk.device);
        if (!reg) {
            std::fprintf(stream, "[ERR] registry register failed err=%d\n", static_cast<int>(reg.error()));
            return false;
        }

        MscDemoContext demo{};
        usb::mock::Session session{};

        usb::spec::DeviceSpec device{};
        device.vendor_id = 0x1209;
        device.product_id = 0x0006;
        device.i_manufacturer = 1;
        device.i_product = 2;
        device.i_serial = 3;
        device.strings = std::span<const std::span<const usb::u8>>(kMscStrings.entries.data(), kMscStrings.entries.size());

        usb::spec::MscFunctionSpec msc{};
        msc.cap_name = "usb.msc0";
        msc.block_cap = "block.sd0";
        msc.vendor = "Charm";
        msc.product = "MockDisk";
        msc.revision = "1.00";
        const bool writable_case = case_name.starts_with("msc-write10-");
        disk.set_writable(writable_case);
        msc.read_only = !writable_case;

        const auto spec = usb::spec::msc_device(device, msc);
        const auto model = usb::build(spec);
        const auto plan = usb::plan::build(model);
        if (!plan) {
            std::fprintf(stream, "[ERR] plan build failed err=%d\n", static_cast<int>(plan.error()));
            return false;
        }

        const auto runtime = usb::runtime::host_mock(
            session.dcd_ops(),
            &session,
            &session.adapter(),
            usb::runtime::MscReadyHook{&msc_on_ready, &demo});

        auto binding = usb::runtime::make(plan.value(), registry, runtime);
        const auto init = decltype(binding)::init_trampoline(&binding);
        if (!init) {
            std::fprintf(stream, "[ERR] binding init failed err=%d\n", static_cast<int>(init.error()));
            return false;
        }

        if (!usb::fixture::expect(demo.ready, "msc ready hook not called", stream)) return false;

        MscPumpContext pump{demo.bot, plan.value().msc.msc_cfg};
        session.clear_trace();
        demo.bot->clear_trace();
        if (!usb::fixture::run_replay_file(session, trace_path, usb::replay::Hooks{&msc_pump_in, &pump}, stream)) {
            return false;
        }

        const auto msc_cfg = plan.value().msc.msc_cfg;
        if (!usb::fixture::expect(session.address() == 7, "device address not applied", stream)) return false;
        if (!usb::fixture::expect(session.configured(), "device not configured", stream)) return false;
        if (!usb::fixture::expect(session.endpoint_state(msc_cfg.ep_out).opened, "msc bulk out endpoint not opened", stream)) return false;
        if (!usb::fixture::expect(session.endpoint_state(msc_cfg.ep_in).opened, "msc bulk in endpoint not opened", stream)) return false;
        if (case_name == "msc-recovery") {
            if (!usb::fixture::expect(has_host_event(session.host_events(), usb::mock::HostEventKind::clear_stall, msc_cfg.ep_out),
                                      "msc recovery clear-stall host event missing", stream)) return false;
            constexpr auto kRead10 = static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_10);
            const auto* stall_out = find_msc_trace_event(demo.bot->trace_events(),
                                                         usb::class_driver::MscTraceEventKind::stall_out_requested,
                                                         kRead10);
            if (!usb::fixture::expect(stall_out != nullptr, "msc recovery stall-out trace missing", stream)) return false;
            if (!usb::fixture::expect(stall_out->transfer_length == 512, "msc recovery stall-out transfer length mismatch", stream)) return false;
            if (!usb::fixture::expect(stall_out->residue == 512, "msc recovery stall-out residue mismatch", stream)) return false;
            if (!usb::fixture::expect(!stall_out->flag, "msc recovery stall-out direction flag mismatch", stream)) return false;

            const auto* wait_csw = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::wait_csw,
                                                        kRead10);
            if (!usb::fixture::expect(wait_csw != nullptr, "msc recovery wait-csw trace missing", stream)) return false;
            if (!usb::fixture::expect(wait_csw->transfer_length == 512, "msc recovery wait-csw transfer length mismatch", stream)) return false;

            const auto* phase_error = find_msc_trace_event(demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::phase_error,
                                                           kRead10);
            if (!usb::fixture::expect(phase_error != nullptr, "msc recovery phase-error trace missing", stream)) return false;
            if (!usb::fixture::expect(phase_error->transfer_length == 512, "msc recovery phase-error transfer length mismatch", stream)) return false;
            if (!usb::fixture::expect(phase_error->residue == 512, "msc recovery phase-error residue mismatch", stream)) return false;

            const auto* clear_stall = find_msc_trace_event(demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::clear_stall_seen,
                                                           kRead10);
            if (!usb::fixture::expect(clear_stall != nullptr, "msc recovery clear-stall trace missing", stream)) return false;
            if (!usb::fixture::expect(!clear_stall->flag, "msc recovery clear-stall endpoint flag mismatch", stream)) return false;

            const auto* csw_sent = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::csw_sent,
                                                        kRead10);
            if (!usb::fixture::expect(csw_sent != nullptr, "msc recovery csw trace missing", stream)) return false;
            if (!usb::fixture::expect(csw_sent->residue == 512, "msc recovery csw residue mismatch", stream)) return false;
            if (!usb::fixture::expect(csw_sent->flag, "msc recovery csw phase flag mismatch", stream)) return false;
        } else if (case_name == "msc-invalid-cbw") {
            constexpr auto kReadCapacity = static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_capacity_10);
            if (!usb::fixture::expect(has_host_event(session.host_events(), usb::mock::HostEventKind::clear_stall, msc_cfg.ep_in),
                                      "msc invalid-cbw clear-stall host event missing", stream)) return false;
            const auto* cbw_invalid = find_msc_trace_event(demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::cbw_invalid,
                                                           kReadCapacity);
            if (!usb::fixture::expect(cbw_invalid != nullptr, "msc invalid-cbw trace missing", stream)) return false;
            if (!usb::fixture::expect(cbw_invalid->transfer_length == 8, "msc invalid-cbw transfer length mismatch", stream)) return false;
            if (!usb::fixture::expect(cbw_invalid->residue == 8, "msc invalid-cbw residue mismatch", stream)) return false;
            if (!usb::fixture::expect(cbw_invalid->flag, "msc invalid-cbw direction flag mismatch", stream)) return false;

            const auto* wait_csw = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::wait_csw,
                                                        kReadCapacity);
            if (!usb::fixture::expect(wait_csw != nullptr, "msc invalid-cbw wait-csw trace missing", stream)) return false;
            if (!usb::fixture::expect(wait_csw->transfer_length == 8, "msc invalid-cbw wait-csw transfer length mismatch", stream)) return false;

            const auto* phase_error = find_msc_trace_event(demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::phase_error,
                                                           kReadCapacity);
            if (!usb::fixture::expect(phase_error != nullptr, "msc invalid-cbw phase-error trace missing", stream)) return false;
            if (!usb::fixture::expect(phase_error->residue == 8, "msc invalid-cbw phase-error residue mismatch", stream)) return false;

            const auto* clear_stall = find_msc_trace_event(demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::clear_stall_seen,
                                                           kReadCapacity);
            if (!usb::fixture::expect(clear_stall != nullptr, "msc invalid-cbw clear-stall trace missing", stream)) return false;
            if (!usb::fixture::expect(clear_stall->flag, "msc invalid-cbw clear-stall endpoint flag mismatch", stream)) return false;

            const auto* csw_sent = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::csw_sent,
                                                        kReadCapacity);
            if (!usb::fixture::expect(csw_sent != nullptr, "msc invalid-cbw csw trace missing", stream)) return false;
            if (!usb::fixture::expect(csw_sent->residue == 8, "msc invalid-cbw csw residue mismatch", stream)) return false;
            if (!usb::fixture::expect(csw_sent->flag, "msc invalid-cbw csw phase flag mismatch", stream)) return false;

            const auto* read_capacity = find_msc_trace_event(demo.bot->trace_events(),
                                                             usb::class_driver::MscTraceEventKind::read_capacity,
                                                             kReadCapacity);
            if (!usb::fixture::expect(read_capacity != nullptr, "msc invalid-cbw recovery read-capacity trace missing", stream)) return false;
            if (!usb::fixture::expect(read_capacity->transfer_length == 8, "msc invalid-cbw recovery transfer length mismatch", stream)) return false;
        } else if (case_name == "msc-read-capacity-residue") {
            constexpr auto kReadCapacity = static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_capacity_10);
            const auto* read_capacity = find_msc_trace_event(demo.bot->trace_events(),
                                                             usb::class_driver::MscTraceEventKind::read_capacity,
                                                             kReadCapacity);
            if (!usb::fixture::expect(read_capacity != nullptr, "msc read-capacity residue trace missing", stream)) return false;
            if (!usb::fixture::expect(read_capacity->transfer_length == 10, "msc read-capacity residue transfer length mismatch", stream)) return false;
            if (!usb::fixture::expect(read_capacity->lba == 15, "msc read-capacity residue lba mismatch", stream)) return false;
            if (!usb::fixture::expect(read_capacity->blocks == 16, "msc read-capacity residue block count mismatch", stream)) return false;

            const auto* csw_ready = find_msc_trace_event(demo.bot->trace_events(),
                                                         usb::class_driver::MscTraceEventKind::csw_ready,
                                                         kReadCapacity);
            if (!usb::fixture::expect(csw_ready != nullptr, "msc read-capacity residue csw-ready trace missing", stream)) return false;
            if (!usb::fixture::expect(csw_ready->residue == 2, "msc read-capacity residue csw-ready residue mismatch", stream)) return false;
            if (!usb::fixture::expect(!csw_ready->flag, "msc read-capacity residue csw-ready phase flag mismatch", stream)) return false;

            const auto* csw_sent = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::csw_sent,
                                                        kReadCapacity);
            if (!usb::fixture::expect(csw_sent != nullptr, "msc read-capacity residue csw-sent trace missing", stream)) return false;
            if (!usb::fixture::expect(csw_sent->residue == 2, "msc read-capacity residue csw-sent residue mismatch", stream)) return false;
            if (!usb::fixture::expect(!csw_sent->flag, "msc read-capacity residue csw-sent phase flag mismatch", stream)) return false;
        } else if (case_name == "msc-read10-short") {
            constexpr auto kRead10 = static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_10);
            if (!usb::fixture::expect(!has_host_event(session.host_events(), usb::mock::HostEventKind::clear_stall, msc_cfg.ep_in),
                                      "msc read10 short should not clear stall", stream)) return false;

            const auto* read10 = find_msc_trace_event(demo.bot->trace_events(),
                                                      usb::class_driver::MscTraceEventKind::read10_started,
                                                      kRead10);
            if (!usb::fixture::expect(read10 != nullptr, "msc read10 short trace missing", stream)) return false;
            if (!usb::fixture::expect(read10->transfer_length == 512, "msc read10 short transfer length mismatch", stream)) return false;
            if (!usb::fixture::expect(read10->lba == 0, "msc read10 short lba mismatch", stream)) return false;
            if (!usb::fixture::expect(read10->blocks == 2, "msc read10 short block count mismatch", stream)) return false;

            const auto* data_in = find_msc_trace_event(demo.bot->trace_events(),
                                                       usb::class_driver::MscTraceEventKind::data_in_started,
                                                       kRead10);
            if (!usb::fixture::expect(data_in != nullptr, "msc read10 short data-in trace missing", stream)) return false;
            if (!usb::fixture::expect(data_in->transfer_length == 512, "msc read10 short data-in length mismatch", stream)) return false;
            if (!usb::fixture::expect(data_in->lba == 0, "msc read10 short data-in lba mismatch", stream)) return false;
            if (!usb::fixture::expect(data_in->blocks == 2, "msc read10 short data-in block count mismatch", stream)) return false;
            if (!usb::fixture::expect(data_in->residue == 512, "msc read10 short data-in residue mismatch", stream)) return false;

            const auto* csw_ready = find_msc_trace_event(demo.bot->trace_events(),
                                                         usb::class_driver::MscTraceEventKind::csw_ready,
                                                         kRead10);
            if (!usb::fixture::expect(csw_ready != nullptr, "msc read10 short csw-ready trace missing", stream)) return false;
            if (!usb::fixture::expect(csw_ready->residue == 512, "msc read10 short csw-ready residue mismatch", stream)) return false;
            if (!usb::fixture::expect(!csw_ready->flag, "msc read10 short csw-ready phase flag mismatch", stream)) return false;

            const auto* csw_sent = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::csw_sent,
                                                        kRead10);
            if (!usb::fixture::expect(csw_sent != nullptr, "msc read10 short csw trace missing", stream)) return false;
            if (!usb::fixture::expect(csw_sent->residue == 512, "msc read10 short csw residue mismatch", stream)) return false;
            if (!usb::fixture::expect(!csw_sent->flag, "msc read10 short csw phase flag mismatch", stream)) return false;
        } else if (case_name == "msc-read10-overrun") {
            constexpr auto kRead10 = static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_10);
            if (!usb::fixture::expect(has_host_event(session.host_events(), usb::mock::HostEventKind::clear_stall, msc_cfg.ep_in),
                                      "msc read10 overrun clear-stall host event missing", stream)) return false;

            const auto* read10 = find_msc_trace_event(demo.bot->trace_events(),
                                                      usb::class_driver::MscTraceEventKind::read10_started,
                                                      kRead10);
            if (!usb::fixture::expect(read10 != nullptr, "msc read10 overrun trace missing", stream)) return false;
            if (!usb::fixture::expect(read10->transfer_length == 1024, "msc read10 overrun transfer length mismatch", stream)) return false;
            if (!usb::fixture::expect(read10->lba == 0, "msc read10 overrun lba mismatch", stream)) return false;
            if (!usb::fixture::expect(read10->blocks == 1, "msc read10 overrun block count mismatch", stream)) return false;

            const auto* data_in = find_msc_trace_event(demo.bot->trace_events(),
                                                       usb::class_driver::MscTraceEventKind::data_in_started,
                                                       kRead10);
            if (!usb::fixture::expect(data_in != nullptr, "msc read10 overrun data-in trace missing", stream)) return false;
            if (!usb::fixture::expect(data_in->transfer_length == 512, "msc read10 overrun data-in length mismatch", stream)) return false;
            if (!usb::fixture::expect(data_in->residue == 512, "msc read10 overrun data-in residue mismatch", stream)) return false;

            const auto* stall_in = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::stall_in_requested,
                                                        kRead10);
            if (!usb::fixture::expect(stall_in != nullptr, "msc read10 overrun stall-in trace missing", stream)) return false;
            if (!usb::fixture::expect(stall_in->transfer_length == 1024, "msc read10 overrun stall-in length mismatch", stream)) return false;
            if (!usb::fixture::expect(stall_in->residue == 512, "msc read10 overrun stall-in residue mismatch", stream)) return false;
            if (!usb::fixture::expect(stall_in->flag, "msc read10 overrun stall-in flag mismatch", stream)) return false;

            const auto* wait_csw = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::wait_csw,
                                                        kRead10);
            if (!usb::fixture::expect(wait_csw != nullptr, "msc read10 overrun wait-csw trace missing", stream)) return false;
            if (!usb::fixture::expect(wait_csw->transfer_length == 1024, "msc read10 overrun wait-csw length mismatch", stream)) return false;

            const auto* phase_error = find_msc_trace_event(demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::phase_error,
                                                           kRead10);
            if (!usb::fixture::expect(phase_error != nullptr, "msc read10 overrun phase-error trace missing", stream)) return false;
            if (!usb::fixture::expect(phase_error->residue == 512, "msc read10 overrun phase-error residue mismatch", stream)) return false;

            const auto* clear_stall = find_msc_trace_event(demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::clear_stall_seen,
                                                           kRead10);
            if (!usb::fixture::expect(clear_stall != nullptr, "msc read10 overrun clear-stall trace missing", stream)) return false;
            if (!usb::fixture::expect(clear_stall->flag, "msc read10 overrun clear-stall flag mismatch", stream)) return false;

            const auto* csw_sent = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::csw_sent,
                                                        kRead10);
            if (!usb::fixture::expect(csw_sent != nullptr, "msc read10 overrun csw trace missing", stream)) return false;
            if (!usb::fixture::expect(csw_sent->residue == 512, "msc read10 overrun csw residue mismatch", stream)) return false;
            if (!usb::fixture::expect(csw_sent->flag, "msc read10 overrun csw phase flag mismatch", stream)) return false;
        } else if (case_name == "msc-read10-zero-len") {
            constexpr auto kRead10 = static_cast<usb::u8>(usb::class_driver::ScsiCmd::read_10);
            if (!usb::fixture::expect(has_host_event(session.host_events(), usb::mock::HostEventKind::clear_stall, msc_cfg.ep_in),
                                      "msc read10 zero-len clear-stall host event missing", stream)) return false;
            const auto* read10 = find_msc_trace_event(demo.bot->trace_events(),
                                                      usb::class_driver::MscTraceEventKind::read10_started,
                                                      kRead10);
            if (!usb::fixture::expect(read10 != nullptr, "msc read10 zero-len read10 trace missing", stream)) return false;
            if (!usb::fixture::expect(read10->transfer_length == 0, "msc read10 zero-len transfer length mismatch", stream)) return false;
            if (!usb::fixture::expect(read10->lba == 0, "msc read10 zero-len lba mismatch", stream)) return false;
            if (!usb::fixture::expect(read10->blocks == 1, "msc read10 zero-len block count mismatch", stream)) return false;

            const auto* sense = find_msc_trace_event(demo.bot->trace_events(),
                                                     usb::class_driver::MscTraceEventKind::sense_set,
                                                     kRead10);
            if (!usb::fixture::expect(sense != nullptr, "msc read10 zero-len sense trace missing", stream)) return false;
            if (!usb::fixture::expect(sense->sense_key == 0x05, "msc read10 zero-len sense key mismatch", stream)) return false;
            if (!usb::fixture::expect(sense->sense_asc == 0x20, "msc read10 zero-len sense asc mismatch", stream)) return false;
            if (!usb::fixture::expect(sense->sense_ascq == 0x00, "msc read10 zero-len sense ascq mismatch", stream)) return false;

            const auto* stall_in = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::stall_in_requested,
                                                        kRead10);
            if (!usb::fixture::expect(stall_in != nullptr, "msc read10 zero-len stall-in trace missing", stream)) return false;
            if (!usb::fixture::expect(stall_in->residue == 0, "msc read10 zero-len stall-in residue mismatch", stream)) return false;
            if (!usb::fixture::expect(stall_in->flag, "msc read10 zero-len stall-in direction flag mismatch", stream)) return false;

            const auto* wait_csw = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::wait_csw,
                                                        kRead10);
            if (!usb::fixture::expect(wait_csw != nullptr, "msc read10 zero-len wait-csw trace missing", stream)) return false;
            if (!usb::fixture::expect(wait_csw->transfer_length == 0, "msc read10 zero-len wait-csw transfer length mismatch", stream)) return false;

            const auto* phase_error = find_msc_trace_event(demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::phase_error,
                                                           kRead10);
            if (!usb::fixture::expect(phase_error != nullptr, "msc read10 zero-len phase-error trace missing", stream)) return false;
            if (!usb::fixture::expect(phase_error->residue == 0, "msc read10 zero-len phase-error residue mismatch", stream)) return false;

            const auto* clear_stall = find_msc_trace_event(demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::clear_stall_seen,
                                                           kRead10);
            if (!usb::fixture::expect(clear_stall != nullptr, "msc read10 zero-len clear-stall trace missing", stream)) return false;
            if (!usb::fixture::expect(clear_stall->flag, "msc read10 zero-len clear-stall endpoint flag mismatch", stream)) return false;

            const auto* csw_sent = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::csw_sent,
                                                        kRead10);
            if (!usb::fixture::expect(csw_sent != nullptr, "msc read10 zero-len csw trace missing", stream)) return false;
            if (!usb::fixture::expect(csw_sent->residue == 0, "msc read10 zero-len csw residue mismatch", stream)) return false;
            if (!usb::fixture::expect(csw_sent->flag, "msc read10 zero-len csw phase flag mismatch", stream)) return false;
        } else if (case_name == "msc-write10-short") {
            constexpr auto kWrite10 = static_cast<usb::u8>(usb::class_driver::ScsiCmd::write_10);
            if (!usb::fixture::expect(!has_host_event(session.host_events(), usb::mock::HostEventKind::clear_stall, msc_cfg.ep_out),
                                      "msc write10 short should not clear stall", stream)) return false;

            const auto* write10 = find_msc_trace_event(demo.bot->trace_events(),
                                                       usb::class_driver::MscTraceEventKind::write10_started,
                                                       kWrite10);
            if (!usb::fixture::expect(write10 != nullptr, "msc write10 short trace missing", stream)) return false;
            if (!usb::fixture::expect(write10->transfer_length == 512, "msc write10 short transfer length mismatch", stream)) return false;
            if (!usb::fixture::expect(write10->lba == 1, "msc write10 short lba mismatch", stream)) return false;
            if (!usb::fixture::expect(write10->blocks == 2, "msc write10 short block count mismatch", stream)) return false;

            const auto* data_out = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::data_out_started,
                                                        kWrite10);
            if (!usb::fixture::expect(data_out != nullptr, "msc write10 short data-out trace missing", stream)) return false;
            if (!usb::fixture::expect(data_out->transfer_length == 512, "msc write10 short data-out length mismatch", stream)) return false;
            if (!usb::fixture::expect(data_out->lba == 1, "msc write10 short data-out lba mismatch", stream)) return false;
            if (!usb::fixture::expect(data_out->blocks == 2, "msc write10 short data-out block count mismatch", stream)) return false;
            if (!usb::fixture::expect(data_out->residue == 512, "msc write10 short data-out residue mismatch", stream)) return false;

            const auto* csw_ready = find_msc_trace_event(demo.bot->trace_events(),
                                                         usb::class_driver::MscTraceEventKind::csw_ready,
                                                         kWrite10);
            if (!usb::fixture::expect(csw_ready != nullptr, "msc write10 short csw-ready trace missing", stream)) return false;
            if (!usb::fixture::expect(csw_ready->residue == 512, "msc write10 short csw-ready residue mismatch", stream)) return false;
            if (!usb::fixture::expect(!csw_ready->flag, "msc write10 short csw-ready phase flag mismatch", stream)) return false;

            const auto* csw_sent = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::csw_sent,
                                                        kWrite10);
            if (!usb::fixture::expect(csw_sent != nullptr, "msc write10 short csw trace missing", stream)) return false;
            if (!usb::fixture::expect(csw_sent->residue == 512, "msc write10 short csw residue mismatch", stream)) return false;
            if (!usb::fixture::expect(!csw_sent->flag, "msc write10 short csw phase flag mismatch", stream)) return false;
            if (!usb::fixture::expect(span_is_filled(disk.block_span(1), 0xA5), "msc write10 short block payload mismatch", stream)) return false;
            if (!usb::fixture::expect(span_is_filled(disk.block_span(2), 0x00), "msc write10 short trailing block should remain empty", stream)) return false;
        } else if (case_name == "msc-write10-overrun") {
            constexpr auto kWrite10 = static_cast<usb::u8>(usb::class_driver::ScsiCmd::write_10);
            if (!usb::fixture::expect(has_host_event(session.host_events(), usb::mock::HostEventKind::clear_stall, msc_cfg.ep_out),
                                      "msc write10 overrun clear-stall host event missing", stream)) return false;

            const auto* write10 = find_msc_trace_event(demo.bot->trace_events(),
                                                       usb::class_driver::MscTraceEventKind::write10_started,
                                                       kWrite10);
            if (!usb::fixture::expect(write10 != nullptr, "msc write10 overrun trace missing", stream)) return false;
            if (!usb::fixture::expect(write10->transfer_length == 1024, "msc write10 overrun transfer length mismatch", stream)) return false;
            if (!usb::fixture::expect(write10->lba == 1, "msc write10 overrun lba mismatch", stream)) return false;
            if (!usb::fixture::expect(write10->blocks == 1, "msc write10 overrun block count mismatch", stream)) return false;

            const auto* data_out = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::data_out_started,
                                                        kWrite10);
            if (!usb::fixture::expect(data_out != nullptr, "msc write10 overrun data-out trace missing", stream)) return false;
            if (!usb::fixture::expect(data_out->transfer_length == 512, "msc write10 overrun data-out length mismatch", stream)) return false;
            if (!usb::fixture::expect(data_out->lba == 1, "msc write10 overrun data-out lba mismatch", stream)) return false;
            if (!usb::fixture::expect(data_out->blocks == 1, "msc write10 overrun data-out block count mismatch", stream)) return false;
            if (!usb::fixture::expect(data_out->residue == 512, "msc write10 overrun data-out residue mismatch", stream)) return false;

            const auto* stall_out = find_msc_trace_event(demo.bot->trace_events(),
                                                         usb::class_driver::MscTraceEventKind::stall_out_requested,
                                                         kWrite10);
            if (!usb::fixture::expect(stall_out != nullptr, "msc write10 overrun stall-out trace missing", stream)) return false;
            if (!usb::fixture::expect(stall_out->transfer_length == 1024, "msc write10 overrun stall-out length mismatch", stream)) return false;
            if (!usb::fixture::expect(stall_out->residue == 512, "msc write10 overrun stall-out residue mismatch", stream)) return false;
            if (!usb::fixture::expect(!stall_out->flag, "msc write10 overrun stall-out flag mismatch", stream)) return false;

            const auto* wait_csw = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::wait_csw,
                                                        kWrite10);
            if (!usb::fixture::expect(wait_csw != nullptr, "msc write10 overrun wait-csw trace missing", stream)) return false;
            if (!usb::fixture::expect(wait_csw->transfer_length == 1024, "msc write10 overrun wait-csw length mismatch", stream)) return false;

            const auto* phase_error = find_msc_trace_event(demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::phase_error,
                                                           kWrite10);
            if (!usb::fixture::expect(phase_error != nullptr, "msc write10 overrun phase-error trace missing", stream)) return false;
            if (!usb::fixture::expect(phase_error->residue == 512, "msc write10 overrun phase-error residue mismatch", stream)) return false;

            const auto* clear_stall = find_msc_trace_event(demo.bot->trace_events(),
                                                           usb::class_driver::MscTraceEventKind::clear_stall_seen,
                                                           kWrite10);
            if (!usb::fixture::expect(clear_stall != nullptr, "msc write10 overrun clear-stall trace missing", stream)) return false;
            if (!usb::fixture::expect(!clear_stall->flag, "msc write10 overrun clear-stall flag mismatch", stream)) return false;

            const auto* csw_sent = find_msc_trace_event(demo.bot->trace_events(),
                                                        usb::class_driver::MscTraceEventKind::csw_sent,
                                                        kWrite10);
            if (!usb::fixture::expect(csw_sent != nullptr, "msc write10 overrun csw trace missing", stream)) return false;
            if (!usb::fixture::expect(csw_sent->residue == 512, "msc write10 overrun csw residue mismatch", stream)) return false;
            if (!usb::fixture::expect(csw_sent->flag, "msc write10 overrun csw phase flag mismatch", stream)) return false;
            if (!usb::fixture::expect(span_is_filled(disk.block_span(1), 0x5A), "msc write10 overrun block payload mismatch", stream)) return false;
            if (!usb::fixture::expect(span_is_filled(disk.block_span(2), 0x00), "msc write10 overrun extra data should not spill", stream)) return false;
        }
        return true;
    }

    bool run_msc_cdc_case(std::string_view trace_path, std::FILE* stream) noexcept {
        MemoryDisk disk{};
        block::Registry<2> registry{};
        registry.init();
        auto reg = registry.register_device({"block.sd0", block::cap_id("block.sd0")}, disk.device);
        if (!reg) {
            std::fprintf(stream, "[ERR] registry register failed err=%d\n", static_cast<int>(reg.error()));
            return false;
        }

        MscDemoContext demo{};
        usb::mock::Session session{};

        usb::spec::DeviceSpec device{};
        device.vendor_id = 0x1209;
        device.product_id = 0x0004;
        device.device_class = 0xEF;
        device.device_subclass = 0x02;
        device.device_protocol = 0x01;
        device.i_manufacturer = 1;
        device.i_product = 2;
        device.i_serial = 3;
        device.strings = std::span<const std::span<const usb::u8>>(kMscCdcStrings.entries.data(), kMscCdcStrings.entries.size());

        usb::spec::MscFunctionSpec msc{};
        msc.cap_name = "usb.msc0";
        msc.block_cap = "block.sd0";
        msc.vendor = "Charm";
        msc.product = "MockDisk";
        msc.revision = "1.00";
        msc.read_only = true;
        msc.ep_out = 0x01;
        msc.ep_in = 0x81;

        usb::spec::CdcFunctionSpec cdc{};
        cdc.cap_name = "usb.cdc0";
        cdc.ep_notify = 0x82;
        cdc.ep_out = 0x02;
        cdc.ep_in = 0x83;

        const auto spec = usb::spec::msc_cdc_device(device, msc, cdc);
        const auto model = usb::build(spec);
        const auto plan = usb::plan::build(model);
        if (!plan) {
            std::fprintf(stream, "[ERR] plan build failed err=%d\n", static_cast<int>(plan.error()));
            return false;
        }

        const auto runtime = usb::runtime::host_mock(
            session.dcd_ops(),
            &session,
            &session.adapter(),
            usb::runtime::MscReadyHook{&msc_on_ready, &demo});

        auto binding = usb::runtime::make(plan.value(), registry, runtime);
        const auto init = decltype(binding)::init_trampoline(&binding);
        if (!init) {
            std::fprintf(stream, "[ERR] binding init failed err=%d\n", static_cast<int>(init.error()));
            return false;
        }

        if (!usb::fixture::expect(demo.ready, "msc ready hook not called", stream)) return false;
        if (!usb::fixture::expect(binding.cdc.has_value(), "cdc instance missing", stream)) return false;

        constexpr char kPing[] = "ping";
        constexpr char kPong[] = "pong";
        std::memcpy(binding.cdc_tx_buf.data(), kPong, 4);
        binding.cdc_tx_len = 4;

        const auto msc_cfg = plan.value().msc.msc_cfg;
        const auto cdc_cfg = plan.value().cdc.cdc_cfg;
        MscCdcPumpContext pump{demo.bot, msc_cfg, &(*binding.cdc), cdc_cfg, false};
        if (!usb::fixture::run_replay_file(session, trace_path, usb::replay::Hooks{&msc_cdc_pump_in, &pump}, stream)) {
            return false;
        }

        if (!usb::fixture::expect(session.address() == 9, "device address not applied", stream)) return false;
        if (!usb::fixture::expect(session.configured(), "device not configured", stream)) return false;
        if (!usb::fixture::expect(session.endpoint_state(msc_cfg.ep_out).opened, "msc bulk out endpoint not opened", stream)) return false;
        if (!usb::fixture::expect(session.endpoint_state(msc_cfg.ep_in).opened, "msc bulk in endpoint not opened", stream)) return false;
        if (!usb::fixture::expect(session.endpoint_state(cdc_cfg.ep_notify).opened, "cdc notify endpoint not opened", stream)) return false;
        if (!usb::fixture::expect(session.endpoint_state(cdc_cfg.ep_out).opened, "cdc bulk out endpoint not opened", stream)) return false;
        if (!usb::fixture::expect(session.endpoint_state(cdc_cfg.ep_in).opened, "cdc bulk in endpoint not opened", stream)) return false;
        if (!usb::fixture::expect(binding.cdc->line_coding().baud == 921600, "cdc line coding callback mismatch", stream)) return false;
        if (!usb::fixture::expect(binding.cdc->control_line_state() == 0x0003, "cdc control line state mismatch", stream)) return false;
        if (!usb::fixture::expect(binding.cdc_rx_len == 4, "cdc bulk out length mismatch", stream)) return false;
        if (!usb::fixture::expect(std::memcmp(binding.cdc_rx_buf.data(), kPing, 4) == 0, "cdc bulk out payload mismatch", stream)) return false;
        if (!usb::fixture::expect(binding.cdc_tx_len == 0, "cdc tx length not drained", stream)) return false;
        if (!usb::fixture::expect(pump.notify_sent, "cdc notify not sent", stream)) return false;
        return true;
    }

    bool run_case(void*,
                  std::string_view case_name,
                  std::string_view trace_path,
                  std::FILE* stream) noexcept {
        if (case_name.starts_with("msc-cdc-")) {
            return run_msc_cdc_case(trace_path, stream);
        }
        if (case_name.starts_with("cdc-")) {
            return run_cdc_case(trace_path, stream);
        }
        if (case_name.starts_with("msc-")) {
            return run_msc_case(case_name, trace_path, stream);
        }
        std::fprintf(stream, "[ERR] unknown suite case '%.*s'\n",
                     static_cast<int>(case_name.size()),
                     case_name.data());
        return false;
    }
}

int main() {
    return usb::fixture::run_suite_file(
        USB_REPLAY_SUITE_PATH,
        usb::fixture::ManifestHooks{&run_case, nullptr});
}
