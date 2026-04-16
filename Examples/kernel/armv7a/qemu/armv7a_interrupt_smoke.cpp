#include "armv7a_interrupt_smoke.hpp"

#include <cstddef>

#include "armv7a_cpu.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_handler_stack.hpp"
#include "armv7a_interrupt_diagnostics.hpp"
#include "armv7a_platform.hpp"

namespace {
constexpr std::size_t kObservationSlotCount = 8u;

struct Armv7aInterruptStoredObservation {
    bool entry_seen = false;
    bool completion_seen = false;
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
    std::uint32_t completion_controller_distributor_control = 0u;
    std::uint32_t completion_controller_cpu_control = 0u;
    std::uint32_t completion_controller_priority_mask = 0u;
    std::uint32_t completion_controller_binary_point = 0u;
    std::uint32_t completion_controller_highest_pending = 0u;
    unsigned int completion_controller_highest_pending_intid = 0u;
    bool completion_controller_highest_pending_special = true;
    std::uint32_t completion_line_group_bank = 0u;
    std::uint32_t completion_line_enabled_bank = 0u;
    std::uint32_t completion_line_pending_bank = 0u;
    std::uint32_t completion_line_active_bank = 0u;
    bool completion_line_group1 = false;
    bool completion_line_enabled = false;
    bool completion_line_pending = false;
    bool completion_line_active = false;
    std::uint32_t entry_handler_psr = 0u;
    std::uint32_t entry_origin_psr = 0u;
    std::uint32_t entry_return_pc = 0u;
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
    observation.entry_seen = false;
    observation.completion_seen = false;
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
    observation.completion_controller_distributor_control = 0u;
    observation.completion_controller_cpu_control = 0u;
    observation.completion_controller_priority_mask = 0u;
    observation.completion_controller_binary_point = 0u;
    observation.completion_controller_highest_pending = 0u;
    observation.completion_controller_highest_pending_intid =
        armv7a_platform_spurious_interrupt_id();
    observation.completion_controller_highest_pending_special = true;
    observation.completion_line_group_bank = 0u;
    observation.completion_line_enabled_bank = 0u;
    observation.completion_line_pending_bank = 0u;
    observation.completion_line_active_bank = 0u;
    observation.completion_line_group1 = false;
    observation.completion_line_enabled = false;
    observation.completion_line_pending = false;
    observation.completion_line_active = false;
    observation.entry_handler_psr = 0u;
    observation.entry_origin_psr = 0u;
    observation.entry_return_pc = 0u;
}

Armv7aInterruptObservation load_observation(
    const volatile Armv7aInterruptStoredObservation& observation)
{
    return Armv7aInterruptObservation{
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
        .entry =
            observation.entry_seen
                ? armv7a_make_vector_entry_observation(observation.entry_origin_psr,
                                                       observation.entry_handler_psr,
                                                       observation.entry_return_pc)
                : armv7a_make_unobserved_vector_entry(),
    };
}

Armv7aInterruptCompletionObservation load_completion(
    const volatile Armv7aInterruptStoredObservation& observation)
{
    const auto delivery = load_observation(observation);
    if (!observation.completion_seen) {
        return armv7a_make_unobserved_interrupt_completion(
            armv7a_platform_spurious_interrupt_id());
    }

    return armv7a_make_interrupt_completion_observation(
        delivery,
        Armv7aPlatformInterruptControllerState{
            .distributor_control = observation.completion_controller_distributor_control,
            .cpu_control = observation.completion_controller_cpu_control,
            .priority_mask = observation.completion_controller_priority_mask,
            .binary_point = observation.completion_controller_binary_point,
            .highest_pending = observation.completion_controller_highest_pending,
            .highest_pending_intid = observation.completion_controller_highest_pending_intid,
            .highest_pending_special = observation.completion_controller_highest_pending_special,
        },
        Armv7aPlatformInterruptLineState{
            .intid = observation.intid,
            .group = observation.completion_line_group_bank,
            .enabled = observation.completion_line_enabled_bank,
            .pending = observation.completion_line_pending_bank,
            .active = observation.completion_line_active_bank,
            .line_group1 = observation.completion_line_group1,
            .line_enabled = observation.completion_line_enabled,
            .line_pending = observation.completion_line_pending,
            .line_active = observation.completion_line_active,
        });
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
    observation.entry_seen = true;
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
    observation.entry_handler_psr = armv7a_read_cpsr();
    observation.entry_origin_psr = frame.spsr;
    observation.entry_return_pc = armv7a_exception_return_pc(frame);
}

void store_completion(volatile Armv7aInterruptStoredObservation& observation,
                      const Armv7aPlatformInterruptControllerState& controller_after_eoi,
                      const Armv7aPlatformInterruptLineState& line_after_eoi)
{
    observation.completion_seen = true;
    observation.completion_controller_distributor_control =
        controller_after_eoi.distributor_control;
    observation.completion_controller_cpu_control = controller_after_eoi.cpu_control;
    observation.completion_controller_priority_mask = controller_after_eoi.priority_mask;
    observation.completion_controller_binary_point = controller_after_eoi.binary_point;
    observation.completion_controller_highest_pending = controller_after_eoi.highest_pending;
    observation.completion_controller_highest_pending_intid =
        controller_after_eoi.highest_pending_intid;
    observation.completion_controller_highest_pending_special =
        controller_after_eoi.highest_pending_special;
    observation.completion_line_group_bank = line_after_eoi.group;
    observation.completion_line_enabled_bank = line_after_eoi.enabled;
    observation.completion_line_pending_bank = line_after_eoi.pending;
    observation.completion_line_active_bank = line_after_eoi.active;
    observation.completion_line_group1 = line_after_eoi.line_group1;
    observation.completion_line_enabled = line_after_eoi.line_enabled;
    observation.completion_line_pending = line_after_eoi.line_pending;
    observation.completion_line_active = line_after_eoi.line_active;
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

void record_completion(const Armv7aPlatformInterruptControllerState& controller_after_eoi,
                       const Armv7aPlatformInterruptLineState& line_after_eoi)
{
    store_completion(g_last_observation, controller_after_eoi, line_after_eoi);

    const auto index = observation_index(g_interrupt_smoke_kind);
    if (index >= kObservationSlotCount || g_interrupt_smoke_kind == Armv7aInterruptSmokeKind::kNone) {
        return;
    }

    store_completion(g_observations[index], controller_after_eoi, line_after_eoi);
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
    case Armv7aInterruptSmokeKind::kSgiIrqTimeout:
    case Armv7aInterruptSmokeKind::kUnexpectedIrq:
    case Armv7aInterruptSmokeKind::kSgiFiqTimeout:
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
    const auto observation = load_observation(g_last_observation);
    if (!interrupt_matches_expected(intid, fiq_route)) {
        armv7a_interrupt_print_unexpected(label, observation, *frame);
    }

    armv7a_platform_complete_interrupt(acknowledge.raw);
    record_completion(armv7a_platform_interrupt_controller_state(),
                      armv7a_platform_interrupt_line_state(intid));
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
        return armv7a_make_unobserved_interrupt_observation(
            armv7a_platform_spurious_interrupt_id());
    }

    return load_observation(g_observations[index]);
}

Armv7aInterruptCompletionObservation armv7a_interrupt_smoke_last_completion()
{
    return load_completion(g_last_observation);
}

Armv7aInterruptCompletionObservation armv7a_interrupt_smoke_completion(
    Armv7aInterruptSmokeKind kind)
{
    const auto index = observation_index(kind);
    if (index >= kObservationSlotCount) {
        return armv7a_make_unobserved_interrupt_completion(
            armv7a_platform_spurious_interrupt_id());
    }

    return load_completion(g_observations[index]);
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
