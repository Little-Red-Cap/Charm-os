#include "armv7a_exception_diagnostics.hpp"

#include <cstdint>

#include "armv7a_bringup_phase.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_fault_observation_contract.hpp"
#include "armv7a_fault_status.hpp"
#include "armv7a_handler_stack.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_translation_walk.hpp"

namespace {
const char* exception_stack_tag_name(Armv7aExceptionKind kind)
{
    switch (kind) {
    case kArmv7aExceptionUndefined:
        return "undefined";
    case kArmv7aExceptionPrefetchAbort:
        return "prefetch-abort";
    case kArmv7aExceptionDataAbort:
        return "data-abort";
    case kArmv7aExceptionReserved:
        return "reserved";
    case kArmv7aExceptionIrq:
        return "irq";
    case kArmv7aExceptionFiq:
        return "fiq";
    case kArmv7aExceptionSvc:
        return "svc";
    default:
        return "unknown";
    }
}

Armv7aFaultRegistersSnapshot capture_prefetch_fault_registers()
{
    const auto syndrome = armv7a_read_ifsr();
    return Armv7aFaultRegistersSnapshot{
        .syndrome = syndrome,
        .fault_address = armv7a_read_ifar(),
        .aux_syndrome = armv7a_read_aifsr(),
        .decode = armv7a_decode_prefetch_fault_status(syndrome),
    };
}

Armv7aFaultRegistersSnapshot capture_data_fault_registers()
{
    const auto syndrome = armv7a_read_dfsr();
    return Armv7aFaultRegistersSnapshot{
        .syndrome = syndrome,
        .fault_address = armv7a_read_dfar(),
        .aux_syndrome = armv7a_read_adfsr(),
        .decode = armv7a_decode_data_fault_status(syndrome),
    };
}

Armv7aFaultMapSnapshot capture_fault_map(std::uint32_t fault_address)
{
    const auto ttbr0 = armv7a_read_ttbr0();
    const auto l1_descriptor = armv7a_l1_descriptor_from_ttbr0(ttbr0, fault_address);
    auto snapshot = Armv7aFaultMapSnapshot{
        .fault_address = fault_address,
        .ttbr0 = ttbr0,
        .l1 = armv7a_decode_l1_descriptor(fault_address, l1_descriptor),
    };
    if (armv7a_fault_map_uses_l2(snapshot.l1.kind)) {
        snapshot.l2_descriptor = armv7a_l2_descriptor_from_l1(l1_descriptor, fault_address);
        snapshot.l2 = armv7a_decode_l2_descriptor(fault_address, snapshot.l2_descriptor);
    }
    return snapshot;
}

Armv7aFaultContextSnapshot capture_fault_context()
{
    return Armv7aFaultContextSnapshot{
        .sctlr = armv7a_read_sctlr(),
        .ttbr0 = armv7a_read_ttbr0(),
        .ttbcr = armv7a_read_ttbcr(),
        .dacr = armv7a_read_dacr(),
    };
}

Armv7aFaultObservation capture_fault_observation(Armv7aExceptionKind kind)
{
    auto observation = Armv7aFaultObservation{
        .kind = kind,
        .context = capture_fault_context(),
    };
    switch (kind) {
    case kArmv7aExceptionPrefetchAbort:
        observation.registers_valid = true;
        observation.registers = capture_prefetch_fault_registers();
        break;
    case kArmv7aExceptionDataAbort:
        observation.registers_valid = true;
        observation.registers = capture_data_fault_registers();
        break;
    default:
        break;
    }

    if (observation.registers_valid && observation.registers.fault_address != 0u) {
        observation.map_valid = true;
        observation.map = capture_fault_map(observation.registers.fault_address);
    }
    return observation;
}

void print_fault_observation(const Armv7aFaultObservation& observation)
{
    if (observation.kind == kArmv7aExceptionPrefetchAbort &&
        armv7a_fault_observation_has_registers(observation)) {
        armv7a_platform_early_console_puts("ARMv7-A prefetch fault, ifsr=0x");
        armv7a_diag_put_hex(observation.registers.syndrome);
        armv7a_platform_early_console_puts(", ifar=0x");
        armv7a_diag_put_hex(observation.registers.fault_address);
        armv7a_platform_early_console_puts(", aifsr=0x");
        armv7a_diag_put_hex(observation.registers.aux_syndrome);
        armv7a_platform_early_console_puts("\r\n");
        armv7a_platform_early_console_puts("ARMv7-A prefetch fault decode, status=0x");
        armv7a_diag_put_hex(observation.registers.decode.status_code, 2);
        armv7a_platform_early_console_puts(" (");
        armv7a_platform_early_console_puts(observation.registers.decode.description);
        armv7a_platform_early_console_puts("), domain=0x");
        armv7a_diag_put_hex(observation.registers.decode.domain, 1);
        armv7a_platform_early_console_puts("\r\n");
    } else if (observation.kind == kArmv7aExceptionDataAbort &&
               armv7a_fault_observation_has_registers(observation)) {
        armv7a_platform_early_console_puts("ARMv7-A data fault, dfsr=0x");
        armv7a_diag_put_hex(observation.registers.syndrome);
        armv7a_platform_early_console_puts(", dfar=0x");
        armv7a_diag_put_hex(observation.registers.fault_address);
        armv7a_platform_early_console_puts(", adfsr=0x");
        armv7a_diag_put_hex(observation.registers.aux_syndrome);
        armv7a_platform_early_console_puts("\r\n");
        armv7a_platform_early_console_puts("ARMv7-A data fault decode, status=0x");
        armv7a_diag_put_hex(observation.registers.decode.status_code, 2);
        armv7a_platform_early_console_puts(" (");
        armv7a_platform_early_console_puts(observation.registers.decode.description);
        armv7a_platform_early_console_puts("), domain=0x");
        armv7a_diag_put_hex(observation.registers.decode.domain, 1);
        armv7a_platform_early_console_puts(", write=");
        armv7a_platform_early_console_puts(
            armv7a_diag_yes_no(observation.registers.decode.write));
        armv7a_platform_early_console_puts(", cm=");
        armv7a_platform_early_console_puts(
            armv7a_diag_yes_no(observation.registers.decode.cache_maintenance));
        armv7a_platform_early_console_puts("\r\n");
    }

    if (armv7a_fault_observation_has_map(observation)) {
        armv7a_platform_early_console_puts("ARMv7-A fault map, far=0x");
        armv7a_diag_put_hex(observation.map.fault_address);
        armv7a_platform_early_console_puts(", ttbr0=0x");
        armv7a_diag_put_hex(observation.map.ttbr0);
        armv7a_platform_early_console_puts(", l1[0x");
        armv7a_diag_put_hex(observation.map.l1.index, 3);
        armv7a_platform_early_console_puts("]=0x");
        armv7a_diag_put_hex(observation.map.l1.descriptor);
        armv7a_platform_early_console_puts(" (");
        armv7a_platform_early_console_puts(
            armv7a_l1_descriptor_kind_name(observation.map.l1.kind));
        armv7a_platform_early_console_puts(")");
        if (armv7a_fault_map_has_domain(observation.map.l1.kind)) {
            armv7a_platform_early_console_puts(", domain=0x");
            armv7a_diag_put_hex(observation.map.l1.domain, 1);
        }
        if (armv7a_fault_map_uses_l2(observation.map.l1.kind)) {
            armv7a_platform_early_console_puts(", l2[0x");
            armv7a_diag_put_hex(observation.map.l2.index, 2);
            armv7a_platform_early_console_puts("]=0x");
            armv7a_diag_put_hex(observation.map.l2.descriptor);
            armv7a_platform_early_console_puts(" (");
            armv7a_platform_early_console_puts(
                armv7a_l2_descriptor_kind_name(observation.map.l2.kind));
            armv7a_platform_early_console_puts(")");
            if (armv7a_fault_map_has_l2_attributes(observation.map.l2.kind)) {
                armv7a_platform_early_console_puts(", xn=");
                armv7a_platform_early_console_puts(
                    armv7a_diag_yes_no(observation.map.l2.execute_never));
                armv7a_platform_early_console_puts(", s=");
                armv7a_platform_early_console_puts(
                    armv7a_diag_yes_no(observation.map.l2.shareable));
                armv7a_platform_early_console_puts(", c=");
                armv7a_platform_early_console_puts(
                    armv7a_diag_yes_no(observation.map.l2.cacheable));
                armv7a_platform_early_console_puts(", b=");
                armv7a_platform_early_console_puts(
                    armv7a_diag_yes_no(observation.map.l2.bufferable));
                armv7a_platform_early_console_puts(", ap=0x");
                armv7a_diag_put_hex(observation.map.l2.access_permission, 1);
            }
        } else if (armv7a_fault_map_has_l1_attributes(observation.map.l1.kind)) {
            armv7a_platform_early_console_puts(", xn=");
            armv7a_platform_early_console_puts(
                armv7a_diag_yes_no(observation.map.l1.execute_never));
            armv7a_platform_early_console_puts(", s=");
            armv7a_platform_early_console_puts(
                armv7a_diag_yes_no(observation.map.l1.shareable));
            armv7a_platform_early_console_puts(", c=");
            armv7a_platform_early_console_puts(
                armv7a_diag_yes_no(observation.map.l1.cacheable));
            armv7a_platform_early_console_puts(", b=");
            armv7a_platform_early_console_puts(
                armv7a_diag_yes_no(observation.map.l1.bufferable));
            armv7a_platform_early_console_puts(", ap=0x");
            armv7a_diag_put_hex(observation.map.l1.access_permission, 1);
        }
        armv7a_platform_early_console_puts("\r\n");
    }

    armv7a_platform_early_console_puts("ARMv7-A fault context, sctlr=0x");
    armv7a_diag_put_hex(observation.context.sctlr);
    armv7a_platform_early_console_puts(", ttbr0=0x");
    armv7a_diag_put_hex(observation.context.ttbr0);
    armv7a_platform_early_console_puts(", ttbcr=0x");
    armv7a_diag_put_hex(observation.context.ttbcr);
    armv7a_platform_early_console_puts(", dacr=0x");
    armv7a_diag_put_hex(observation.context.dacr);
    armv7a_platform_early_console_puts("\r\n");
}
} // namespace

