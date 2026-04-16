#include "rk3506_armv7a_state.hpp"
#include "rk3506_exception_frame.hpp"
#include "rk3506_platform.hpp"

#include <cstdint>

#ifndef CHARM_RK3506_SDRAM_BASE
#define CHARM_RK3506_SDRAM_BASE 0x00000000
#endif

#ifndef CHARM_RK3506_SYSTEM_SRAM_BASE
#define CHARM_RK3506_SYSTEM_SRAM_BASE 0xfff80000
#endif

#ifndef CHARM_RK3506_SYSTEM_SRAM_SIZE
#define CHARM_RK3506_SYSTEM_SRAM_SIZE 0x0000c000
#endif

#ifndef CHARM_RK3506_SDRAM_SIZE
#define CHARM_RK3506_SDRAM_SIZE 0x04000000
#endif

#ifndef CHARM_RK3506_IMAGE_TEXT_BASE
#define CHARM_RK3506_IMAGE_TEXT_BASE 0x00200000
#endif

#ifndef CHARM_RK3506_UART0_BASE
#define CHARM_RK3506_UART0_BASE 0xff0a0000
#endif

#ifndef CHARM_RK3506_UART4_BASE
#define CHARM_RK3506_UART4_BASE 0xff0e0000
#endif

#ifndef CHARM_RK3506_GPIO0_IOC_BASE
#define CHARM_RK3506_GPIO0_IOC_BASE 0xff950000
#endif

#ifndef CHARM_RK3506_EARLY_UART_BASE
#define CHARM_RK3506_EARLY_UART_BASE CHARM_RK3506_UART0_BASE
#endif

#ifndef CHARM_RK3506_UART_REG_SHIFT
#define CHARM_RK3506_UART_REG_SHIFT 2
#endif

#ifndef CHARM_RK3506_XIN_OSC_HZ
#define CHARM_RK3506_XIN_OSC_HZ 24000000
#endif

#ifndef CHARM_RK3506_EARLY_UART_BAUD_RATE
#define CHARM_RK3506_EARLY_UART_BAUD_RATE 115200
#endif

#ifndef CHARM_RK3506_GICD_BASE
#define CHARM_RK3506_GICD_BASE 0xff581000
#endif

#ifndef CHARM_RK3506_GICC_BASE
#define CHARM_RK3506_GICC_BASE 0xff582000
#endif

#ifndef CHARM_RK3506_GRF_BASE
#define CHARM_RK3506_GRF_BASE 0xff288000
#endif

#ifndef CHARM_RK3506_GRF_PMU_BASE
#define CHARM_RK3506_GRF_PMU_BASE 0xff910000
#endif

#ifndef CHARM_RK3506_CRU_BASE
#define CHARM_RK3506_CRU_BASE 0xff9a0000
#endif

#ifndef CHARM_RK3506_SCRU_BASE
#define CHARM_RK3506_SCRU_BASE 0xff9a8000
#endif

#ifndef CHARM_RK3506_GENERIC_TIMER_FREQUENCY_HZ
#define CHARM_RK3506_GENERIC_TIMER_FREQUENCY_HZ 24000000
#endif

#ifndef CHARM_RK3506_GENERIC_TIMER_EXPECTED_INTID
#define CHARM_RK3506_GENERIC_TIMER_EXPECTED_INTID 30
#endif

