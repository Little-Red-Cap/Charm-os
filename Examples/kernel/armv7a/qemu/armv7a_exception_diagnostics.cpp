#include "armv7a_exception_diagnostics.hpp"

#include <cstdint>

#include "armv7a_bringup_phase.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_fault_status.hpp"
#include "armv7a_handler_stack.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_translation_walk.hpp"

namespace {
const char* exception_name(Armv7aExceptionKind kind)
{
    switch (kind) {
    case kArmv7aExceptionUndefined:
        return "undefined";
    case kArmv7aExceptionPrefetchAbort:
        return "prefetch abort";
    case kArmv7aExceptionDataAbort:
        return "data abort";
    case kArmv7aExceptionReserved:
        return "reserved vector";
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

void print_fault_registers(Armv7aExceptionKind kind)
{
    std::uint32_t fault_address = 0;
    switch (kind) {
    case kArmv7aExceptionPrefetchAbort:
    {
        const auto ifsr = armv7a_read_ifsr();
        const auto decode = armv7a_decode_prefetch_fault_status(ifsr);
        fault_address = armv7a_read_ifar();
        armv7a_platform_early_console_puts("ARMv7-A prefetch fault, ifsr=0x");
        armv7a_diag_put_hex(ifsr);
        armv7a_platform_early_console_puts(", ifar=0x");
        armv7a_diag_put_hex(fault_address);
        armv7a_platform_early_console_puts(", aifsr=0x");
        armv7a_diag_put_hex(armv7a_read_aifsr());
        armv7a_platform_early_console_puts("\r\n");
        armv7a_platform_early_console_puts("ARMv7-A prefetch fault decode, status=0x");
        armv7a_diag_put_hex(decode.status_code, 2);
        armv7a_platform_early_console_puts(" (");
        armv7a_platform_early_console_puts(decode.description);
        armv7a_platform_early_console_puts("), domain=0x");
        armv7a_diag_put_hex(decode.domain, 1);
        armv7a_platform_early_console_puts("\r\n");
        break;
    }
    case kArmv7aExceptionDataAbort:
    {
        const auto dfsr = armv7a_read_dfsr();
        const auto decode = armv7a_decode_data_fault_status(dfsr);
        fault_address = armv7a_read_dfar();
        armv7a_platform_early_console_puts("ARMv7-A data fault, dfsr=0x");
        armv7a_diag_put_hex(dfsr);
        armv7a_platform_early_console_puts(", dfar=0x");
        armv7a_diag_put_hex(fault_address);
        armv7a_platform_early_console_puts(", adfsr=0x");
        armv7a_diag_put_hex(armv7a_read_adfsr());
        armv7a_platform_early_console_puts("\r\n");
        armv7a_platform_early_console_puts("ARMv7-A data fault decode, status=0x");
        armv7a_diag_put_hex(decode.status_code, 2);
        armv7a_platform_early_console_puts(" (");
        armv7a_platform_early_console_puts(decode.description);
        armv7a_platform_early_console_puts("), domain=0x");
        armv7a_diag_put_hex(decode.domain, 1);
        armv7a_platform_early_console_puts(", write=");
        armv7a_platform_early_console_puts(armv7a_diag_yes_no(decode.write));
        armv7a_platform_early_console_puts(", cm=");
        armv7a_platform_early_console_puts(armv7a_diag_yes_no(decode.cache_maintenance));
        armv7a_platform_early_console_puts("\r\n");
        break;
    }
    default:
        break;
    }

    if (fault_address != 0u) {
        const auto ttbr0 = armv7a_read_ttbr0();
        const auto descriptor = armv7a_l1_descriptor_from_ttbr0(ttbr0, fault_address);
        const auto decode = armv7a_decode_l1_descriptor(fault_address, descriptor);

        armv7a_platform_early_console_puts("ARMv7-A fault map, far=0x");
        armv7a_diag_put_hex(fault_address);
        armv7a_platform_early_console_puts(", ttbr0=0x");
        armv7a_diag_put_hex(ttbr0);
        armv7a_platform_early_console_puts(", l1[0x");
        armv7a_diag_put_hex(decode.index, 3);
        armv7a_platform_early_console_puts("]=0x");
        armv7a_diag_put_hex(decode.descriptor);
        armv7a_platform_early_console_puts(" (");
        armv7a_platform_early_console_puts(armv7a_l1_descriptor_kind_name(decode.kind));
        armv7a_platform_early_console_puts(")");
        if (decode.kind == Armv7aL1DescriptorKind::kPageTable ||
            decode.kind == Armv7aL1DescriptorKind::kSection ||
            decode.kind == Armv7aL1DescriptorKind::kSupersection) {
            armv7a_platform_early_console_puts(", domain=0x");
            armv7a_diag_put_hex(decode.domain, 1);
        }
        if (decode.kind == Armv7aL1DescriptorKind::kPageTable) {
            const auto l2_descriptor = armv7a_l2_descriptor_from_l1(descriptor, fault_address);
            const auto l2_decode = armv7a_decode_l2_descriptor(fault_address, l2_descriptor);

            armv7a_platform_early_console_puts(", l2[0x");
            armv7a_diag_put_hex(l2_decode.index, 2);
            armv7a_platform_early_console_puts("]=0x");
            armv7a_diag_put_hex(l2_decode.descriptor);
            armv7a_platform_early_console_puts(" (");
            armv7a_platform_early_console_puts(armv7a_l2_descriptor_kind_name(l2_decode.kind));
            armv7a_platform_early_console_puts(")");
            if (l2_decode.kind == Armv7aL2DescriptorKind::kSmallPage ||
                l2_decode.kind == Armv7aL2DescriptorKind::kLargePage) {
                armv7a_platform_early_console_puts(", xn=");
                armv7a_platform_early_console_puts(
                    armv7a_diag_yes_no(l2_decode.execute_never));
                armv7a_platform_early_console_puts(", s=");
                armv7a_platform_early_console_puts(armv7a_diag_yes_no(l2_decode.shareable));
                armv7a_platform_early_console_puts(", c=");
                armv7a_platform_early_console_puts(armv7a_diag_yes_no(l2_decode.cacheable));
                armv7a_platform_early_console_puts(", b=");
                armv7a_platform_early_console_puts(armv7a_diag_yes_no(l2_decode.bufferable));
                armv7a_platform_early_console_puts(", ap=0x");
                armv7a_diag_put_hex(l2_decode.access_permission, 1);
            }
        } else if (decode.kind == Armv7aL1DescriptorKind::kSection ||
                   decode.kind == Armv7aL1DescriptorKind::kSupersection) {
            armv7a_platform_early_console_puts(", xn=");
            armv7a_platform_early_console_puts(armv7a_diag_yes_no(decode.execute_never));
            armv7a_platform_early_console_puts(", s=");
            armv7a_platform_early_console_puts(armv7a_diag_yes_no(decode.shareable));
            armv7a_platform_early_console_puts(", c=");
            armv7a_platform_early_console_puts(armv7a_diag_yes_no(decode.cacheable));
            armv7a_platform_early_console_puts(", b=");
            armv7a_platform_early_console_puts(armv7a_diag_yes_no(decode.bufferable));
            armv7a_platform_early_console_puts(", ap=0x");
            armv7a_diag_put_hex(decode.access_permission, 1);
        }
        armv7a_platform_early_console_puts("\r\n");
    }

    armv7a_platform_early_console_puts("ARMv7-A fault context, sctlr=0x");
    armv7a_diag_put_hex(armv7a_read_sctlr());
    armv7a_platform_early_console_puts(", ttbr0=0x");
    armv7a_diag_put_hex(armv7a_read_ttbr0());
    armv7a_platform_early_console_puts(", ttbcr=0x");
    armv7a_diag_put_hex(armv7a_read_ttbcr());
    armv7a_platform_early_console_puts(", dacr=0x");
    armv7a_diag_put_hex(armv7a_read_dacr());
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
    armv7a_platform_early_console_puts(exception_name(kind));
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
    armv7a_platform_early_console_puts("\r\n");
    armv7a_print_handler_stack_evidence(exception_stack_tag_name(kind), current_cpsr);
    print_fault_registers(kind);
    armv7a_platform_idle_forever();
}
