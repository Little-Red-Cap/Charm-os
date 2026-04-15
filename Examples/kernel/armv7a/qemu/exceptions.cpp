#include <cstdint>

#include "armv7a_cpu.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_fault_status.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_translation_walk.hpp"

namespace {
void platform_console_write_hex(std::uint32_t value, int digits)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
        armv7a_platform_early_console_putc(kHex[(value >> shift) & 0x0Fu]);
    }
}

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
        platform_console_write_hex(ifsr, 8);
        armv7a_platform_early_console_puts(", ifar=0x");
        platform_console_write_hex(fault_address, 8);
        armv7a_platform_early_console_puts(", aifsr=0x");
        platform_console_write_hex(armv7a_read_aifsr(), 8);
        armv7a_platform_early_console_puts("\r\n");
        armv7a_platform_early_console_puts("ARMv7-A prefetch fault decode, status=0x");
        platform_console_write_hex(decode.status_code, 2);
        armv7a_platform_early_console_puts(" (");
        armv7a_platform_early_console_puts(decode.description);
        armv7a_platform_early_console_puts("), domain=0x");
        platform_console_write_hex(decode.domain, 1);
        armv7a_platform_early_console_puts("\r\n");
        break;
    }
    case kArmv7aExceptionDataAbort:
    {
        const auto dfsr = armv7a_read_dfsr();
        const auto decode = armv7a_decode_data_fault_status(dfsr);
        fault_address = armv7a_read_dfar();
        armv7a_platform_early_console_puts("ARMv7-A data fault, dfsr=0x");
        platform_console_write_hex(dfsr, 8);
        armv7a_platform_early_console_puts(", dfar=0x");
        platform_console_write_hex(fault_address, 8);
        armv7a_platform_early_console_puts(", adfsr=0x");
        platform_console_write_hex(armv7a_read_adfsr(), 8);
        armv7a_platform_early_console_puts("\r\n");
        armv7a_platform_early_console_puts("ARMv7-A data fault decode, status=0x");
        platform_console_write_hex(decode.status_code, 2);
        armv7a_platform_early_console_puts(" (");
        armv7a_platform_early_console_puts(decode.description);
        armv7a_platform_early_console_puts("), domain=0x");
        platform_console_write_hex(decode.domain, 1);
        armv7a_platform_early_console_puts(", write=");
        armv7a_platform_early_console_puts(decode.write ? "yes" : "no");
        armv7a_platform_early_console_puts(", cm=");
        armv7a_platform_early_console_puts(decode.cache_maintenance ? "yes" : "no");
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
        platform_console_write_hex(fault_address, 8);
        armv7a_platform_early_console_puts(", ttbr0=0x");
        platform_console_write_hex(ttbr0, 8);
        armv7a_platform_early_console_puts(", l1[0x");
        platform_console_write_hex(decode.index, 3);
        armv7a_platform_early_console_puts("]=0x");
        platform_console_write_hex(decode.descriptor, 8);
        armv7a_platform_early_console_puts(" (");
        armv7a_platform_early_console_puts(armv7a_l1_descriptor_kind_name(decode.kind));
        armv7a_platform_early_console_puts(")");
        if (decode.kind == Armv7aL1DescriptorKind::kPageTable ||
            decode.kind == Armv7aL1DescriptorKind::kSection ||
            decode.kind == Armv7aL1DescriptorKind::kSupersection) {
            armv7a_platform_early_console_puts(", domain=0x");
            platform_console_write_hex(decode.domain, 1);
        }
        if (decode.kind == Armv7aL1DescriptorKind::kPageTable) {
            const auto l2_descriptor = armv7a_l2_descriptor_from_l1(descriptor, fault_address);
            const auto l2_decode = armv7a_decode_l2_descriptor(fault_address, l2_descriptor);

            armv7a_platform_early_console_puts(", l2[0x");
            platform_console_write_hex(l2_decode.index, 2);
            armv7a_platform_early_console_puts("]=0x");
            platform_console_write_hex(l2_decode.descriptor, 8);
            armv7a_platform_early_console_puts(" (");
            armv7a_platform_early_console_puts(armv7a_l2_descriptor_kind_name(l2_decode.kind));
            armv7a_platform_early_console_puts(")");
            if (l2_decode.kind == Armv7aL2DescriptorKind::kSmallPage ||
                l2_decode.kind == Armv7aL2DescriptorKind::kLargePage) {
                armv7a_platform_early_console_puts(", xn=");
                armv7a_platform_early_console_puts(l2_decode.execute_never ? "yes" : "no");
                armv7a_platform_early_console_puts(", s=");
                armv7a_platform_early_console_puts(l2_decode.shareable ? "yes" : "no");
                armv7a_platform_early_console_puts(", c=");
                armv7a_platform_early_console_puts(l2_decode.cacheable ? "yes" : "no");
                armv7a_platform_early_console_puts(", b=");
                armv7a_platform_early_console_puts(l2_decode.bufferable ? "yes" : "no");
                armv7a_platform_early_console_puts(", ap=0x");
                platform_console_write_hex(l2_decode.access_permission, 1);
            }
        } else if (decode.kind == Armv7aL1DescriptorKind::kSection ||
                   decode.kind == Armv7aL1DescriptorKind::kSupersection) {
            armv7a_platform_early_console_puts(", xn=");
            armv7a_platform_early_console_puts(decode.execute_never ? "yes" : "no");
            armv7a_platform_early_console_puts(", s=");
            armv7a_platform_early_console_puts(decode.shareable ? "yes" : "no");
            armv7a_platform_early_console_puts(", c=");
            armv7a_platform_early_console_puts(decode.cacheable ? "yes" : "no");
            armv7a_platform_early_console_puts(", b=");
            armv7a_platform_early_console_puts(decode.bufferable ? "yes" : "no");
            armv7a_platform_early_console_puts(", ap=0x");
            platform_console_write_hex(decode.access_permission, 1);
        }
        armv7a_platform_early_console_puts("\r\n");
    }

    armv7a_platform_early_console_puts("ARMv7-A fault context, sctlr=0x");
    platform_console_write_hex(armv7a_read_sctlr(), 8);
    armv7a_platform_early_console_puts(", ttbr0=0x");
    platform_console_write_hex(armv7a_read_ttbr0(), 8);
    armv7a_platform_early_console_puts(", ttbcr=0x");
    platform_console_write_hex(armv7a_read_ttbcr(), 8);
    armv7a_platform_early_console_puts(", dacr=0x");
    platform_console_write_hex(armv7a_read_dacr(), 8);
    armv7a_platform_early_console_puts("\r\n");
}
} // namespace