namespace {
constexpr std::uint32_t kUartRegShift = CHARM_RK3506_UART_REG_SHIFT;
constexpr std::uint32_t kUartThrIndex = 0u;
constexpr std::uint32_t kUartDllIndex = 0u;
constexpr std::uint32_t kUartDlhIndex = 1u;
constexpr std::uint32_t kUartFcrIndex = 2u;
constexpr std::uint32_t kUartLcrIndex = 3u;
constexpr std::uint32_t kUartMcrIndex = 4u;
constexpr std::uint32_t kUartLsrIndex = 5u;
constexpr std::uint32_t kUartSrrIndex = 0x88u >> kUartRegShift;
constexpr std::uint32_t kUartDmasaIndex = 0xa8u >> kUartRegShift;
constexpr std::uint32_t kUartFcrEnableFifo = 0x01u;
constexpr std::uint32_t kUartFcrClearRcvr = 0x02u;
constexpr std::uint32_t kUartFcrClearXmit = 0x04u;
constexpr std::uint32_t kUartFcrTTrig10 = 0x20u;
constexpr std::uint32_t kUartFcrRTrig10 = 0x80u;
constexpr std::uint32_t kUartLcrWlen8 = 0x03u;
constexpr std::uint32_t kUartLcrDlab = 0x80u;
constexpr std::uint32_t kUartMcrLoop = 0x10u;
constexpr std::uint32_t kUartLsrThre = 0x20u;
constexpr std::uint32_t kUartSrrUr = 0x01u;
constexpr std::uint32_t kUartSrrRfr = 0x02u;
constexpr std::uint32_t kUartSrrXfr = 0x04u;
constexpr std::uint32_t kUartModeXDiv = 16u;

constexpr std::uint32_t kCruClkselCon29Offset = 0x374u;
constexpr std::uint32_t kCruClkselCon29SclkUart0DivShift = 7u;
constexpr std::uint32_t kCruClkselCon29SclkUart0DivMask = 0x00000f80u;
constexpr std::uint32_t kCruClkselCon29SclkUart0SelShift = 12u;
constexpr std::uint32_t kCruClkselCon29SclkUart0SelMask = 0x00007000u;
constexpr std::uint32_t kCruClkselCon29SclkUart0SelXinOsc0Func = 0u;
constexpr std::uint32_t kCruGateCon11Offset = 0x82cu;
constexpr std::uint32_t kCruGateCon11HclkLsperiRootMask = 0x00000001u;
constexpr std::uint32_t kCruGateCon11PclkLsperiRootMask = 0x00000002u;
constexpr std::uint32_t kCruGateCon11HclkLsperiBiuMask = 0x00000004u;
constexpr std::uint32_t kCruGateCon11PclkUart0Mask = 0x00000010u;
constexpr std::uint32_t kCruGateCon11SclkUart0SrcMask = 0x00000200u;

constexpr std::uint32_t kGicdCtlrOffset = 0x0000u;
constexpr std::uint32_t kGicdIgrouprOffset = 0x0080u;
constexpr std::uint32_t kGicdIsenablerOffset = 0x0100u;
constexpr std::uint32_t kGicdIcenablerOffset = 0x0180u;
constexpr std::uint32_t kGicdIspendrOffset = 0x0200u;
constexpr std::uint32_t kGicdIcpendrOffset = 0x0280u;
constexpr std::uint32_t kGicdIsactiverOffset = 0x0300u;
constexpr std::uint32_t kGicdIcactiverOffset = 0x0380u;
constexpr std::uint32_t kGicdIpriorityrOffset = 0x0400u;
constexpr std::uint32_t kGicdIcfgrOffset = 0x0c00u;
constexpr std::uint32_t kGicdTyperOffset = 0x0004u;
constexpr std::uint32_t kGicdIidrOffset = 0x0008u;
constexpr std::uint32_t kGiccCtlrOffset = 0x0000u;
constexpr std::uint32_t kGiccPmrOffset = 0x0004u;
constexpr std::uint32_t kGiccBprOffset = 0x0008u;
constexpr std::uint32_t kGiccIarOffset = 0x000cu;
constexpr std::uint32_t kGiccEoirOffset = 0x0010u;
constexpr std::uint32_t kGiccHppirOffset = 0x0018u;
constexpr std::uint32_t kGiccIidrOffset = 0x00fcu;
constexpr std::uint32_t kGicdTyperItLinesNumberMask = 0x1fu;
constexpr std::uint32_t kGicdTyperCpuNumberShift = 5u;
constexpr std::uint32_t kGicdTyperCpuNumberMask = 0x000000e0u;
constexpr std::uint32_t kGiccCtlrEnableGroup0 = 1u << 0;
constexpr std::uint32_t kGiccCtlrEnableGroup1 = 1u << 1;
constexpr std::uint32_t kGiccCtlrAckCtl = 1u << 2;
constexpr std::uint32_t kGicdCtlrEnableGroup0 = 1u << 0;
constexpr std::uint32_t kGicdCtlrEnableGroup1 = 1u << 1;
constexpr std::uint32_t kGicIntIdMask = 0x3ffu;
constexpr unsigned int kGicSpecialIntIdMin = 1020u;
constexpr unsigned int kRk3506SecurePhysicalTimerIntId = 29u;
constexpr unsigned int kRk3506NonSecurePhysicalTimerIntId = 30u;
constexpr unsigned int kRk3506ExpectedGenericTimerIntId =
    CHARM_RK3506_GENERIC_TIMER_EXPECTED_INTID;

constexpr std::uint32_t kTimerCtrlEnable = 1u << 0;
constexpr std::uint32_t kTimerCtrlItMask = 1u << 1;
constexpr std::uint32_t kTimerCtrlItStatus = 1u << 2;

constexpr std::uint32_t kGpio0cIomuxSel1Offset = 0x14u;
constexpr std::uint32_t kGpio0cIomuxSel1C6Shift = 8u;
constexpr std::uint32_t kGpio0cIomuxSel1C6Mask = 0x00000f00u;
constexpr std::uint32_t kGpio0cIomuxSel1C7Shift = 12u;
constexpr std::uint32_t kGpio0cIomuxSel1C7Mask = 0x0000f000u;
constexpr std::uint32_t kGpio0cIomuxFunc1 = 0x1u;
constexpr std::uint32_t kGpio0cPullOffset = 0x208u;
constexpr std::uint32_t kGpio0cPullC6Shift = 12u;
constexpr std::uint32_t kGpio0cPullC6Mask = 0x00003000u;
constexpr std::uint32_t kGpio0cPullC7Shift = 14u;
constexpr std::uint32_t kGpio0cPullC7Mask = 0x0000c000u;
constexpr std::uint32_t kGpio0cIeOffset = 0x308u;
constexpr std::uint32_t kGpio0cSmtOffset = 0x408u;
constexpr std::uint32_t kGpio0cDs3Offset = 0x12cu;
constexpr std::uint32_t kGpioPullUp = 0x1u;

constexpr Rk3506PlatformAddressSpace kAddressSpace{
    CHARM_RK3506_SYSTEM_SRAM_BASE,
    CHARM_RK3506_SYSTEM_SRAM_SIZE,
    CHARM_RK3506_SDRAM_BASE,
    CHARM_RK3506_SDRAM_SIZE,
    CHARM_RK3506_IMAGE_TEXT_BASE,
};

constexpr Rk3506PlatformMmioLayout kMmioLayout{
    CHARM_RK3506_EARLY_UART_BASE,
    CHARM_RK3506_UART0_BASE,
    CHARM_RK3506_UART4_BASE,
    CHARM_RK3506_GPIO0_IOC_BASE,
    CHARM_RK3506_GICD_BASE,
    CHARM_RK3506_GICC_BASE,
    CHARM_RK3506_GRF_BASE,
    CHARM_RK3506_GRF_PMU_BASE,
    CHARM_RK3506_CRU_BASE,
    CHARM_RK3506_SCRU_BASE,
};

constexpr Rk3506PlatformTiming kTiming{
    CHARM_RK3506_GENERIC_TIMER_FREQUENCY_HZ,
};

Rk3506PlatformResetState g_resetState{};
Rk3506PlatformEarlyConsoleState g_earlyConsoleState{};
Rk3506PlatformGenericTimerSmokeState g_genericTimerSmokeState{};
Rk3506PlatformGicSmokeState g_gicSmokeState{};
Rk3506PlatformIrqTimerSmokeState g_irqTimerSmokeState{};
volatile bool g_irqTimerSmokeActive = false;
volatile bool g_irqTimerSmokeExceptionSeen = false;

inline volatile std::uint32_t& mmio_reg(std::uintptr_t base,
                                        std::uint32_t index) noexcept
{
    return *reinterpret_cast<volatile std::uint32_t*>(
        base + (static_cast<std::uintptr_t>(index) << kUartRegShift));
}

inline volatile std::uint32_t& raw_reg(std::uintptr_t address) noexcept
{
    return *reinterpret_cast<volatile std::uint32_t*>(address);
}

inline std::uint32_t mmio_read(std::uintptr_t base,
                               std::uint32_t offset) noexcept
{
    return raw_reg(base + offset);
}

inline void mmio_write(std::uintptr_t base,
                       std::uint32_t offset,
                       std::uint32_t value) noexcept
{
    raw_reg(base + offset) = value;
}

inline std::uint32_t bit_field(std::uint32_t value,
                               std::uint32_t shift,
                               std::uint32_t mask) noexcept
{
    return (value << shift) & mask;
}

inline void write_masked_lower_16(std::uintptr_t address,
                                  std::uint32_t mask,
                                  std::uint32_t value) noexcept
{
    raw_reg(address) = (mask << 16u) | (value & mask);
}

bool line_bank_bit(std::uint32_t bank, unsigned int intid) noexcept
{
    return ((bank >> (intid % 32u)) & 1u) != 0u;
}

std::uint32_t gic_read_line_bank(std::uint32_t offset,
                                 unsigned int intid) noexcept
{
    return mmio_read(
        kMmioLayout.gic_distributor_base,
        offset + static_cast<std::uint32_t>(4u * (intid / 32u)));
}

void gic_set_group0(unsigned int intid) noexcept
{
    const auto offset =
        kGicdIgrouprOffset + static_cast<std::uint32_t>(4u * (intid / 32u));
    auto value = mmio_read(kMmioLayout.gic_distributor_base, offset);
    value &= ~(1u << (intid % 32u));
    mmio_write(kMmioLayout.gic_distributor_base, offset, value);
}

void gic_set_group1(unsigned int intid) noexcept
{
    const auto offset =
        kGicdIgrouprOffset + static_cast<std::uint32_t>(4u * (intid / 32u));
    auto value = mmio_read(kMmioLayout.gic_distributor_base, offset);
    value |= 1u << (intid % 32u);
    mmio_write(kMmioLayout.gic_distributor_base, offset, value);
}

void gic_set_priority(unsigned int intid, std::uint8_t priority) noexcept
{
    const auto offset =
        kGicdIpriorityrOffset + static_cast<std::uint32_t>(4u * (intid / 4u));
    const auto shift = static_cast<unsigned int>((intid % 4u) * 8u);
    auto value = mmio_read(kMmioLayout.gic_distributor_base, offset);
    value &= ~(0xffu << shift);
    value |= static_cast<std::uint32_t>(priority) << shift;
    mmio_write(kMmioLayout.gic_distributor_base, offset, value);
}

void gic_set_level_triggered(unsigned int intid) noexcept
{
    const auto offset =
        kGicdIcfgrOffset + static_cast<std::uint32_t>(4u * (intid / 16u));
    const auto shift = static_cast<unsigned int>(((intid % 16u) * 2u) + 1u);
    auto value = mmio_read(kMmioLayout.gic_distributor_base, offset);
    value &= ~(1u << shift);
    mmio_write(kMmioLayout.gic_distributor_base, offset, value);
}

void gic_enable_line(unsigned int intid) noexcept
{
    mmio_write(kMmioLayout.gic_distributor_base,
        kGicdIsenablerOffset + static_cast<std::uint32_t>(4u * (intid / 32u)),
        1u << (intid % 32u));
}

void gic_disable_line(unsigned int intid) noexcept
{
    mmio_write(kMmioLayout.gic_distributor_base,
        kGicdIcenablerOffset + static_cast<std::uint32_t>(4u * (intid / 32u)),
        1u << (intid % 32u));
}

void gic_clear_pending(unsigned int intid) noexcept
{
    mmio_write(kMmioLayout.gic_distributor_base,
        kGicdIcpendrOffset + static_cast<std::uint32_t>(4u * (intid / 32u)),
        1u << (intid % 32u));
}

void gic_clear_active(unsigned int intid) noexcept
{
    mmio_write(kMmioLayout.gic_distributor_base,
        kGicdIcactiverOffset + static_cast<std::uint32_t>(4u * (intid / 32u)),
        1u << (intid % 32u));
}

void gic_reset_interfaces() noexcept
{
    mmio_write(kMmioLayout.gic_cpu_interface_base, kGiccCtlrOffset, 0u);
    mmio_write(kMmioLayout.gic_distributor_base, kGicdCtlrOffset, 0u);
}

void gic_prepare_timer_line(unsigned int intid, bool group1) noexcept
{
    gic_disable_line(intid);
    gic_clear_pending(intid);
    gic_clear_active(intid);
    if (group1) {
        gic_set_group1(intid);
    } else {
        gic_set_group0(intid);
    }
    gic_set_level_triggered(intid);
    gic_set_priority(intid, 0x80u);
    gic_enable_line(intid);
}

void gic_prepare_timer_interrupts() noexcept
{
    gic_reset_interfaces();
    gic_prepare_timer_line(kRk3506SecurePhysicalTimerIntId, false);
    gic_prepare_timer_line(kRk3506NonSecurePhysicalTimerIntId, true);
    rk3506::armv7a::data_sync_barrier();
    rk3506::armv7a::instruction_sync_barrier();
}

void gic_release_timer_interrupts() noexcept
{
    gic_disable_line(kRk3506SecurePhysicalTimerIntId);
    gic_disable_line(kRk3506NonSecurePhysicalTimerIntId);
    gic_clear_pending(kRk3506SecurePhysicalTimerIntId);
    gic_clear_pending(kRk3506NonSecurePhysicalTimerIntId);
    gic_clear_active(kRk3506SecurePhysicalTimerIntId);
    gic_clear_active(kRk3506NonSecurePhysicalTimerIntId);
}

void gic_enable_irq_interfaces() noexcept
{
    mmio_write(kMmioLayout.gic_cpu_interface_base, kGiccPmrOffset, 0xffu);
    mmio_write(kMmioLayout.gic_cpu_interface_base, kGiccBprOffset, 0u);
    mmio_write(kMmioLayout.gic_cpu_interface_base,
        kGiccCtlrOffset,
        kGiccCtlrEnableGroup0 | kGiccCtlrEnableGroup1 | kGiccCtlrAckCtl);
    mmio_write(kMmioLayout.gic_distributor_base,
        kGicdCtlrOffset,
        kGicdCtlrEnableGroup0 | kGicdCtlrEnableGroup1);
    rk3506::armv7a::data_sync_barrier();
    rk3506::armv7a::instruction_sync_barrier();
}

void gic_disable_irq_interfaces() noexcept
{
    mmio_write(kMmioLayout.gic_cpu_interface_base, kGiccCtlrOffset, 0u);
    mmio_write(kMmioLayout.gic_distributor_base, kGicdCtlrOffset, 0u);
    rk3506::armv7a::data_sync_barrier();
    rk3506::armv7a::instruction_sync_barrier();
}

std::uint32_t gic_acknowledge_interrupt() noexcept
{
    return mmio_read(kMmioLayout.gic_cpu_interface_base, kGiccIarOffset);
}

void gic_complete_interrupt(std::uint32_t raw_acknowledge) noexcept
{
    mmio_write(
        kMmioLayout.gic_cpu_interface_base, kGiccEoirOffset, raw_acknowledge);
}

bool gic_special_interrupt(unsigned int intid) noexcept
{
    return intid >= kGicSpecialIntIdMin;
}

bool timer_interrupt_intid(unsigned int intid) noexcept
{
    return intid == kRk3506SecurePhysicalTimerIntId ||
        intid == kRk3506NonSecurePhysicalTimerIntId;
}

Rk3506PlatformInterruptControllerState capture_interrupt_controller_state() noexcept
{
    const auto hppir =
        mmio_read(kMmioLayout.gic_cpu_interface_base, kGiccHppirOffset);
    const auto highest_pending_intid = hppir & kGicIntIdMask;
    return Rk3506PlatformInterruptControllerState{
        .distributor_ctlr =
            mmio_read(kMmioLayout.gic_distributor_base, kGicdCtlrOffset),
        .cpu_interface_ctlr =
            mmio_read(kMmioLayout.gic_cpu_interface_base, kGiccCtlrOffset),
        .cpu_interface_pmr =
            mmio_read(kMmioLayout.gic_cpu_interface_base, kGiccPmrOffset),
        .cpu_interface_bpr =
            mmio_read(kMmioLayout.gic_cpu_interface_base, kGiccBprOffset),
        .cpu_interface_hppir = hppir,
        .highest_pending_intid = highest_pending_intid,
        .highest_pending_special = gic_special_interrupt(highest_pending_intid),
    };
}

Rk3506PlatformInterruptLineState capture_interrupt_line_state(
    unsigned int intid) noexcept
{
    if (gic_special_interrupt(intid)) {
        return Rk3506PlatformInterruptLineState{
            .intid = intid,
        };
    }

    const auto group_bank = gic_read_line_bank(kGicdIgrouprOffset, intid);
    const auto enabled_bank = gic_read_line_bank(kGicdIsenablerOffset, intid);
    const auto pending_bank = gic_read_line_bank(kGicdIspendrOffset, intid);
    const auto active_bank = gic_read_line_bank(kGicdIsactiverOffset, intid);
    return Rk3506PlatformInterruptLineState{
        .intid = intid,
        .group_bank = group_bank,
        .enabled_bank = enabled_bank,
        .pending_bank = pending_bank,
        .active_bank = active_bank,
        .line_group1 = line_bank_bit(group_bank, intid),
        .line_enabled = line_bank_bit(enabled_bank, intid),
        .line_pending = line_bank_bit(pending_bank, intid),
        .line_active = line_bank_bit(active_bank, intid),
    };
}

void timer_stop() noexcept
{
    rk3506::armv7a::write_cntp_ctl(kTimerCtrlItMask);
}

void store_count(std::uint32_t& lo_out,
                 std::uint32_t& hi_out,
                 rk3506::armv7a::GenericTimerCount value) noexcept
{
    lo_out = value.lo;
    hi_out = value.hi;
}

void rk3506_uart0_enable_clock_tree() noexcept
{
    const auto cru_base = kMmioLayout.cru_base;
    write_masked_lower_16(
        cru_base + kCruClkselCon29Offset,
        kCruClkselCon29SclkUart0DivMask | kCruClkselCon29SclkUart0SelMask,
        bit_field(1u - 1u,
            kCruClkselCon29SclkUart0DivShift,
            kCruClkselCon29SclkUart0DivMask) |
            bit_field(kCruClkselCon29SclkUart0SelXinOsc0Func,
                kCruClkselCon29SclkUart0SelShift,
                kCruClkselCon29SclkUart0SelMask));

    // Rockchip gate registers use write-enable in the upper 16 bits and 0
    // in the data field means "enable clock".
    write_masked_lower_16(
        cru_base + kCruGateCon11Offset,
        kCruGateCon11HclkLsperiRootMask | kCruGateCon11PclkLsperiRootMask |
            kCruGateCon11HclkLsperiBiuMask | kCruGateCon11PclkUart0Mask |
            kCruGateCon11SclkUart0SrcMask,
        0u);
}

void rk3506_uart0_select_default_pins() noexcept
{
    const auto gpio0_ioc_base = kMmioLayout.gpio0_ioc_base;

    // Public RK3506 DTS/U-Boot data wires UART0 to GPIO0_C6/C7 with func1.
    write_masked_lower_16(
        gpio0_ioc_base + kGpio0cIomuxSel1Offset,
        kGpio0cIomuxSel1C6Mask | kGpio0cIomuxSel1C7Mask,
        bit_field(kGpio0cIomuxFunc1,
            kGpio0cIomuxSel1C6Shift,
            kGpio0cIomuxSel1C6Mask) |
            bit_field(kGpio0cIomuxFunc1,
                kGpio0cIomuxSel1C7Shift,
                kGpio0cIomuxSel1C7Mask));

    write_masked_lower_16(
        gpio0_ioc_base + kGpio0cPullOffset,
        kGpio0cPullC6Mask | kGpio0cPullC7Mask,
        bit_field(kGpioPullUp, kGpio0cPullC6Shift, kGpio0cPullC6Mask) |
            bit_field(kGpioPullUp, kGpio0cPullC7Shift, kGpio0cPullC7Mask));
}

std::uint32_t rk3506_uart_calculate_divisor(std::uint32_t input_clock_hz,
                                            std::uint32_t baud_rate) noexcept
{
    std::uint32_t divisor = 1u;
    if (baud_rate != 0u) {
        const auto denominator = kUartModeXDiv * baud_rate;
        const auto computed = input_clock_hz / denominator;
        divisor = computed == 0u ? 1u : computed;
    }

    return divisor;
}

std::uint32_t rk3506_uart_configure_8n1(std::uintptr_t uart_base,
                                        std::uint32_t input_clock_hz,
                                        std::uint32_t baud_rate) noexcept
{
    const auto divisor =
        rk3506_uart_calculate_divisor(input_clock_hz, baud_rate);

    mmio_reg(uart_base, kUartSrrIndex) = kUartSrrUr | kUartSrrRfr | kUartSrrXfr;
    mmio_reg(uart_base, kUartDmasaIndex) = 1u;
    mmio_reg(uart_base, kUartDlhIndex) = 0u;
    mmio_reg(uart_base, kUartFcrIndex) = kUartFcrEnableFifo |
        kUartFcrClearRcvr | kUartFcrClearXmit | kUartFcrRTrig10 |
        kUartFcrTTrig10;
    mmio_reg(uart_base, kUartLcrIndex) = kUartLcrWlen8;
    mmio_reg(uart_base, kUartMcrIndex) |= kUartMcrLoop;
    mmio_reg(uart_base, kUartLcrIndex) |= kUartLcrDlab;
    mmio_reg(uart_base, kUartDllIndex) = divisor & 0xffu;
    mmio_reg(uart_base, kUartDlhIndex) = (divisor >> 8u) & 0xffu;
    mmio_reg(uart_base, kUartLcrIndex) &= ~kUartLcrDlab;
    mmio_reg(uart_base, kUartMcrIndex) &= ~kUartMcrLoop;

    return divisor;
}

void rk3506_uart0_capture_local_init_state(
    Rk3506PlatformEarlyConsoleState& state) noexcept
{
    const auto cru_base = kMmioLayout.cru_base;
    const auto gpio0_ioc_base = kMmioLayout.gpio0_ioc_base;

    state.cru_clksel_con29 = raw_reg(cru_base + kCruClkselCon29Offset);
    state.cru_gate_con11 = raw_reg(cru_base + kCruGateCon11Offset);
    state.gpio0c_iomux_sel1 = raw_reg(gpio0_ioc_base + kGpio0cIomuxSel1Offset);
    state.gpio0c_pull = raw_reg(gpio0_ioc_base + kGpio0cPullOffset);
    state.gpio0c_ie = raw_reg(gpio0_ioc_base + kGpio0cIeOffset);
    state.gpio0c_smt = raw_reg(gpio0_ioc_base + kGpio0cSmtOffset);
    state.gpio0c_ds3 = raw_reg(gpio0_ioc_base + kGpio0cDs3Offset);
}

void rk3506_capture_generic_timer_smoke_state(
    Rk3506PlatformGenericTimerSmokeState& state) noexcept
{
    state = {};
    state.mpidr = rk3506::armv7a::read_mpidr();
    state.id_pfr1 = rk3506::armv7a::read_id_pfr1();
    state.generic_timer_present =
        rk3506::armv7a::generic_timer_present(state.id_pfr1);
    state.counter_frequency_hz = rk3506::armv7a::read_cntfrq();
    const auto first = rk3506::armv7a::read_cntpct();
    const auto second = rk3506::armv7a::read_cntpct();
    state.first_count_lo = first.lo;
    state.first_count_hi = first.hi;
    state.second_count_lo = second.lo;
    state.second_count_hi = second.hi;
    state.counter_advanced = rk3506::armv7a::count_advanced(first, second);
    state.counter_frequency_matches_target =
        state.counter_frequency_hz == CHARM_RK3506_GENERIC_TIMER_FREQUENCY_HZ;
    state.probed = true;
}

void rk3506_capture_gic_smoke_state(Rk3506PlatformGicSmokeState& state) noexcept
{
    state = {};

    const auto gicd_base = kMmioLayout.gic_distributor_base;
    const auto gicc_base = kMmioLayout.gic_cpu_interface_base;

    state.distributor_ctlr = mmio_read(gicd_base, kGicdCtlrOffset);
    state.distributor_typer = mmio_read(gicd_base, kGicdTyperOffset);
    state.distributor_iidr = mmio_read(gicd_base, kGicdIidrOffset);
    state.cpu_interface_ctlr = mmio_read(gicc_base, kGiccCtlrOffset);
    state.cpu_interface_pmr = mmio_read(gicc_base, kGiccPmrOffset);
    state.cpu_interface_hppir = mmio_read(gicc_base, kGiccHppirOffset);
    state.cpu_interface_iidr = mmio_read(gicc_base, kGiccIidrOffset);

    state.implemented_interrupts = 32u *
        ((state.distributor_typer & kGicdTyperItLinesNumberMask) + 1u);
    state.cpu_interface_count =
        ((state.distributor_typer & kGicdTyperCpuNumberMask) >>
            kGicdTyperCpuNumberShift) +
        1u;
    state.probed = true;
}
} // namespace

