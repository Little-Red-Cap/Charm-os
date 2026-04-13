#include <array>
#include <cstdio>
#include <span>

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
    constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm Host Harness");
    constexpr auto kSerialStr = usb::make_ascii_string_descriptor("0002");

    static const usb::StringTable<4> kStrings{
        std::array<std::span<const usb::u8>, 4>{
            std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
            std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
            std::span<const usb::u8>(kProductStr.data(), kProductStr.size()),
            std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
        }
    };

    struct DemoContext {
        std::array<usb::u8, 64> tx{};
        std::array<usb::u8, 64> rx{};
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

    std::size_t tx_length(void*) noexcept {
        return 0;
    }

    void on_rx_done(void*, std::size_t) noexcept {
    }

    void on_tx_done(void*, std::size_t) noexcept {
    }

    void on_ready(void* ctx,
                  usb::class_driver::CdcAcm*,
                  const usb::class_driver::CdcConfig*) noexcept {
        auto* demo = static_cast<DemoContext*>(ctx);
        if (!demo) return;
        demo->ready = true;
    }

    bool has_device_connect(std::span<const usb::mock::DeviceAction> actions, bool enable) {
        for (const auto& action : actions) {
            if (action.kind == usb::mock::DeviceActionKind::connect && action.flag == enable) {
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

    usb::spec::DeviceSpec device{};
    device.vendor_id = 0x1209;
    device.product_id = 0x0004;
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
    if (!expect(has_device_connect(session.device_actions(), true), "device connect action not observed")) return 1;

    session.clear_trace();
    session.signal_reset();
    if (!expect(session.host_events().size() == 1, "reset host event count mismatch")) return 1;
    if (!expect(session.host_events()[0].kind == usb::mock::HostEventKind::reset, "reset host event kind mismatch")) return 1;
    if (!expect(has_device_action(session.device_actions(), usb::mock::DeviceActionKind::set_address), "reset did not emit set_address")) return 1;
    if (!expect(has_device_action(session.device_actions(), usb::mock::DeviceActionKind::set_configured), "reset did not emit set_configured")) return 1;

    session.clear_trace();
    session.feed_setup(usb::SetupPacket{0x80, 0x06, 0x0100, 0x0000, 0x0040});
    if (!expect(session.host_events().size() == 1, "descriptor host event count mismatch")) return 1;
    if (!expect(session.host_events()[0].kind == usb::mock::HostEventKind::setup, "descriptor host event kind mismatch")) return 1;
    if (!expect(session.device_actions().size() == 1, "descriptor device action count mismatch")) return 1;
    if (!expect(session.device_actions()[0].kind == usb::mock::DeviceActionKind::send_in, "descriptor action kind mismatch")) return 1;
    if (!expect(session.device_actions()[0].ep == 0x80, "descriptor action endpoint mismatch")) return 1;
    if (!expect(session.device_actions()[0].data.size() == 18, "descriptor length mismatch")) return 1;
    auto pkt = session.poll_in();
    if (!expect(pkt.has_value(), "descriptor packet missing")) return 1;
    if (!expect(!pkt->zlp, "descriptor packet unexpectedly zlp")) return 1;
    if (!expect(session.ack_in(pkt->ep, pkt->data.size(), pkt->zlp), "descriptor ack failed")) return 1;
    if (!expect(session.host_events().size() == 2, "descriptor ack host event count mismatch")) return 1;
    if (!expect(session.host_events()[1].kind == usb::mock::HostEventKind::in_ack, "descriptor ack kind mismatch")) return 1;
    if (!expect(session.host_events()[1].size == 18, "descriptor ack size mismatch")) return 1;

    session.clear_trace();
    session.feed_setup(usb::SetupPacket{0x00, 0x05, 0x0005, 0x0000, 0x0000});
    if (!expect(session.address() == 0, "set address applied before status stage")) return 1;
    if (!expect(session.device_actions().size() == 1, "set address pre-ack action count mismatch")) return 1;
    if (!expect(session.device_actions()[0].kind == usb::mock::DeviceActionKind::send_zlp, "set address pre-ack action kind mismatch")) return 1;
    if (!expect(session.device_actions()[0].flag, "set address pre-ack zlp missing")) return 1;
    auto addr_pkt = session.poll_in();
    if (!expect(addr_pkt.has_value(), "set address status packet missing")) return 1;
    if (!expect(addr_pkt->zlp, "set address status packet not zlp")) return 1;
    if (!expect(session.address() == 0, "set address committed before in ack")) return 1;
    if (!expect(session.ack_in(addr_pkt->ep, addr_pkt->data.size(), addr_pkt->zlp), "set address ack failed")) return 1;
    if (!expect(session.host_events().size() == 2, "set address host event count mismatch")) return 1;
    if (!expect(session.host_events()[1].kind == usb::mock::HostEventKind::in_ack, "set address ack kind mismatch")) return 1;
    if (!expect(session.host_events()[1].flag, "set address ack zlp flag mismatch")) return 1;
    if (!expect(session.device_actions().size() == 2, "set address post-ack action count mismatch")) return 1;
    if (!expect(session.device_actions()[1].kind == usb::mock::DeviceActionKind::set_address, "set address commit action mismatch")) return 1;
    if (!expect(session.device_actions()[1].address == 5, "set address value mismatch")) return 1;
    if (!expect(session.address() == 5, "set address not committed after status stage")) return 1;

    session.clear_trace();
    session.feed_setup(usb::SetupPacket{0x00, 0x09, 0x0001, 0x0000, 0x0000});
    if (!expect(!session.configured(), "set configuration applied before status stage")) return 1;
    if (!expect(session.device_actions().size() == 1, "set configuration pre-ack action count mismatch")) return 1;
    if (!expect(session.device_actions()[0].kind == usb::mock::DeviceActionKind::send_zlp, "set configuration pre-ack action kind mismatch")) return 1;
    if (!expect(session.device_actions()[0].flag, "set configuration pre-ack zlp missing")) return 1;
    auto cfg_pkt = session.poll_in();
    if (!expect(cfg_pkt.has_value(), "set configuration status packet missing")) return 1;
    if (!expect(cfg_pkt->zlp, "set configuration status packet not zlp")) return 1;
    if (!expect(session.ack_in(cfg_pkt->ep, cfg_pkt->data.size(), cfg_pkt->zlp), "set configuration ack failed")) return 1;
    if (!expect(session.device_actions().size() >= 2, "set configuration post-ack action count mismatch")) return 1;
    const auto& cfg_commit = session.device_actions()[session.device_actions().size() - 1];
    if (!expect(cfg_commit.kind == usb::mock::DeviceActionKind::set_configured, "set configuration commit action mismatch")) return 1;
    if (!expect(cfg_commit.flag, "set configuration commit flag mismatch")) return 1;
    if (!expect(session.configured(), "set configuration not committed after status stage")) return 1;

    std::printf("[OK] usb-host-harness-smoke passed\n");
    std::printf("[state] address=%u configured=%d host_events=%zu device_actions=%zu\n",
                static_cast<unsigned>(session.address()),
                session.configured() ? 1 : 0,
                session.host_events().size(),
                session.device_actions().size());
    return 0;
}
