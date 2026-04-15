import out.format;
import out.sink;

#include "armv7a_attribute_probe.hpp"
#include "armv7a_boot_page_table.hpp"
#include "armv7a_cache.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_dcache_probe.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_handler_stack.hpp"
#include "armv7a_icache_probe.hpp"
#include "armv7a_interrupt_smoke.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_page_table_probe.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_section_split_probe.hpp"
#include "armv7a_small_page_probe.hpp"

extern "C" void armv7a_irq_smoke_test();
extern "C" void armv7a_sgi_smoke_test();
extern "C" void armv7a_fiq_smoke_test();
extern "C" void armv7a_prepare_abort_smoke_mappings();
extern "C" void armv7a_prepare_abort_smoke_runtime();
extern "C" void armv7a_print_abort_smoke_mapping_state();
extern "C" void armv7a_run_abort_smoke_if_enabled();
extern "C" void armv7a_run_exception_smoke_if_enabled();

namespace {
constexpr std::uint32_t kArmv7aTimerCtrlEnable = 1u << 0;
constexpr std::uint32_t kArmv7aTimerCtrlImask = 1u << 1;
constexpr std::uint32_t kArmv7aTimerCtrlIstatus = 1u << 2;

void platform_console_write(auto text)
{
    for (char ch : text) {
        armv7a_platform_early_console_putc(ch);
    }
}

void platform_console_put_hex32(unsigned int value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        armv7a_platform_early_console_putc(kHex[(value >> shift) & 0xFu]);
    }
}

const char* yes_no_name(bool value)
{
    return value ? "yes" : "no";
}

void print_charm_module_status()
{
    const auto& mmio = armv7a_platform_mmio_layout();
    out::buffer_sink<96> buffer{};
    auto status = out::vprint<"Charm out.format import active, PL011 @ 0x{:08X}\r\n">(
        buffer, mmio.pl011_base);
    if (!status) {
        armv7a_platform_early_console_puts("out.format failed, err=0x");
        platform_console_put_hex32(static_cast<unsigned int>(status.error()));
        armv7a_platform_early_console_puts("\r\n");
        return;
    }
    if (buffer.view().empty()) {
        armv7a_platform_early_console_puts("out.format produced an empty buffer\r\n");
        return;
    }
    platform_console_write(buffer.view());
}