const Rk3506PlatformAddressSpace& rk3506_platform_address_space()
{
    return kAddressSpace;
}

const Rk3506PlatformMmioLayout& rk3506_platform_mmio_layout()
{
    return kMmioLayout;
}

const Rk3506PlatformTiming& rk3506_platform_timing()
{
    return kTiming;
}

const Rk3506PlatformResetState& rk3506_platform_reset_state()
{
    return g_resetState;
}

const Rk3506PlatformEarlyConsoleState& rk3506_platform_early_console_state()
{
    return g_earlyConsoleState;
}

const Rk3506PlatformGenericTimerSmokeState&
rk3506_platform_generic_timer_smoke_state()
{
    return g_genericTimerSmokeState;
}

const Rk3506PlatformGicSmokeState& rk3506_platform_gic_smoke_state()
{
    return g_gicSmokeState;
}

const Rk3506PlatformIrqTimerSmokeState& rk3506_platform_irq_timer_smoke_state()
{
    return g_irqTimerSmokeState;
}

const char* rk3506_platform_interrupt_source_name(unsigned int intid)
{
    switch (intid) {
    case kRk3506SecurePhysicalTimerIntId:
        return "secure-phys-ppi";
    case kRk3506NonSecurePhysicalTimerIntId:
        return "nonsecure-phys-ppi";
    default:
        return gic_special_interrupt(intid) ? "special-intid" : "unexpected-intid";
    }
}

