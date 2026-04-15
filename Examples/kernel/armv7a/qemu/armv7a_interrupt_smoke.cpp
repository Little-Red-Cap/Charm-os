#include "armv7a_interrupt_smoke.hpp"

#include <cstddef>

#include "armv7a_cpu.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_handler_stack.hpp"
#include "armv7a_interrupt_diagnostics.hpp"
#include "armv7a_platform.hpp"

namespace {
constexpr std::size_t kObservationSlotCount = 5u;

struct Armv7aInterruptStoredObservation {
    bool seen = false;
    bool special = false;
    bool synthetic = false;
    unsigned int intid = 0u;
    std::uint32_t raw_acknowledge = 0u;
    std::uint32_t controller_distributor_control = 0u;
    std::uint32_t controller_cpu_control = 0u;
    std::uint32_t controller_priority_mask = 0u;
    std::uint32_t controller_binary_point = 0u;
    std::uint32_t controller_highest_pending = 0u;
    unsigned int controller_highest_pending_intid = 0u;
    bool controller_highest_pending_special = true;
    std::uint32_t line_group_bank = 0u;
    std::uint32_t line_enabled_bank = 0u;
    std::uint32_t line_pending_bank = 0u;
    std::uint32_t line_active_bank = 0u;
    bool line_group1 = false;
    bool line_enabled = false;
    bool line_pending = false;
    bool line_active = false;
    std::uint32_t handler_cpsr = 0u;
    std::uint32_t handler_spsr = 0u;
    std::uint32_t return_pc = 0u;
};

volatile unsigned int g_interrupt_count = 0;
volatile Armv7aInterruptSmokeKind g_interrupt_smoke_kind = Armv7aInterruptSmokeKind::kNone;
volatile Armv7aInterruptStoredObservation g_last_observation{};
volatile Armv7aInterruptStoredObservation g_observations[kObservationSlotCount]{};

std::size_t observation_index(Armv7aInterruptSmokeKind kind)
{
    return static_cast<std::size_t>(kind);
}

void clear_stored_observation(volatile Armv7aInterruptStoredObservation& observation)
{
    observation.seen = false;
    observation.special = false;
    observation.synthetic = false;
    observation.intid = armv7a_platform_spurious_interrupt_id();
    observation.raw_acknowledge = 0u;
    observation.controller_distributor_control = 0u;
    observation.controller_cpu_control = 0u;
    observation.controller_priority_mask = 0u;
    observation.controller_binary_point = 0u;
    observation.controller_highest_pending = 0u;
    observation.controller_highest_pending_intid = armv7a_platform_spurious_interrupt_id();
    observation.controller_highest_pending_special = true;
    observation.line_group_bank = 0u;
    observation.line_enabled_bank = 0u;
    observation.line_pending_bank = 0u;
    observation.line_active_bank = 0u;
    observation.line_group1 = false;
    observation.line_enabled = false;
    observation.line_pending = false;
    observation.line_active = false;
    observation.handler_cpsr = 0u;
    observation.handler_spsr = 0u;
    observation.return_pc = 0u;
}

Armv7aInterruptObservation load_observation(
    const volatile Armv7aInterruptStoredObservation& observation)
{
    return Armv7aInterruptObservation{
        .seen = observation.seen,
        .special = observation.special,
        .synthetic = observation.synthetic,
        .intid = observation.intid,
        .raw_acknowledge = observation.raw_acknowledge,
        .controller =
            Armv7aPlatformInterruptControllerState{
                .distributor_control = observation.controller_distributor_control,
                .cpu_control = observation.controller_cpu_control,
                .priority_mask = observation.controller_priority_mask,
                .binary_point = observation.controller_binary_point,
                .highest_pending = observation.controller_highest_pending,
                .highest_pending_intid = observation.controller_highest_pending_intid,
                .highest_pending_special = observation.controller_highest_pending_special,
            },
        .line =
            Armv7aPlatformInterruptLineState{
                .intid = observation.intid,
                .group = observation.line_group_bank,
                .enabled = observation.line_enabled_bank,
                .pending = observation.line_pending_bank,
                .active = observation.line_active_bank,
                .line_group1 = observation.line_group1,
                .line_enabled = observation.line_enabled,
                .line_pending = observation.line_pending,
                .line_active = observation.line_active,
            },
        .handler_cpsr = observation.handler_cpsr,
        .handler_spsr = observation.handler_spsr,
        .return_pc = observation.return_pc,
    };
}

void clear_observation(Armv7aInterruptSmokeKind kind)
{
    const auto index = observation_index(kind);
    if (index >= kObservationSlotCount) {
        return;
    }

    clear_stored_observation(g_observations[index]);
}

void store_observation(volatile Armv7aInterruptStoredObservation& observation,
                       const Armv7aPlatformInterruptAcknowledge& acknowledge,
                       const Armv7aPlatformInterruptControllerState& controller,
                       const Armv7aPlatformInterruptLineState& line,
                       const Armv7aExceptionFrame& frame,
                       bool synthetic)
{
    observation.seen = true;
    observation.special = acknowledge.special;
    observation.synthetic = synthetic;
    observation.intid = acknowledge.intid;
    observation.raw_acknowledge = acknowledge.raw;
    observation.controller_distributor_control = controller.distributor_control;
    observation.controller_cpu_control = controller.cpu_control;
    observation.controller_priority_mask = controller.priority_mask;
    observation.controller_binary_point = controller.binary_point;
    observation.controller_highest_pending = controller.highest_pending;
    observation.controller_highest_pending_intid = controller.highest_pending_intid;
    observation.controller_highest_pending_special = controller.highest_pending_special;
    observation.line_group_bank = line.group;
    observation.line_enabled_bank = line.enabled;
    observation.line_pending_bank = line.pending;
    observation.line_active_bank = line.active;
    observation.line_group1 = line.line_group1;
    observation.line_enabled = line.line_enabled;
    observation.line_pending = line.line_pending;
    observation.line_active = line.line_active;
    observation.handler_cpsr = armv7a_read_cpsr();
    observation.handler_spsr = frame.spsr;
    observation.return_pc = armv7a_exception_return_pc(frame);
}

void record_interrupt(const Armv7aPlatformInterruptAcknowledge& acknowledge,
                      const Armv7aPlatformInterruptControllerState& controller,
                      const Armv7aPlatformInterruptLineState& line,
                      const Armv7aExceptionFrame& frame,
                      bool synthetic)
{
    store_observation(g_last_observation, acknowledge, controller, line, frame, synthetic);
    if (!acknowledge.special) {
        g_interrupt_count = 1u;
    }

    const auto index = observation_index(g_interrupt_smoke_kind);
    if (index >= kObservationSlotCount || g_interrupt_smoke_kind == Armv7aInterruptSmokeKind::kNone) {
        return;
    }

    store_observation(g_observations[index], acknowledge, controller, line, frame, synthetic);
}

bool interrupt_matches_expected(unsigned int intid, bool fiq_route)
{
    switch (g_interrupt_smoke_kind) {
    case Armv7aInterruptSmokeKind::kTimerIrq:
        return !fiq_route && armv7a_platform_is_timer_interrupt(intid);
    case Armv7aInterruptSmokeKind::kSgiIrq:
        return !fiq_route && armv7a_platform_is_self_sgi_interrupt(intid);
    case Armv7aInterruptSmokeKind::kSgiFiq:
        return fiq_route && armv7a_platform_is_self_sgi_interrupt(intid);
    case Armv7aInterruptSmokeKind::kSpecialIrq:
        return !fiq_route && armv7a_platform_is_special_interrupt(intid);
    case Armv7aInterruptSmokeKind::kNone:
    default:
        return false;
    }
}

void handle_interrupt(Armv7aExceptionFrame* frame,
                      const char* label,
                      bool fiq_route,
                      bool synthetic)
{
    const auto controller_before_ack = armv7a_platform_interrupt_controller_state();
    const auto acknowledge = armv7a_platform_acknowledge_interrupt();
    const auto intid = acknowledge.intid;

    const auto line = armv7a_platform_interrupt_line_state(intid);

    if (acknowledge.special) {
        record_interrupt(acknowledge, controller_before_ack, line, *frame, synthetic);
        if (!synthetic) {
            armv7a_print_handler_stack_evidence(fiq_route ? "fiq" : "irq", armv7a_read_cpsr());
        }
        armv7a_interrupt_print_special_ack(
            fiq_route ? "ARMv7-A special FIQ acknowledge"
                      : "ARMv7-A special IRQ acknowledge",
            fiq_route ? Armv7aPlatformInterruptRoute::kFiq
                      : Armv7aPlatformInterruptRoute::kIrq,
            load_observation(g_last_observation));
        return;
    }

    if (!fiq_route) {
        armv7a_platform_timer_stop();
    }

    record_interrupt(acknowledge, controller_before_ack, line, *frame, synthetic);
    armv7a_print_handler_stack_evidence(fiq_route ? "fiq" : "irq", armv7a_read_cpsr());
    if (!interrupt_matches_expected(intid, fiq_route)) {
        armv7a_interrupt_print_unexpected(label, intid, *frame);
    }

    armv7a_platform_complete_interrupt(acknowledge.raw);
}
} // namespace

void armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind kind)
{
    g_interrupt_count = 0;
    g_interrupt_smoke_kind = kind;
    clear_stored_observation(g_last_observation);
    clear_observation(kind);
}

void armv7a_interrupt_smoke_finish()
{
    g_interrupt_smoke_kind = Armv7aInterruptSmokeKind::kNone;
}

bool armv7a_interrupt_smoke_seen()
{
    return g_interrupt_count != 0u;
}

Armv7aInterruptObservation armv7a_interrupt_smoke_last_observation()
{
    return load_observation(g_last_observation);
}

Armv7aInterruptObservation armv7a_interrupt_smoke_observation(Armv7aInterruptSmokeKind kind)
{
    const auto index = observation_index(kind);
    if (index >= kObservationSlotCount) {
        return Armv7aInterruptObservation{
            .seen = false,
            .special = false,
            .synthetic = false,
            .intid = armv7a_platform_spurious_interrupt_id(),
            .raw_acknowledge = 0u,
            .controller =
                Armv7aPlatformInterruptControllerState{
                    .highest_pending_intid = armv7a_platform_spurious_interrupt_id(),
                    .highest_pending_special = true,
                },
            .line =
                Armv7aPlatformInterruptLineState{
                    .intid = armv7a_platform_spurious_interrupt_id(),
                },
            .handler_cpsr = 0u,
            .handler_spsr = 0u,
            .return_pc = 0u,
        };
    }

    return load_observation(g_observations[index]);
}

void armv7a_handle_irq_synthetic(Armv7aExceptionFrame* frame)
{
    handle_interrupt(frame, "IRQ", false, true);
}

extern "C" void armv7a_handle_irq(Armv7aExceptionFrame* frame)
{
    handle_interrupt(frame, "IRQ", false, false);
}

extern "C" void armv7a_handle_fiq(Armv7aExceptionFrame* frame)
{
    handle_interrupt(frame, "FIQ", true, false);
}
