#include "armv7a_gic.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_platform.hpp"

namespace {
constexpr std::uint32_t kGicdCtlr = 0x0000u;
constexpr std::uint32_t kGicdIgroupr = 0x0080u;
constexpr std::uint32_t kGicdIsenabler = 0x0100u;
constexpr std::uint32_t kGicdIcenabler = 0x0180u;
constexpr std::uint32_t kGicdIspendr = 0x0200u;
constexpr std::uint32_t kGicdIcpendr = 0x0280u;
constexpr std::uint32_t kGicdIsactiver = 0x0300u;
constexpr std::uint32_t kGicdIcactiver = 0x0380u;
constexpr std::uint32_t kGicdIpriorityr = 0x0400u;
constexpr std::uint32_t kGicdIcfgr = 0x0c00u;
constexpr std::uint32_t kGicdSgir = 0x0f00u;
constexpr std::uint32_t kGicdCpendsgir = 0x0f10u;

constexpr std::uint32_t kGiccCtlr = 0x0000u;
constexpr std::uint32_t kGiccPmr = 0x0004u;
constexpr std::uint32_t kGiccBpr = 0x0008u;
constexpr std::uint32_t kGiccIar = 0x000cu;
constexpr std::uint32_t kGiccEoir = 0x0010u;
constexpr std::uint32_t kGiccHppir = 0x0018u;

constexpr std::uint32_t kGicdSgirTargetFilterSelf = 2u << 24;
constexpr std::uint32_t kGiccCtlrFiqEn = 1u << 3;

inline volatile std::uint32_t& reg(std::uintptr_t addr)
{
    return *reinterpret_cast<volatile std::uint32_t*>(addr);
}

inline std::uint32_t mmio_read(std::uintptr_t base, std::uint32_t offset)
{
    return reg(base + offset);
}

inline void mmio_write(std::uintptr_t base, std::uint32_t offset, std::uint32_t value)
{
    reg(base + offset) = value;
}

std::uintptr_t gic_dist_base()
{
    return armv7a_platform_mmio_layout().gic_distributor_base;
}

std::uintptr_t gic_cpu_base()
{
    return armv7a_platform_mmio_layout().gic_cpu_interface_base;
}

std::uint32_t gic_current_cpu_mask()
{
    const auto affinity0 = armv7a_read_mpidr() & 0x7u;
    return 1u << affinity0;
}

std::uint32_t gic_read_line_bank(std::uint32_t offset, unsigned int intid)
{
    return mmio_read(gic_dist_base(), offset + static_cast<std::uint32_t>(4u * (intid / 32u)));
}

void gic_clear_active(unsigned int intid)
{
    mmio_write(gic_dist_base(),
               kGicdIcactiver + static_cast<std::uint32_t>(4u * (intid / 32u)),
               1u << (intid % 32u));
}

void gic_set_group0(unsigned int intid)
{
    const auto offset = kGicdIgroupr + static_cast<std::uint32_t>(4u * (intid / 32u));
    auto value = mmio_read(gic_dist_base(), offset);
    value &= ~(1u << (intid % 32u));
    mmio_write(gic_dist_base(), offset, value);
}

void gic_set_group1(unsigned int intid)
{
    const auto offset = kGicdIgroupr + static_cast<std::uint32_t>(4u * (intid / 32u));
    auto value = mmio_read(gic_dist_base(), offset);
    value |= 1u << (intid % 32u);
    mmio_write(gic_dist_base(), offset, value);
}

void gic_set_priority(unsigned int intid, std::uint8_t priority)
{
    const auto offset = kGicdIpriorityr + static_cast<std::uint32_t>(4u * (intid / 4u));
    const auto shift = static_cast<unsigned int>((intid % 4u) * 8u);
    auto value = mmio_read(gic_dist_base(), offset);
    value &= ~(0xffu << shift);
    value |= static_cast<std::uint32_t>(priority) << shift;
    mmio_write(gic_dist_base(), offset, value);
}

void gic_set_level_triggered(unsigned int intid)
{
    const auto offset = kGicdIcfgr + static_cast<std::uint32_t>(4u * (intid / 16u));
    const auto shift = static_cast<unsigned int>(((intid % 16u) * 2u) + 1u);
    auto value = mmio_read(gic_dist_base(), offset);
    value &= ~(1u << shift);
    mmio_write(gic_dist_base(), offset, value);
}

void gic_enable_line(unsigned int intid)
{
    mmio_write(gic_dist_base(),
               kGicdIsenabler + static_cast<std::uint32_t>(4u * (intid / 32u)),
               1u << (intid % 32u));
}

void gic_reset_interfaces()
{
    mmio_write(gic_cpu_base(), kGiccCtlr, 0u);
    mmio_write(gic_dist_base(), kGicdCtlr, 0u);
}

void gic_configure_timer_line(unsigned int intid, Armv7aGicInterruptGroup group)
{
    armv7a_gic_disable_line(intid);
    armv7a_gic_clear_pending(intid);
    gic_clear_active(intid);
    if (group == Armv7aGicInterruptGroup::kGroup1) {
        gic_set_group1(intid);
    } else {
        gic_set_group0(intid);
    }
    gic_set_level_triggered(intid);
    gic_set_priority(intid, 0x80u);
    gic_enable_line(intid);
}

void gic_configure_sgi_line(unsigned int intid, Armv7aGicInterruptGroup group)
{
    armv7a_gic_disable_line(intid);
    armv7a_gic_clear_pending(intid);
    armv7a_gic_clear_sgi_pending(intid);
    if (group == Armv7aGicInterruptGroup::kGroup1) {
        gic_set_group1(intid);
    } else {
        gic_set_group0(intid);
    }
    gic_set_priority(intid, 0x40u);
    gic_enable_line(intid);
}
} // namespace

