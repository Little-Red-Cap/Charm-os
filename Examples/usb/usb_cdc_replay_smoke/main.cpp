#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

import usb.class_cdc;
import usb.common;
import usb.device_driver;
import usb.fixture;
import usb.mock;
import usb.model;
import usb.plan;
import usb.replay;
import usb.runtime;
import usb.spec;

namespace {
    constexpr usb::u16 kLangs[] = { 0x0409 };
    constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
    constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
    constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm CDC Replay");
    constexpr auto kSerialStr = usb::make_ascii_string_descriptor("0001");

    static const usb::StringTable<4> kStrings{
        std::array<std::span<const usb::u8>, 4>{
            std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
            std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
            std::span<const usb::u8>(kProductStr.data(), kProductStr.size()),
            std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
        }
    };

    struct DemoContext {
        std::array<usb::u8, 256> tx{};
        std::array<usb::u8, 256> rx{};
        std::size_t tx_len{0};
        std::size_t rx_len{0};
        usb::class_driver::CdcLineCoding last_line_coding{};
        usb::u16 control_line_state{0};
        bool ready{false};
    };

    std::span<usb::u8> tx_buffer(void* ctx) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        return demo ? std::span<usb::u8>(demo->tx.data(), demo->tx.size()) : std::span<usb::u8>{};
    }

    std::span<usb::u8> rx_buffer(void* ctx) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        return demo ? std::span<usb::u8>(demo->rx.data(), demo->rx.size()) : std::span<usb::u8>{};
    }

    std::size_t tx_length(void* ctx) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        return demo ? demo->tx_len : 0;
    }

    void on_rx_done(void* ctx, std::size_t len) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (demo) demo->rx_len = len;
    }

    void on_tx_done(void* ctx, std::size_t len) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (!demo) return;
        if (len >= demo->tx_len) {
            demo->tx_len = 0;
            return;
        }
        std::memmove(demo->tx.data(), demo->tx.data() + len, demo->tx_len - len);
        demo->tx_len -= len;
    }

    void on_line_coding(void* ctx, const usb::class_driver::CdcLineCoding& coding) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (demo) demo->last_line_coding = coding;
    }

    void on_control_line(void* ctx, usb::u16 value) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (demo) demo->control_line_state = value;
    }

    void on_ready(void* ctx,
                  usb::class_driver::CdcAcm*,
                  const usb::class_driver::CdcConfig*) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (demo) demo->ready = true;
    }

    struct PumpContext {
        usb::class_driver::CdcAcm* cdc{nullptr};
    };

    bool pump_in(void* ctx, usb::mock::Session& session, usb::u8 ep) noexcept {
        auto* pump = static_cast<PumpContext*>(ctx);
        if (!pump || !pump->cdc) return false;
        if (ep != pump->cdc->config().ep_in) return false;
        return usb::device::examples::send_cdc_in_packet(
            session.dcd_ops(), &session, *pump->cdc, pump->cdc->config().ep_mps);
    }
}

int main() {
    DemoContext demo{};
    usb::mock::Session session{};

    usb::class_driver::CdcOps cdc_ops{};
    cdc_ops.tx_buffer = &tx_buffer;
    cdc_ops.rx_buffer = &rx_buffer;
    cdc_ops.tx_length = &tx_length;
    cdc_ops.on_rx_done = &on_rx_done;
    cdc_ops.on_tx_done = &on_tx_done;
    cdc_ops.on_line_coding = &on_line_coding;
    cdc_ops.on_control_line = &on_control_line;

    usb::spec::DeviceSpec device{};
    device.vendor_id = 0x1209;
    device.product_id = 0x0005;
    device.i_manufacturer = 1;
    device.i_product = 2;
    device.i_serial = 3;
    device.strings = std::span<const std::span<const usb::u8>>(kStrings.entries.data(), kStrings.entries.size());

    usb::spec::CdcFunctionSpec cdc{};
    cdc.cap_name = "usb.cdc0";

    const auto spec = usb::spec::cdc_device(device, cdc);
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
        {},
        usb::runtime::CdcRuntimeConfig{&demo, cdc_ops, usb::runtime::CdcReadyHook{&on_ready, &demo}});

    auto binding = usb::runtime::make(plan.value(), runtime);
    const auto init = decltype(binding)::init_trampoline(&binding);
    if (!init) {
        std::fprintf(stderr, "[ERR] binding init failed err=%d\n", static_cast<int>(init.error()));
        return 1;
    }

    if (!usb::fixture::expect(demo.ready, "cdc ready hook not called")) return 1;
    if (!usb::fixture::expect(binding.cdc.has_value(), "cdc instance missing")) return 1;

    const auto cdc_cfg = plan.value().cdc.cdc_cfg;
    constexpr char kPing[] = "ping";
    constexpr char kPong[] = "pong";
    std::memcpy(demo.tx.data(), kPong, 4);
    demo.tx_len = 4;

    PumpContext pump{&(*binding.cdc)};
    if (!usb::fixture::run_replay_file(
            session,
            USB_REPLAY_FIXTURE_PATH,
            usb::replay::Hooks{&pump_in, &pump})) {
        return 1;
    }

    if (!usb::fixture::expect(session.address() == 5, "device address not applied")) return 1;
    if (!usb::fixture::expect(session.configured(), "device not configured")) return 1;
    if (!usb::fixture::expect(session.endpoint_state(cdc_cfg.ep_notify).opened, "cdc notify endpoint not opened")) return 1;
    if (!usb::fixture::expect(session.endpoint_state(cdc_cfg.ep_out).opened, "cdc bulk out endpoint not opened")) return 1;
    if (!usb::fixture::expect(session.endpoint_state(cdc_cfg.ep_in).opened, "cdc bulk in endpoint not opened")) return 1;
    if (!usb::fixture::expect(demo.last_line_coding.baud == 921600, "line coding callback mismatch")) return 1;
    if (!usb::fixture::expect(demo.rx_len == 4, "cdc bulk out length mismatch")) return 1;
    if (!usb::fixture::expect(std::memcmp(demo.rx.data(), kPing, 4) == 0, "cdc bulk out payload mismatch")) return 1;
    if (!usb::fixture::expect(demo.tx_len == 0, "cdc tx length not drained")) return 1;

    std::printf("[OK] usb-cdc-replay-smoke passed\n");
    std::printf("[state] address=%u configured=%d resets=%zu cfg_calls=%zu\n",
                static_cast<unsigned>(session.address()),
                session.configured() ? 1 : 0,
                session.reset_count(),
                session.set_configured_count());
    return 0;
}