void print_cpu_boot_state()
{
    const auto cpsr = armv7a_read_cpsr();
    const auto sctlr = armv7a_read_sctlr();
    const auto id_mmfr0 = armv7a_read_id_mmfr0();
    const auto id_pfr1 = armv7a_read_id_pfr1();
    const auto& reset_state = armv7a_platform_reset_state();
    armv7a_platform_early_console_puts("ARMv7-A boot state, cpsr=0x");
    platform_console_put_hex32(cpsr);
    armv7a_platform_early_console_puts(", mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(cpsr));
    armv7a_platform_early_console_puts(", irq=");
    armv7a_platform_early_console_puts(armv7a_irq_masked(cpsr) ? "masked" : "enabled");
    armv7a_platform_early_console_puts("\r\n");

    armv7a_platform_early_console_puts("ARMv7-A reset evidence, sctlr=0x");
    platform_console_put_hex32(reset_state.initial_sctlr);
    armv7a_platform_early_console_puts(", vbar=0x");
    platform_console_put_hex32(reset_state.initial_vbar);
    armv7a_platform_early_console_puts(", high-vectors=");
    armv7a_platform_early_console_puts(
        armv7a_high_vectors_enabled(reset_state.initial_sctlr) ? "on" : "off");
    armv7a_platform_early_console_puts(", low-vectors-forced=");
    armv7a_platform_early_console_puts(reset_state.forced_low_vectors ? "yes" : "no");
    armv7a_platform_early_console_puts("\r\n");

    armv7a_platform_early_console_puts("ARMv7-A cp15 state, sctlr=0x");
    platform_console_put_hex32(sctlr);
    armv7a_platform_early_console_puts(", vbar=0x");
    platform_console_put_hex32(armv7a_read_vbar());
    armv7a_platform_early_console_puts(", mpidr=0x");
    platform_console_put_hex32(armv7a_read_mpidr());
    armv7a_platform_early_console_puts(", cntfrq=0x");
    platform_console_put_hex32(armv7a_platform_timer_frequency_hz());
    armv7a_platform_early_console_puts("\r\n");

    const auto interrupt_state = armv7a_platform_interrupt_controller_state();
    const auto secure_timer_line = armv7a_platform_secure_timer_interrupt_line_state();
    const auto nonsecure_timer_line = armv7a_platform_nonsecure_timer_interrupt_line_state();
    const auto sgi_line = armv7a_platform_self_sgi_line_state();
    const auto timer_ctrl = armv7a_platform_timer_control();

    armv7a_platform_early_console_puts("ARMv7-A interrupt reset state, gicd=0x");
    platform_console_put_hex32(interrupt_state.distributor_control);
    armv7a_platform_early_console_puts(", gicc=0x");
    platform_console_put_hex32(interrupt_state.cpu_control);
    armv7a_platform_early_console_puts(", pmr=0x");
    platform_console_put_hex32(interrupt_state.priority_mask);
    armv7a_platform_early_console_puts(", bpr=0x");
    platform_console_put_hex32(interrupt_state.binary_point);
    armv7a_platform_early_console_puts(", hppir=0x");
    platform_console_put_hex32(interrupt_state.highest_pending);
    armv7a_platform_early_console_puts(", spurious=");
    armv7a_platform_early_console_puts(yes_no_name(interrupt_state.highest_pending_special));
    armv7a_platform_early_console_puts("\r\n");

    armv7a_platform_early_console_puts("ARMv7-A timer reset state, cntp_ctl=0x");
    platform_console_put_hex32(timer_ctrl);
    armv7a_platform_early_console_puts(", enabled=");
    armv7a_platform_early_console_puts(yes_no_name((timer_ctrl & kArmv7aTimerCtrlEnable) != 0u));
    armv7a_platform_early_console_puts(", imask=");
    armv7a_platform_early_console_puts(yes_no_name((timer_ctrl & kArmv7aTimerCtrlImask) != 0u));
    armv7a_platform_early_console_puts(", istatus=");
    armv7a_platform_early_console_puts(
        yes_no_name((timer_ctrl & kArmv7aTimerCtrlIstatus) != 0u));
    armv7a_platform_early_console_puts(", secure-line=");
    armv7a_platform_early_console_puts(armv7a_platform_interrupt_line_group_name(secure_timer_line));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(yes_no_name(secure_timer_line.line_enabled));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(yes_no_name(secure_timer_line.line_pending));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(yes_no_name(secure_timer_line.line_active));
    armv7a_platform_early_console_puts(", nonsecure-line=");
    armv7a_platform_early_console_puts(
        armv7a_platform_interrupt_line_group_name(nonsecure_timer_line));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(yes_no_name(nonsecure_timer_line.line_enabled));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(yes_no_name(nonsecure_timer_line.line_pending));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(yes_no_name(nonsecure_timer_line.line_active));
    armv7a_platform_early_console_puts("\r\n");

    armv7a_platform_early_console_puts("ARMv7-A SGI reset state, line=");
    armv7a_platform_early_console_puts(armv7a_platform_interrupt_line_group_name(sgi_line));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(yes_no_name(sgi_line.line_enabled));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(yes_no_name(sgi_line.line_pending));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(yes_no_name(sgi_line.line_active));
    armv7a_platform_early_console_puts("\r\n");

    armv7a_platform_early_console_puts("ARMv7-A memory model, id_mmfr0=0x");
    platform_console_put_hex32(id_mmfr0);
    armv7a_platform_early_console_puts(", vmsa=0x");
    platform_console_put_hex32(armv7a_id_mmfr0_vmsa_field(id_mmfr0));
    armv7a_platform_early_console_puts(" (");
    armv7a_platform_early_console_puts(armv7a_feature_presence_name(armv7a_id_mmfr0_vmsa_field(id_mmfr0)));
    armv7a_platform_early_console_puts("), pmsa=0x");
    platform_console_put_hex32(armv7a_id_mmfr0_pmsa_field(id_mmfr0));
    armv7a_platform_early_console_puts(" (");
    armv7a_platform_early_console_puts(armv7a_feature_presence_name(armv7a_id_mmfr0_pmsa_field(id_mmfr0)));
    armv7a_platform_early_console_puts(")\r\n");

    armv7a_platform_early_console_puts("ARMv7-A feature state, id_pfr1=0x");
    platform_console_put_hex32(id_pfr1);
    armv7a_platform_early_console_puts(", security=0x");
    platform_console_put_hex32(armv7a_id_pfr1_security_field(id_pfr1));
    armv7a_platform_early_console_puts(" (");
    armv7a_platform_early_console_puts(armv7a_feature_presence_name(armv7a_id_pfr1_security_field(id_pfr1)));
    armv7a_platform_early_console_puts("), virtualization=0x");
    platform_console_put_hex32(armv7a_id_pfr1_virtualization_field(id_pfr1));
    armv7a_platform_early_console_puts(" (");
    armv7a_platform_early_console_puts(armv7a_feature_presence_name(armv7a_id_pfr1_virtualization_field(id_pfr1)));
    armv7a_platform_early_console_puts("), gentimer=0x");
    platform_console_put_hex32(armv7a_id_pfr1_gentimer_field(id_pfr1));
    armv7a_platform_early_console_puts(" (");
    armv7a_platform_early_console_puts(armv7a_feature_presence_name(armv7a_id_pfr1_gentimer_field(id_pfr1)));
    armv7a_platform_early_console_puts(")\r\n");

    armv7a_platform_early_console_puts("ARMv7-A cache state, mmu=");
    armv7a_platform_early_console_puts(armv7a_mmu_enabled(sctlr) ? "on" : "off");
    armv7a_platform_early_console_puts(", dcache=");
    armv7a_platform_early_console_puts(armv7a_dcache_enabled(sctlr) ? "on" : "off");
    armv7a_platform_early_console_puts(", icache=");
    armv7a_platform_early_console_puts(armv7a_icache_enabled(sctlr) ? "on" : "off");
    armv7a_platform_early_console_puts(", high-vectors=");
    armv7a_platform_early_console_puts(armv7a_high_vectors_enabled(sctlr) ? "on" : "off");
    armv7a_platform_early_console_puts("\r\n");

    armv7a_platform_early_console_puts("ARMv7-A translation state, ttbr0=0x");
    platform_console_put_hex32(armv7a_read_ttbr0());
    armv7a_platform_early_console_puts(", ttbr1=0x");
    platform_console_put_hex32(armv7a_read_ttbr1());
    armv7a_platform_early_console_puts(", ttbcr=0x");
    platform_console_put_hex32(armv7a_read_ttbcr());
    armv7a_platform_early_console_puts(", dacr=0x");
    platform_console_put_hex32(armv7a_read_dacr());
    armv7a_platform_early_console_puts("\r\n");
}

void print_boot_page_table_state()
{
    const auto& address_space = armv7a_platform_address_space();
    const auto& mmio = armv7a_platform_mmio_layout();
    armv7a_platform_early_console_puts("ARMv7-A L1 table ready, base=0x");
    platform_console_put_hex32(static_cast<unsigned int>(armv7a_boot_l1_table_base()));
    armv7a_platform_early_console_puts(", ram=0x");
    platform_console_put_hex32(armv7a_boot_l1_descriptor(address_space.image_load_base));
    armv7a_platform_early_console_puts(", gic=0x");
    platform_console_put_hex32(armv7a_boot_l1_descriptor(mmio.gic_distributor_base));
    armv7a_platform_early_console_puts(", uart=0x");
    platform_console_put_hex32(armv7a_boot_l1_descriptor(mmio.pl011_base));
    armv7a_platform_early_console_puts("\r\n");
}

void print_mmu_runtime_state()
{
    const auto sctlr = armv7a_read_sctlr();
    armv7a_platform_early_console_puts("ARMv7-A MMU active, sctlr=0x");
    platform_console_put_hex32(sctlr);
    armv7a_platform_early_console_puts(", ttbr0=0x");
    platform_console_put_hex32(armv7a_read_ttbr0());
    armv7a_platform_early_console_puts(", ttbcr=0x");
    platform_console_put_hex32(armv7a_read_ttbcr());
    armv7a_platform_early_console_puts(", dacr=0x");
    platform_console_put_hex32(armv7a_read_dacr());
    armv7a_platform_early_console_puts("\r\n");

    armv7a_platform_early_console_puts("ARMv7-A MMU flags, mmu=");
    armv7a_platform_early_console_puts(armv7a_mmu_enabled(sctlr) ? "on" : "off");
    armv7a_platform_early_console_puts(", dcache=");
    armv7a_platform_early_console_puts(armv7a_dcache_enabled(sctlr) ? "on" : "off");
    armv7a_platform_early_console_puts(", icache=");
    armv7a_platform_early_console_puts(armv7a_icache_enabled(sctlr) ? "on" : "off");
    armv7a_platform_early_console_puts("\r\n");
}

void print_dcache_runtime_state()
{
    const auto sctlr = armv7a_read_sctlr();
    const auto geometry = armv7a_read_l1_dcache_geometry();
    armv7a_platform_early_console_puts("ARMv7-A D-cache active, sctlr=0x");
    platform_console_put_hex32(sctlr);
    armv7a_platform_early_console_puts(", clidr=0x");
    platform_console_put_hex32(geometry.clidr);
    armv7a_platform_early_console_puts(", ccsidr=0x");
    platform_console_put_hex32(geometry.ccsidr);
    armv7a_platform_early_console_puts(", line=0x");
    platform_console_put_hex32(geometry.line_size_bytes);
    armv7a_platform_early_console_puts(", ways=0x");
    platform_console_put_hex32(geometry.ways);
    armv7a_platform_early_console_puts(", sets=0x");
    platform_console_put_hex32(geometry.sets);
    armv7a_platform_early_console_puts("\r\n");
}
} // namespace

