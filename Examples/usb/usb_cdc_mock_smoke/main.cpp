#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <vector>

import usb.class_cdc;
import usb.common;
import usb.mock;
import usb.model;
import usb.plan;
import usb.runtime;
import usb.spec;

namespace {
    constexpr usb::u16 kLangs[] = { 0x0409 };
    constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
    constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
    constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm CDC Mock");
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

    bool expect(bool cond, const char* message) {
        if (!cond) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    std::span<usb::u8> tx_buffer(void* ctx) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (!demo) return {};
        return std::span<usb::u8>(demo->tx.data(), demo->tx.size());
    }

    std::span<usb::u8> rx_buffer(void* ctx) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (!demo) return {};
        return std::span<usb::u8>(demo->rx.data(), demo->rx.size());
    }

    std::size_t tx_length(void* ctx) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        return demo ? demo->tx_len : 0;
    }

    void on_rx_done(void* ctx, std::size_t len) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (!demo) return;
        demo->rx_len = len;
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
        if (!demo) return;
        demo->last_line_coding = coding;
    }

    void on_control_line(void* ctx, usb::u16 value) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (!demo) return;
        demo->control_line_state = value;
    }

    void on_ready(void* ctx,
                  usb::class_driver::CdcAcm*,
                  const usb::class_driver::CdcConfig*) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (!demo) return;
        demo->ready = true;
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
    device.product_id = 0x0003;
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

    if (!expect(demo.ready, "cdc ready hook not called")) return 1;
    if (!expect(session.pullup_enabled(), "device pull-up not enabled")) return 1;

    session.signal_reset();

    const auto dev_desc = control_in(session, usb::SetupPacket{0x80, 0x06, 0x0100, 0x0000, 0x0040});
    if (!expect(dev_desc.size() == 18, "device descriptor length mismatch")) return 1;

    if (!control_out(session, usb::SetupPacket{0x00, 0x05, 0x0005, 0x0000, 0x0000})) {
        std::fprintf(stderr, "[ERR] set address failed\n");
        return 1;
    }
    if (!expect(session.address() == 5, "device address not applied")) return 1;

    const auto cfg9 = control_in(session, usb::SetupPacket{0x80, 0x06, 0x0200, 0x0000, 0x0009});
    if (!expect(cfg9.size() == 9, "short config descriptor length mismatch")) return 1;
    if (!expect(cfg9[2] == 0x4B && cfg9[3] == 0x00, "config total length mismatch")) return 1;

    const auto cfg_full = control_in(session, usb::SetupPacket{0x80, 0x06, 0x0200, 0x0000, 0x00FF});
    if (!expect(cfg_full.size() == 75, "full config descriptor length mismatch")) return 1;
    if (!expect(session.set_address_count() >= 1, "set address callback not observed")) return 1;

    if (!control_out(session, usb::SetupPacket{0x00, 0x09, 0x0001, 0x0000, 0x0000})) {
        std::fprintf(stderr, "[ERR] set configuration failed\n");
        return 1;
    }
    if (!expect(session.configured(), "device not configured")) return 1;
    if (!expect(session.endpoint_state(0x81).opened, "cdc notify endpoint not opened")) return 1;
    if (!expect(session.endpoint_state(0x01).opened, "cdc bulk out endpoint not opened")) return 1;
    if (!expect(session.endpoint_state(0x82).opened, "cdc bulk in endpoint not opened")) return 1;

    const auto line = control_in(session, usb::SetupPacket{0xA1, 0x21, 0x0000, 0x0000, 0x0007});
    if (!expect(line.size() == sizeof(usb::class_driver::CdcLineCoding), "line coding length mismatch")) return 1;

    usb::class_driver::CdcLineCoding new_line{};
    new_line.baud = 921600;
    new_line.stop_bits = 0;
    new_line.parity = 0;
    new_line.data_bits = 8;
    const auto new_line_bytes = std::span<const usb::u8>(
        reinterpret_cast<const usb::u8*>(&new_line),
        sizeof(new_line));
    if (!control_out(session, usb::SetupPacket{0x21, 0x20, 0x0000, 0x0000, 0x0007}, new_line_bytes)) {
        std::fprintf(stderr, "[ERR] set line coding failed\n");
        return 1;
    }
    if (!expect(demo.last_line_coding.baud == 921600, "line coding callback mismatch")) return 1;

    if (!control_out(session, usb::SetupPacket{0x21, 0x22, 0x0003, 0x0000, 0x0000})) {
        std::fprintf(stderr, "[ERR] set control line state failed\n");
        return 1;
    }
    if (!expect(demo.control_line_state == 0x0003, "control line state callback mismatch")) return 1;

    constexpr char kPing[] = "ping";
    if (!expect(session.feed_out(0x01,
                                 std::span<const usb::u8>(reinterpret_cast<const usb::u8*>(kPing), 4)),
                "bulk OUT feed failed")) return 1;
    if (!expect(demo.rx_len == 4, "cdc bulk out length mismatch")) return 1;
    if (!expect(std::memcmp(demo.rx.data(), kPing, 4) == 0, "cdc bulk out payload mismatch")) return 1;

    std::printf("[OK] usb-cdc-mock-smoke passed\n");
    std::printf("[state] address=%u configured=%d resets=%zu cfg_calls=%zu\n",
                static_cast<unsigned>(session.address()),
                session.configured() ? 1 : 0,
                session.reset_count(),
                session.set_configured_count());
    return 0;
}