extern "C" void rk3506_platform_early_console_init()
{
    g_earlyConsoleState = {};
    g_earlyConsoleState.configured_uart_base = kMmioLayout.early_console_base;
    g_earlyConsoleState.configured_baud_rate =
        CHARM_RK3506_EARLY_UART_BAUD_RATE;

    if (kMmioLayout.early_console_base != kMmioLayout.uart0_base) {
        g_earlyConsoleState.requires_preconfigured_console = true;
        return;
    }

    g_earlyConsoleState.attempted_local_init = true;
    rk3506_uart0_enable_clock_tree();
    rk3506_uart0_select_default_pins();
    g_earlyConsoleState.configured_divisor =
        rk3506_uart_configure_8n1(kMmioLayout.uart0_base,
            CHARM_RK3506_XIN_OSC_HZ,
            CHARM_RK3506_EARLY_UART_BAUD_RATE);
    rk3506_uart0_capture_local_init_state(g_earlyConsoleState);

    g_earlyConsoleState.completed_local_init = true;
    g_earlyConsoleState.requires_preconfigured_console = false;
    g_earlyConsoleState.configured_clock_hz = CHARM_RK3506_XIN_OSC_HZ;
}

extern "C" void rk3506_platform_early_console_putc(char ch)
{
    while ((mmio_reg(kMmioLayout.early_console_base, kUartLsrIndex) &
            kUartLsrThre) == 0u) {
    }
    mmio_reg(kMmioLayout.early_console_base, kUartThrIndex) =
        static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
}