extern "C" void armv7a_handle_svc(Armv7aExceptionFrame* frame)
{
    const auto* instruction =
        reinterpret_cast<const std::uint32_t*>(armv7a_exception_pc(*frame));
    const auto immediate = *instruction & 0x00FFFFFFu;
    const auto current_cpsr = armv7a_read_cpsr();
    armv7a_platform_early_console_puts("ARMv7-A SVC vector active, imm=0x");
    platform_console_write_hex(immediate, 6);
    armv7a_platform_early_console_puts(", origin-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(frame->spsr));
    armv7a_platform_early_console_puts(", handler-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(current_cpsr));
    armv7a_platform_early_console_puts("\r\n");
}

extern "C" [[noreturn]] void armv7a_exception_fatal(const Armv7aExceptionFrame* frame)
{
    const auto kind = armv7a_exception_kind(*frame);
    armv7a_platform_early_console_init();
    const auto current_cpsr = armv7a_read_cpsr();
    armv7a_platform_early_console_puts("ARMv7-A exception: ");
    armv7a_platform_early_console_puts(exception_name(kind));
    armv7a_platform_early_console_puts(", pc=0x");
    platform_console_write_hex(armv7a_exception_pc(*frame), 8);
    armv7a_platform_early_console_puts(", lr=0x");
    platform_console_write_hex(frame->lr, 8);
    armv7a_platform_early_console_puts(", spsr=0x");
    platform_console_write_hex(frame->spsr, 8);
    armv7a_platform_early_console_puts(", origin-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(frame->spsr));
    armv7a_platform_early_console_puts(", current-cpsr=0x");
    platform_console_write_hex(current_cpsr, 8);
    armv7a_platform_early_console_puts(", current-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(current_cpsr));
    armv7a_platform_early_console_puts("\r\n");
    print_fault_registers(kind);
    armv7a_platform_idle_forever();
}