int main()
{
#if defined(CHARM_QEMU_SEMIHOST_DEBUG)
    armv7a_platform_debug_trace("semihost: entering main\n");
#endif
    armv7a_platform_early_console_init();
    armv7a_platform_early_console_puts("Charm ARMv7-A QEMU skeleton\r\n");
    armv7a_platform_early_console_puts("Targeting Cortex-A7 first, RK3506 later.\r\n");
    print_cpu_boot_state();
    armv7a_prepare_boot_identity_map();
    armv7a_prepare_abort_smoke_mappings();
    armv7a_prepare_small_page_probe_mapping();
    armv7a_prepare_attribute_probe_mapping();
    armv7a_prepare_dcache_probe_mapping();
    armv7a_prepare_icache_probe_mapping();
    armv7a_prepare_page_table_probe_mapping();
    armv7a_prepare_section_split_probe_mapping();
    print_boot_page_table_state();
    armv7a_print_abort_smoke_mapping_state();
    armv7a_print_small_page_probe_mapping_state();
    armv7a_print_attribute_probe_mapping_state();
    armv7a_print_dcache_probe_mapping_state();
    armv7a_print_icache_probe_mapping_state();
    armv7a_print_page_table_probe_mapping_state();
    armv7a_print_section_split_probe_mapping_state();
    armv7a_enable_identity_mmu(armv7a_boot_l1_table_base());
    armv7a_prepare_abort_smoke_runtime();
    armv7a_enable_icache();
    print_mmu_runtime_state();
    print_charm_module_status();
    armv7a_run_small_page_probe();
    armv7a_run_attribute_probe();
    armv7a_run_icache_probe();
    armv7a_run_abort_smoke_if_enabled();
    armv7a_run_exception_smoke_if_enabled();
    armv7a_enable_dcache();
    print_dcache_runtime_state();
    armv7a_run_dcache_probe();
    armv7a_run_page_table_probe();
    armv7a_run_section_split_probe();
    armv7a_svc_smoke_test();
    if (armv7a_svc_observation_seen()) {
        armv7a_print_return_state_evidence(
            "svc", armv7a_svc_observation_spsr(), armv7a_read_cpsr());
    }
    armv7a_irq_smoke_test();
    armv7a_sgi_smoke_test();
    armv7a_fiq_smoke_test();
    armv7a_interrupt_print_security_side_evidence();
    armv7a_platform_idle_forever();
}