extern "C" void rk3506_platform_early_console_puts(const char* text)
{
    if (!text) {
        return;
    }

    while (*text != '\0') {
        if (*text == '\n') {
            rk3506_platform_early_console_putc('\r');
        }
        rk3506_platform_early_console_putc(*text++);
    }
}

extern "C" void rk3506_platform_capture_read_only_smoke()
{
    rk3506_capture_generic_timer_smoke_state(g_genericTimerSmokeState);
    rk3506_capture_gic_smoke_state(g_gicSmokeState);
}

extern "C" void rk3506_platform_run_irq_timer_smoke()
{
    g_irqTimerSmokeState = {};
    g_irqTimerSmokeState.attempted = true;
    g_irqTimerSmokeState.expected_intid = kRk3506ExpectedGenericTimerIntId;
    g_irqTimerSmokeState.generic_timer_available =
        rk3506::armv7a::generic_timer_present(
            rk3506::armv7a::read_id_pfr1()) &&
        rk3506::armv7a::read_cntfrq() != 0u;
    g_irqTimerSmokeState.gic_available =
        kMmioLayout.gic_distributor_base != 0u &&
        kMmioLayout.gic_cpu_interface_base != 0u;

    if (!g_irqTimerSmokeState.generic_timer_available ||
        !g_irqTimerSmokeState.gic_available) {
        return;
    }

    auto ticks = kTiming.generic_timer_frequency_hz / 200u;
    if (ticks < 0x1000u) {
        ticks = 0x1000u;
    }

    const auto start = rk3506::armv7a::read_cntpct();
    const auto pending_deadline = rk3506::armv7a::add_ticks(
        start,
        kTiming.generic_timer_frequency_hz != 0u
            ? (kTiming.generic_timer_frequency_hz / 20u)
            : 0x100000u);
    const auto timeout_deadline = rk3506::armv7a::add_ticks(
        start,
        kTiming.generic_timer_frequency_hz != 0u
            ? kTiming.generic_timer_frequency_hz
            : 0x100000u);

    g_irqTimerSmokeState.programmed_ticks = ticks;
    store_count(g_irqTimerSmokeState.start_count_lo,
        g_irqTimerSmokeState.start_count_hi,
        start);

    rk3506::armv7a::disable_irq();
    rk3506::armv7a::compiler_barrier();
    g_irqTimerSmokeExceptionSeen = false;
    g_irqTimerSmokeActive = true;
    rk3506::armv7a::compiler_barrier();

    timer_stop();
    gic_disable_irq_interfaces();
    gic_release_timer_interrupts();
    gic_prepare_timer_interrupts();
    gic_enable_irq_interfaces();

    g_irqTimerSmokeState.timer_control_before_start =
        rk3506::armv7a::read_cntp_ctl();
    rk3506::armv7a::write_cntp_tval(ticks);
    rk3506::armv7a::write_cntp_ctl(kTimerCtrlEnable);
    g_irqTimerSmokeState.timer_control_after_start =
        rk3506::armv7a::read_cntp_ctl();

    while (!g_irqTimerSmokeState.pending_seen) {
        ++g_irqTimerSmokeState.pending_poll_count;
        const auto now = rk3506::armv7a::read_cntpct();
        const auto control = rk3506::armv7a::read_cntp_ctl();
        if ((control & kTimerCtrlItStatus) != 0u) {
            g_irqTimerSmokeState.pending_seen = true;
            g_irqTimerSmokeState.timer_control_pending_snapshot = control;
            store_count(g_irqTimerSmokeState.pending_count_lo,
                g_irqTimerSmokeState.pending_count_hi,
                now);
            break;
        }
        if (rk3506::armv7a::count_at_or_after(now, pending_deadline)) {
            g_irqTimerSmokeState.timer_control_pending_snapshot = control;
            store_count(g_irqTimerSmokeState.pending_count_lo,
                g_irqTimerSmokeState.pending_count_hi,
                now);
            break;
        }
    }

    rk3506::armv7a::enable_irq();
    while (!g_irqTimerSmokeExceptionSeen) {
        ++g_irqTimerSmokeState.irq_poll_count;
        const auto now = rk3506::armv7a::read_cntpct();
        if (rk3506::armv7a::count_at_or_after(now, timeout_deadline)) {
            store_count(g_irqTimerSmokeState.finish_count_lo,
                g_irqTimerSmokeState.finish_count_hi,
                now);
            break;
        }
    }

    rk3506::armv7a::disable_irq();
    rk3506::armv7a::compiler_barrier();
    if (g_irqTimerSmokeExceptionSeen &&
        g_irqTimerSmokeState.finish_count_lo == 0u &&
        g_irqTimerSmokeState.finish_count_hi == 0u) {
        store_count(g_irqTimerSmokeState.finish_count_lo,
            g_irqTimerSmokeState.finish_count_hi,
            rk3506::armv7a::read_cntpct());
    }

    g_irqTimerSmokeState.irq_exception_seen = g_irqTimerSmokeExceptionSeen;
    g_irqTimerSmokeState.timed_out = !g_irqTimerSmokeExceptionSeen;
    g_irqTimerSmokeState.timer_control_after_handler =
        rk3506::armv7a::read_cntp_ctl();
    if (!g_irqTimerSmokeExceptionSeen) {
        g_irqTimerSmokeState.controller = capture_interrupt_controller_state();
    }
    g_irqTimerSmokeState.secure_timer_line =
        capture_interrupt_line_state(kRk3506SecurePhysicalTimerIntId);
    g_irqTimerSmokeState.nonsecure_timer_line =
        capture_interrupt_line_state(kRk3506NonSecurePhysicalTimerIntId);

    timer_stop();
    g_irqTimerSmokeState.timer_control_after_stop =
        rk3506::armv7a::read_cntp_ctl();
    gic_disable_irq_interfaces();
    gic_release_timer_interrupts();

    rk3506::armv7a::compiler_barrier();
    g_irqTimerSmokeActive = false;
    rk3506::armv7a::compiler_barrier();
}