void armv7a_exception_print_svc_active(const Armv7aExceptionFrame& frame, unsigned int current_cpsr)
{
    const auto* instruction = reinterpret_cast<const std::uint32_t*>(armv7a_exception_pc(frame));
    const auto immediate = *instruction & 0x00FFFFFFu;

    armv7a_platform_early_console_puts("ARMv7-A SVC vector active, imm=0x");
    armv7a_diag_put_hex(immediate, 6);
    armv7a_platform_early_console_puts(", origin-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(frame.spsr));
    armv7a_platform_early_console_puts(", handler-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(current_cpsr));
    armv7a_platform_early_console_puts(", return-pc=0x");
    armv7a_diag_put_hex(armv7a_exception_return_pc(frame));
    armv7a_platform_early_console_puts("\r\n");
    armv7a_print_handler_stack_evidence("svc", current_cpsr);
}

[[noreturn]] void armv7a_exception_print_fatal_and_halt(const Armv7aExceptionFrame& frame)
{
    const auto kind = armv7a_exception_kind(frame);
    const auto current_cpsr = armv7a_read_cpsr();

    armv7a_platform_early_console_init();
    armv7a_platform_early_console_puts("ARMv7-A exception: ");
    armv7a_platform_early_console_puts(armv7a_exception_name(kind));
    armv7a_platform_early_console_puts(", pc=0x");
    armv7a_diag_put_hex(armv7a_exception_pc(frame));
    armv7a_platform_early_console_puts(", lr=0x");
    armv7a_diag_put_hex(frame.lr);
    armv7a_platform_early_console_puts(", spsr=0x");
    armv7a_diag_put_hex(frame.spsr);
    armv7a_platform_early_console_puts(", origin-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(frame.spsr));
    armv7a_platform_early_console_puts(", current-cpsr=0x");
    armv7a_diag_put_hex(current_cpsr);
    armv7a_platform_early_console_puts(", current-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(current_cpsr));
    armv7a_platform_early_console_puts("\r\n");
    armv7a_platform_early_console_puts("ARMv7-A exception phase, stage=");
    armv7a_platform_early_console_puts(
        armv7a_bringup_phase_name(armv7a_current_bringup_phase()));
    armv7a_platform_early_console_puts(", last-complete=");
    armv7a_platform_early_console_puts(
        armv7a_bringup_phase_name(armv7a_last_completed_bringup_phase()));
    armv7a_platform_early_console_puts("\r\n");
    armv7a_print_handler_stack_evidence(exception_stack_tag_name(kind), current_cpsr);
    print_fault_observation(capture_fault_observation(kind));
    armv7a_platform_idle_forever();
}
