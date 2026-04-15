#include "armv7a_platform.hpp"

#include "armv7a_arch_timer.hpp"
#include "armv7a_gic.hpp"

namespace {
constexpr std::uint32_t kTimerCtrlEnable = 1u << 0;
constexpr std::uint32_t kTimerCtrlItMask = 1u << 1;
constexpr std::uint32_t kArmv7aGicIntIdMask = 0x3ffu;

Armv7aGicInterruptGroup to_gic_group(Armv7aPlatformInterruptRoute route)
{
    return route == Armv7aPlatformInterruptRoute::kFiq
               ? Armv7aGicInterruptGroup::kGroup0
               : Armv7aGicInterruptGroup::kGroup1;
}

bool line_bank_bit(std::uint32_t bank, unsigned int intid)
{
    return ((bank >> (intid % 32u)) & 1u) != 0u;
}

Armv7aPlatformInterruptLineState to_platform_state(unsigned int intid, const Armv7aGicLineState& state)
{
    return Armv7aPlatformInterruptLineState{
        .intid = intid,
        .group = state.igroupr,
        .enabled = state.isenabler,
        .pending = state.ispendr,
        .active = state.isactiver,
        .line_group1 = line_bank_bit(state.igroupr, intid),
        .line_enabled = line_bank_bit(state.isenabler, intid),
        .line_pending = line_bank_bit(state.ispendr, intid),
        .line_active = line_bank_bit(state.isactiver, intid),
    };
}
} // namespace

std::uint32_t armv7a_platform_timer_frequency_hz()
{
    return armv7a_timer_read_cntfrq();
}

std::uint64_t armv7a_platform_timer_counter()
{
    return armv7a_timer_read_cntpct();
}

std::uint32_t armv7a_platform_timer_control()
{
    return armv7a_timer_read_ctrl();
}

void armv7a_platform_timer_start_oneshot(std::uint32_t ticks)
{
    armv7a_platform_timer_stop();
    armv7a_timer_write_tval(ticks);
    armv7a_timer_write_ctrl(kTimerCtrlEnable);
}

void armv7a_platform_timer_stop()
{
    armv7a_timer_write_ctrl(kTimerCtrlItMask);
}

void armv7a_platform_prepare_timer_interrupt()
{
    armv7a_gic_init_timer_irq();
}

void armv7a_platform_release_timer_interrupt()
{
    armv7a_gic_disable_line(kArmv7aGicSecureTimerIntId);
    armv7a_gic_disable_line(kArmv7aGicNonSecureTimerIntId);
    armv7a_gic_clear_pending(kArmv7aGicSecureTimerIntId);
    armv7a_gic_clear_pending(kArmv7aGicNonSecureTimerIntId);
}

void armv7a_platform_prepare_self_sgi(Armv7aPlatformInterruptRoute route)
{
    armv7a_gic_init_sgi_irq(to_gic_group(route));
}

void armv7a_platform_release_self_sgi()
{
    armv7a_gic_disable_line(kArmv7aGicSelfSgiIntId);
    armv7a_gic_clear_pending(kArmv7aGicSelfSgiIntId);
    armv7a_gic_clear_sgi_pending(kArmv7aGicSelfSgiIntId);
}

void armv7a_platform_enable_interrupt_controller(Armv7aPlatformInterruptRoute route)
{
    armv7a_gic_enable_interfaces(route == Armv7aPlatformInterruptRoute::kFiq);
}

void armv7a_platform_disable_interrupt_controller()
{
    armv7a_gic_disable_interfaces();
}

void armv7a_platform_trigger_self_sgi()
{
    armv7a_gic_send_self_sgi(kArmv7aGicSelfSgiIntId);
}

Armv7aPlatformInterruptAcknowledge armv7a_platform_acknowledge_interrupt()
{
    const auto raw = armv7a_gic_acknowledge_irq();
    const auto intid = raw & 0x3ffu;
    return Armv7aPlatformInterruptAcknowledge{
        .raw = raw,
        .intid = intid,
        .special = armv7a_platform_is_special_interrupt(intid),
    };
}

void armv7a_platform_complete_interrupt(std::uint32_t raw_acknowledge)
{
    armv7a_gic_end_irq(raw_acknowledge);
}

Armv7aPlatformInterruptLineState armv7a_platform_secure_timer_interrupt_line_state()
{
    return to_platform_state(
        kArmv7aGicSecureTimerIntId, armv7a_gic_read_line_state(kArmv7aGicSecureTimerIntId));
}

Armv7aPlatformInterruptLineState armv7a_platform_nonsecure_timer_interrupt_line_state()
{
    return to_platform_state(kArmv7aGicNonSecureTimerIntId,
                             armv7a_gic_read_line_state(kArmv7aGicNonSecureTimerIntId));
}

Armv7aPlatformInterruptLineState armv7a_platform_self_sgi_line_state()
{
    return to_platform_state(
        kArmv7aGicSelfSgiIntId, armv7a_gic_read_line_state(kArmv7aGicSelfSgiIntId));
}

Armv7aPlatformInterruptControllerState armv7a_platform_interrupt_controller_state()
{
    const auto dist_state = armv7a_gic_read_distributor_state();
    const auto state = armv7a_gic_read_cpu_state();
    const auto highest_pending_intid = state.hppir & kArmv7aGicIntIdMask;
    return Armv7aPlatformInterruptControllerState{
        .distributor_control = dist_state.ctlr,
        .cpu_control = state.ctlr,
        .priority_mask = state.pmr,
        .binary_point = state.bpr,
        .highest_pending = state.hppir,
        .highest_pending_intid = highest_pending_intid,
        .highest_pending_special = armv7a_platform_is_special_interrupt(highest_pending_intid),
    };
}

unsigned int armv7a_platform_spurious_interrupt_id()
{
    return kArmv7aGicSpuriousIntId;
}

bool armv7a_platform_is_special_interrupt(unsigned int intid)
{
    return intid >= kArmv7aGicSpecialIntIdMin;
}

bool armv7a_platform_is_timer_interrupt(unsigned int intid)
{
    return armv7a_gic_is_timer_intid(intid);
}

bool armv7a_platform_is_self_sgi_interrupt(unsigned int intid)
{
    return armv7a_gic_is_sgi_intid(intid);
}

const char* armv7a_platform_timer_interrupt_route_name(unsigned int intid)
{
    switch (intid) {
    case kArmv7aGicSecureTimerIntId:
        return "secure-phys-ppi";
    case kArmv7aGicNonSecureTimerIntId:
        return "non-secure-phys-ppi";
    default:
        return "unexpected-intid";
    }
}