extern "C" bool rk3506_platform_handle_irq_exception(
    const Rk3506ExceptionFrame* frame)
{
    if (!g_irqTimerSmokeActive || frame == nullptr) {
        return false;
    }

    g_irqTimerSmokeState.controller = capture_interrupt_controller_state();
    const auto raw_acknowledge = gic_acknowledge_interrupt();
    const auto intid = raw_acknowledge & kGicIntIdMask;

    g_irqTimerSmokeState.raw_acknowledge = raw_acknowledge;
    g_irqTimerSmokeState.observed_intid = intid;
    g_irqTimerSmokeState.acknowledge_special = gic_special_interrupt(intid);
    g_irqTimerSmokeState.observed_line = capture_interrupt_line_state(intid);
    g_irqTimerSmokeState.handler_cpsr = rk3506::armv7a::read_cpsr();
    g_irqTimerSmokeState.handler_spsr = frame->spsr;
    g_irqTimerSmokeState.return_pc = rk3506_exception_return_pc(*frame);
    g_irqTimerSmokeState.timer_source_recognized =
        !g_irqTimerSmokeState.acknowledge_special &&
        timer_interrupt_intid(intid);
    g_irqTimerSmokeState.matches_expected_intid =
        !g_irqTimerSmokeState.acknowledge_special &&
        intid == g_irqTimerSmokeState.expected_intid;

    if (!g_irqTimerSmokeState.acknowledge_special) {
        timer_stop();
        g_irqTimerSmokeState.timer_control_after_handler =
            rk3506::armv7a::read_cntp_ctl();
        gic_complete_interrupt(raw_acknowledge);
    } else {
        g_irqTimerSmokeState.timer_control_after_handler =
            rk3506::armv7a::read_cntp_ctl();
    }

    store_count(g_irqTimerSmokeState.finish_count_lo,
        g_irqTimerSmokeState.finish_count_hi,
        rk3506::armv7a::read_cntpct());
    rk3506::armv7a::compiler_barrier();
    g_irqTimerSmokeExceptionSeen = true;
    rk3506::armv7a::compiler_barrier();
    return true;
}