void armv7a_gic_init_timer_irq()
{
    gic_reset_interfaces();
    // CNTP uses the secure or non-secure physical PPI depending on the
    // current CPU security state. Prepare both lines so the same smoke test
    // works across QEMU reset states and future boards.
    gic_configure_timer_line(kArmv7aGicSecureTimerIntId, Armv7aGicInterruptGroup::kGroup0);
    gic_configure_timer_line(kArmv7aGicNonSecureTimerIntId, Armv7aGicInterruptGroup::kGroup1);
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
}

void armv7a_gic_init_sgi_irq(Armv7aGicInterruptGroup group)
{
    gic_reset_interfaces();
    gic_configure_sgi_line(kArmv7aGicSelfSgiIntId, group);
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
}

void armv7a_gic_enable_interfaces(bool fiq_enabled)
{
    mmio_write(gic_cpu_base(), kGiccPmr, 0xffu);
    mmio_write(gic_cpu_base(), kGiccBpr, 0u);

    // AckCtl lets one IAR/EOIR pair handle both Group0 and Group1 IRQs.
    auto cpu_ctlr = 0x7u;
    if (fiq_enabled) {
        cpu_ctlr |= kGiccCtlrFiqEn;
    }

    mmio_write(gic_cpu_base(), kGiccCtlr, cpu_ctlr);
    mmio_write(gic_dist_base(), kGicdCtlr, 0x3u);
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
}

void armv7a_gic_disable_interfaces()
{
    mmio_write(gic_cpu_base(), kGiccCtlr, 0u);
    mmio_write(gic_dist_base(), kGicdCtlr, 0u);
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
}

void armv7a_gic_disable_line(unsigned int intid)
{
    mmio_write(gic_dist_base(),
               kGicdIcenabler + static_cast<std::uint32_t>(4u * (intid / 32u)),
               1u << (intid % 32u));
}

void armv7a_gic_clear_pending(unsigned int intid)
{
    mmio_write(gic_dist_base(),
               kGicdIcpendr + static_cast<std::uint32_t>(4u * (intid / 32u)),
               1u << (intid % 32u));
}

void armv7a_gic_clear_sgi_pending(unsigned int intid)
{
    const auto offset = kGicdCpendsgir + static_cast<std::uint32_t>(4u * (intid / 4u));
    const auto shift = static_cast<unsigned int>((intid % 4u) * 8u);
    mmio_write(gic_dist_base(), offset, gic_current_cpu_mask() << shift);
}

void armv7a_gic_send_self_sgi(unsigned int intid)
{
    mmio_write(gic_dist_base(), kGicdSgir, kGicdSgirTargetFilterSelf | (intid & 0x0fu));
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
}

std::uint32_t armv7a_gic_acknowledge_irq()
{
    return mmio_read(gic_cpu_base(), kGiccIar);
}

void armv7a_gic_end_irq(std::uint32_t iar)
{
    mmio_write(gic_cpu_base(), kGiccEoir, iar);
}

Armv7aGicLineState armv7a_gic_read_line_state(unsigned int intid)
{
    return Armv7aGicLineState{
        .igroupr = gic_read_line_bank(kGicdIgroupr, intid),
        .isenabler = gic_read_line_bank(kGicdIsenabler, intid),
        .ispendr = gic_read_line_bank(kGicdIspendr, intid),
        .isactiver = gic_read_line_bank(kGicdIsactiver, intid),
    };
}

Armv7aGicCpuState armv7a_gic_read_cpu_state()
{
    return Armv7aGicCpuState{
        .ctlr = mmio_read(gic_cpu_base(), kGiccCtlr),
        .hppir = mmio_read(gic_cpu_base(), kGiccHppir),
    };
}

bool armv7a_gic_is_timer_intid(unsigned int intid)
{
    return intid == kArmv7aGicSecureTimerIntId || intid == kArmv7aGicNonSecureTimerIntId;
}

bool armv7a_gic_is_sgi_intid(unsigned int intid)
{
    return intid == kArmv7aGicSelfSgiIntId;
}
