#include <cstdint>

#include "armv7a_cpu.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_fault_status.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_translation_walk.hpp"

extern "C" void early_uart_init();
extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);
extern "C" [[noreturn]] void charm_spin();

namespace {
void early_uart_write_hex(std::uint32_t value, int digits)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
        early_uart_putc(kHex[(value >> shift) & 0x0Fu]);
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
        early_uart_puts("ARMv7-A prefetch fault, ifsr=0x");
        early_uart_write_hex(ifsr, 8);
        early_uart_puts(", ifar=0x");
        early_uart_write_hex(fault_address, 8);
        early_uart_puts(", aifsr=0x");
        early_uart_write_hex(armv7a_read_aifsr(), 8);
        early_uart_puts("\r\n");
        early_uart_puts("ARMv7-A prefetch fault decode, status=0x");
        early_uart_write_hex(decode.status_code, 2);
        early_uart_puts(" (");
        early_uart_puts(decode.description);
        early_uart_puts("), domain=0x");
        early_uart_write_hex(decode.domain, 1);
        early_uart_puts("\r\n");
        break;
    }
    case kArmv7aExceptionDataAbort:
    {
        const auto dfsr = armv7a_read_dfsr();
        const auto decode = armv7a_decode_data_fault_status(dfsr);
        fault_address = armv7a_read_dfar();
        early_uart_puts("ARMv7-A data fault, dfsr=0x");
        early_uart_write_hex(dfsr, 8);
        early_uart_puts(", dfar=0x");
        early_uart_write_hex(fault_address, 8);
        early_uart_puts(", adfsr=0x");
        early_uart_write_hex(armv7a_read_adfsr(), 8);
        early_uart_puts("\r\n");
        early_uart_puts("ARMv7-A data fault decode, status=0x");
        early_uart_write_hex(decode.status_code, 2);
        early_uart_puts(" (");
        early_uart_puts(decode.description);
        early_uart_puts("), domain=0x");
        early_uart_write_hex(decode.domain, 1);
        early_uart_puts(", write=");
        early_uart_puts(decode.write ? "yes" : "no");
        early_uart_puts(", cm=");
        early_uart_puts(decode.cache_maintenance ? "yes" : "no");
        early_uart_puts("\r\n");
        break;
    }
    default:
        break;
    }

    if (fault_address != 0u) {
        const auto ttbr0 = armv7a_read_ttbr0();
        const auto descriptor = armv7a_l1_descriptor_from_ttbr0(ttbr0, fault_address);
        const auto decode = armv7a_decode_l1_descriptor(fault_address, descriptor);

        early_uart_puts("ARMv7-A fault map, far=0x");
        early_uart_write_hex(fault_address, 8);
        early_uart_puts(", ttbr0=0x");
        early_uart_write_hex(ttbr0, 8);
        early_uart_puts(", l1[0x");
        early_uart_write_hex(decode.index, 3);
        early_uart_puts("]=0x");
        early_uart_write_hex(decode.descriptor, 8);
        early_uart_puts(" (");
        early_uart_puts(armv7a_l1_descriptor_kind_name(decode.kind));
        early_uart_puts(")");
        if (decode.kind == Armv7aL1DescriptorKind::kPageTable ||
            decode.kind == Armv7aL1DescriptorKind::kSection ||
            decode.kind == Armv7aL1DescriptorKind::kSupersection) {
            early_uart_puts(", domain=0x");
            early_uart_write_hex(decode.domain, 1);
        }
        if (decode.kind == Armv7aL1DescriptorKind::kPageTable) {
            const auto l2_descriptor = armv7a_l2_descriptor_from_l1(descriptor, fault_address);
            const auto l2_decode = armv7a_decode_l2_descriptor(fault_address, l2_descriptor);

            early_uart_puts(", l2[0x");
            early_uart_write_hex(l2_decode.index, 2);
            early_uart_puts("]=0x");
            early_uart_write_hex(l2_decode.descriptor, 8);
            early_uart_puts(" (");
            early_uart_puts(armv7a_l2_descriptor_kind_name(l2_decode.kind));
            early_uart_puts(")");
            if (l2_decode.kind == Armv7aL2DescriptorKind::kSmallPage ||
                l2_decode.kind == Armv7aL2DescriptorKind::kLargePage) {
                early_uart_puts(", xn=");
                early_uart_puts(l2_decode.execute_never ? "yes" : "no");
                early_uart_puts(", s=");
                early_uart_puts(l2_decode.shareable ? "yes" : "no");
                early_uart_puts(", c=");
                early_uart_puts(l2_decode.cacheable ? "yes" : "no");
                early_uart_puts(", b=");
                early_uart_puts(l2_decode.bufferable ? "yes" : "no");
            }
        } else if (decode.kind == Armv7aL1DescriptorKind::kSection ||
                   decode.kind == Armv7aL1DescriptorKind::kSupersection) {
            early_uart_puts(", xn=");
            early_uart_puts(decode.execute_never ? "yes" : "no");
            early_uart_puts(", s=");
            early_uart_puts(decode.shareable ? "yes" : "no");
            early_uart_puts(", c=");
            early_uart_puts(decode.cacheable ? "yes" : "no");
            early_uart_puts(", b=");
            early_uart_puts(decode.bufferable ? "yes" : "no");
        }
        early_uart_puts("\r\n");
    }

    early_uart_puts("ARMv7-A fault context, sctlr=0x");
    early_uart_write_hex(armv7a_read_sctlr(), 8);
    early_uart_puts(", ttbr0=0x");
    early_uart_write_hex(armv7a_read_ttbr0(), 8);
    early_uart_puts(", ttbcr=0x");
    early_uart_write_hex(armv7a_read_ttbcr(), 8);
    early_uart_puts(", dacr=0x");
    early_uart_write_hex(armv7a_read_dacr(), 8);
    early_uart_puts("\r\n");
}
} // namespace

extern "C" void armv7a_handle_svc(Armv7aExceptionFrame* frame)
{
    const auto* instruction =
        reinterpret_cast<const std::uint32_t*>(armv7a_exception_pc(*frame));
    const auto immediate = *instruction & 0x00FFFFFFu;
    early_uart_puts("ARMv7-A SVC vector active, imm=0x");
    early_uart_write_hex(immediate, 6);
    early_uart_puts("\r\n");
}

extern "C" [[noreturn]] void armv7a_exception_fatal(const Armv7aExceptionFrame* frame)
{
    const auto kind = armv7a_exception_kind(*frame);
    early_uart_init();
    const auto current_cpsr = armv7a_read_cpsr();
    early_uart_puts("ARMv7-A exception: ");
    early_uart_puts(exception_name(kind));
    early_uart_puts(", pc=0x");
    early_uart_write_hex(armv7a_exception_pc(*frame), 8);
    early_uart_puts(", lr=0x");
    early_uart_write_hex(frame->lr, 8);
    early_uart_puts(", spsr=0x");
    early_uart_write_hex(frame->spsr, 8);
    early_uart_puts(", origin-mode=");
    early_uart_puts(armv7a_mode_name(frame->spsr));
    early_uart_puts(", current-cpsr=0x");
    early_uart_write_hex(current_cpsr, 8);
    early_uart_puts(", current-mode=");
    early_uart_puts(armv7a_mode_name(current_cpsr));
    early_uart_puts("\r\n");
    print_fault_registers(kind);
    charm_spin();
}