extern "C" void rk3506_platform_reset_early(std::uint32_t entry_cpsr)
{
    g_resetState.initial_cpsr = entry_cpsr;
    g_resetState.post_entry_mask_cpsr = rk3506::armv7a::read_cpsr();
    g_resetState.initial_sctlr = rk3506::armv7a::read_sctlr();
    g_resetState.initial_ttbr0 = rk3506::armv7a::read_ttbr0();
    g_resetState.initial_ttbcr = rk3506::armv7a::read_ttbcr();
    g_resetState.initial_dacr = rk3506::armv7a::read_dacr();
    g_resetState.initial_vbar = rk3506::armv7a::read_vbar();
    g_resetState.initial_effective_vector_base = rk3506::armv7a::vector_base(
        g_resetState.initial_sctlr, g_resetState.initial_vbar);
    g_resetState.forced_low_vectors =
        rk3506::armv7a::high_vectors(g_resetState.initial_sctlr);

    const auto sctlr =
        g_resetState.initial_sctlr & ~rk3506::armv7a::kSctlrHighVectors;
    rk3506::armv7a::write_sctlr(sctlr);
    rk3506::armv7a::data_sync_barrier();
    rk3506::armv7a::instruction_sync_barrier();
}

extern "C" void rk3506_platform_install_exception_vectors(const void* vector_base)
{
    const auto sctlr =
        rk3506::armv7a::read_sctlr() & ~rk3506::armv7a::kSctlrHighVectors;
    rk3506::armv7a::write_vbar(reinterpret_cast<std::uintptr_t>(vector_base));
    rk3506::armv7a::write_sctlr(sctlr);
    rk3506::armv7a::data_sync_barrier();
    rk3506::armv7a::instruction_sync_barrier();
}

extern "C" [[noreturn]] void rk3506_platform_idle_forever()
{
    for (;;) {
        asm volatile("wfe");
    }
}
