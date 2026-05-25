#include "display_demo.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "display_service.hpp"
#include "port.h"
#include "power_service.hpp"
#include "console_service.hpp"

import out.core;
import out.format;

namespace h747::apps::display_demo {
namespace {

template <charm::cap::ByteSink Sink>
class OutSinkAdapter {
public:
    explicit OutSinkAdapter(Sink& sink) : sink_(&sink) {}

    out::result<std::size_t> write(const out::bytes bytes) noexcept {
        if (sink_ == nullptr) {
            return out::ok<std::size_t>(0U);
        }
        const auto transfer = sink_->write(bytes);
        return out::ok(static_cast<std::size_t>(transfer.bytes));
    }

    out::result<std::size_t> flush() noexcept {
        if (sink_ != nullptr) {
            (void)sink_->flush();
        }
        return out::ok<std::size_t>(0U);
    }

private:
    Sink* sink_{nullptr};
};

h747::console::ConsoleStream& console_stream() noexcept {
    static h747::console::ConsoleStream stream{};
    return stream;
}

OutSinkAdapter<h747::console::ConsoleStream>& out_sink() noexcept {
    static OutSinkAdapter adapter{console_stream()};
    return adapter;
}

template <out::fixed_string Fmt, class... Args>
void emit(Args&&... args) noexcept {
    out::discard(out::vprint<Fmt>(out_sink(), std::forward<Args>(args)...));
}

std::uint32_t last_tick = 0U;
std::uint32_t tick_count = 0U;
power::Pmic pmic;
display::MinimalPanel panel;

void print_power_status() {
    const auto snapshot = pmic.snapshot();
    const auto raw = snapshot.raw;
    emit<"power: ready={} transport={} dcdc1={}/{} ldo4={}/{} pgood=0x{:02X} wled={}/{} duty={}\n">(
        snapshot.ready(),
        snapshot.transport_text(),
        raw.dcdc1_enabled,
        raw.dcdc1_mv,
        raw.ldo4_enabled,
        raw.ldo4_mv,
        static_cast<unsigned>(raw.pgood_reg),
        raw.wled_enabled,
        raw.wled_fdim_hz,
        raw.wled_duty_percent);
}

void print_display_status() {
    const auto state = panel.state();
    const auto s = state.raw;
    emit<"display: panel_profile={} phase={} init_ok={} power_ready={} dcdc1_fix={}/{} wled_fix={}/{} panel_cmd_ok={} panel_cmd_fail={} last_cmd=0x{:02X} fail_cmd=0x{:02X} hal=0x{:08X} dsi_err=0x{:08X}\n">(
        state.panel_profile_text(),
        state.phase_text(),
        state.init_ok(),
        s.power_ready,
        s.dcdc1_repair_needed,
        s.dcdc1_repair_ok,
        s.wled_repair_needed,
        s.wled_repair_ok,
        s.panel_cmd_ok,
        s.panel_cmd_fail,
        s.last_cmd,
        s.fail_cmd,
        s.last_hal_status,
        s.last_dsi_error);
    emit<"display_regs: WCR=0x{:08X} WISR=0x{:08X} VMCR=0x{:08X} VPCR=0x{:08X} PCR=0x{:08X} ISR0=0x{:08X} ISR1=0x{:08X} LTDC_ISR=0x{:08X} WRPCR=0x{:08X} PSR=0x{:08X}\n">(
        s.wcr,
        s.wisr,
        s.vmcr,
        s.vpcr,
        s.pcr,
        s.isr0,
        s.isr1,
        s.ltdc_isr,
        s.wrpcr,
        s.psr);
}

} // namespace

void init() {
    pmic.init();
    (void)pmic.probe();

    emit<"display_demo: init role=minimal_hx8394d_red_screen\n">();
    print_power_status();

    const bool init_ok = panel.init();
    emit<"display_demo: panel_profile={} init_ok={}\n">(
        panel.state().panel_profile_text(),
        init_ok);

    if (init_ok) {
        (void)panel.fill_solid(display::Argb8888::red());
    }
    print_display_status();
    last_tick = h747::port::tick_ms();
}

void loop_once() noexcept {
    panel.poll();
    const std::uint32_t now = h747::port::tick_ms();
    if ((now - last_tick) >= 1000U) {
        last_tick = now;
        ++tick_count;
        emit<"display_demo: alive tick={} phase={}\n">(
            tick_count,
            panel.state().phase_text());
        print_display_status();
    }
}

} // namespace h747::apps::display_demo
