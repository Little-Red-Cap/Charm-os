import out.format;
import out.sink;

#include "armv7a_arch_timer.hpp"
#include "armv7a_boot_page_table.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_mmu.hpp"

extern "C" void qemu_semihost_write0(const char* text);
extern "C" void armv7a_irq_smoke_test();
extern "C" void early_uart_init();
extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);
extern "C" [[noreturn]] void charm_spin();

namespace {
void early_uart_write(auto text)
{
    for (char ch : text) {
        early_uart_putc(ch);
    }
}

void early_uart_put_hex32(unsigned int value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        early_uart_putc(kHex[(value >> shift) & 0xFu]);
    }
}

void print_charm_module_status()
{
    out::buffer_sink<96> buffer{};
    auto status = out::vprint<"Charm out.format import active, PL011 @ 0x{:08X}\r\n">(buffer, 0x09000000u);
    if (!status) {
        early_uart_puts("out.format failed, err=0x");
        early_uart_put_hex32(static_cast<unsigned int>(status.error()));
        early_uart_puts("\r\n");
        return;
    }
    if (buffer.view().empty()) {
        early_uart_puts("out.format produced an empty buffer\r\n");
        return;
    }
    early_uart_write(buffer.view());
}

void print_cpu_boot_state()
{
    const auto cpsr = armv7a_read_cpsr();
    const auto sctlr = armv7a_read_sctlr();
    early_uart_puts("ARMv7-A boot state, cpsr=0x");
    early_uart_put_hex32(cpsr);
    early_uart_puts(", mode=");
    early_uart_puts(armv7a_mode_name(cpsr));
    early_uart_puts(", irq=");
    early_uart_puts(armv7a_irq_masked(cpsr) ? "masked" : "enabled");
    early_uart_puts("\r\n");

    early_uart_puts("ARMv7-A cp15 state, sctlr=0x");
    early_uart_put_hex32(sctlr);
    early_uart_puts(", vbar=0x");
    early_uart_put_hex32(armv7a_read_vbar());
    early_uart_puts(", mpidr=0x");
    early_uart_put_hex32(armv7a_read_mpidr());
    early_uart_puts(", cntfrq=0x");
    early_uart_put_hex32(armv7a_timer_read_cntfrq());
    early_uart_puts("\r\n");

    early_uart_puts("ARMv7-A cache state, mmu=");
    early_uart_puts(armv7a_mmu_enabled(sctlr) ? "on" : "off");
    early_uart_puts(", dcache=");
    early_uart_puts(armv7a_dcache_enabled(sctlr) ? "on" : "off");
    early_uart_puts(", icache=");
    early_uart_puts(armv7a_icache_enabled(sctlr) ? "on" : "off");
    early_uart_puts(", high-vectors=");
    early_uart_puts(armv7a_high_vectors_enabled(sctlr) ? "on" : "off");
    early_uart_puts("\r\n");

    early_uart_puts("ARMv7-A translation state, ttbr0=0x");
    early_uart_put_hex32(armv7a_read_ttbr0());
    early_uart_puts(", ttbr1=0x");
    early_uart_put_hex32(armv7a_read_ttbr1());
    early_uart_puts(", ttbcr=0x");
    early_uart_put_hex32(armv7a_read_ttbcr());
    early_uart_puts(", dacr=0x");
    early_uart_put_hex32(armv7a_read_dacr());
    early_uart_puts("\r\n");
}

void print_boot_page_table_state()
{
    armv7a_prepare_boot_identity_map();

    early_uart_puts("ARMv7-A L1 table ready, base=0x");
    early_uart_put_hex32(static_cast<unsigned int>(armv7a_boot_l1_table_base()));
    early_uart_puts(", ram=0x");
    early_uart_put_hex32(armv7a_boot_l1_descriptor(0x40200000u));
    early_uart_puts(", gic=0x");
    early_uart_put_hex32(armv7a_boot_l1_descriptor(0x08000000u));
    early_uart_puts(", uart=0x");
    early_uart_put_hex32(armv7a_boot_l1_descriptor(0x09000000u));
    early_uart_puts("\r\n");
}

void print_mmu_runtime_state()
{
    const auto sctlr = armv7a_read_sctlr();
    early_uart_puts("ARMv7-A MMU active, sctlr=0x");
    early_uart_put_hex32(sctlr);
    early_uart_puts(", ttbr0=0x");
    early_uart_put_hex32(armv7a_read_ttbr0());
    early_uart_puts(", ttbcr=0x");
    early_uart_put_hex32(armv7a_read_ttbcr());
    early_uart_puts(", dacr=0x");
    early_uart_put_hex32(armv7a_read_dacr());
    early_uart_puts("\r\n");

    early_uart_puts("ARMv7-A MMU flags, mmu=");
    early_uart_puts(armv7a_mmu_enabled(sctlr) ? "on" : "off");
    early_uart_puts(", dcache=");
    early_uart_puts(armv7a_dcache_enabled(sctlr) ? "on" : "off");
    early_uart_puts(", icache=");
    early_uart_puts(armv7a_icache_enabled(sctlr) ? "on" : "off");
    early_uart_puts("\r\n");
}
} // namespace

int main()
{
#if defined(CHARM_QEMU_SEMIHOST_DEBUG)
    qemu_semihost_write0("semihost: entering main\n");
#endif
    early_uart_init();
    early_uart_puts("Charm ARMv7-A QEMU skeleton\r\n");
    early_uart_puts("Targeting Cortex-A7 first, RK3506 later.\r\n");
    print_cpu_boot_state();
    print_boot_page_table_state();
    armv7a_enable_identity_mmu(armv7a_boot_l1_table_base());
    print_mmu_runtime_state();
    print_charm_module_status();
    armv7a_svc_smoke_test();
    armv7a_irq_smoke_test();
    charm_spin();
}
