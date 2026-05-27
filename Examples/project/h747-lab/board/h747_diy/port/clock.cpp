#include "main.h"

#include <cstdint>

extern "C" void SystemClock_Config(void);

extern "C" {

struct h747_clock_probe_t {
    std::uint32_t magic;
    std::uint32_t stage;
    std::uint32_t osc_status;
    std::uint32_t clock_status;
    std::uint32_t rcc_cr;
    std::uint32_t rcc_cfgr;
    std::uint32_t rcc_d1cfgr;
    std::uint32_t rcc_d2cfgr;
    std::uint32_t rcc_d3cfgr;
    std::uint32_t rcc_pllckselr;
    std::uint32_t rcc_pllcfgr;
    std::uint32_t rcc_pll1divr;
    std::uint32_t flash_acr;
    std::uint32_t systick_ctrl;
    std::uint32_t systick_load;
    std::uint32_t systick_val;
    std::uint32_t system_core_clock;
    std::uint32_t system_d2_clock;
    std::uint32_t uw_tick;
    std::uint32_t uw_tick_prio;
};

volatile h747_clock_probe_t g_h747_clock_probe{};

}

namespace {

constexpr std::uint32_t kClockProbeMagic = 0x434C4B31U; // CLK1

void clock_probe_snapshot(const std::uint32_t stage) noexcept {
    g_h747_clock_probe.magic = kClockProbeMagic;
    g_h747_clock_probe.stage = stage;
    g_h747_clock_probe.rcc_cr = RCC->CR;
    g_h747_clock_probe.rcc_cfgr = RCC->CFGR;
    g_h747_clock_probe.rcc_d1cfgr = RCC->D1CFGR;
    g_h747_clock_probe.rcc_d2cfgr = RCC->D2CFGR;
    g_h747_clock_probe.rcc_d3cfgr = RCC->D3CFGR;
    g_h747_clock_probe.rcc_pllckselr = RCC->PLLCKSELR;
    g_h747_clock_probe.rcc_pllcfgr = RCC->PLLCFGR;
    g_h747_clock_probe.rcc_pll1divr = RCC->PLL1DIVR;
    g_h747_clock_probe.flash_acr = FLASH->ACR;
    g_h747_clock_probe.systick_ctrl = SysTick->CTRL;
    g_h747_clock_probe.systick_load = SysTick->LOAD;
    g_h747_clock_probe.systick_val = SysTick->VAL;
    g_h747_clock_probe.system_core_clock = SystemCoreClock;
    g_h747_clock_probe.system_d2_clock = SystemD2Clock;
    g_h747_clock_probe.uw_tick = uwTick;
    g_h747_clock_probe.uw_tick_prio = uwTickPrio;
}

} // namespace

void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc{};
    RCC_ClkInitTypeDef clk{};

    clock_probe_snapshot(1U);

    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    clock_probe_snapshot(2U);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 5;
    osc.PLL.PLLN = 192;
    osc.PLL.PLLP = 2;
    osc.PLL.PLLQ = 15;
    osc.PLL.PLLR = 2;
    osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    osc.PLL.PLLFRACN = 0;
    g_h747_clock_probe.osc_status = static_cast<std::uint32_t>(HAL_RCC_OscConfig(&osc));
    clock_probe_snapshot(3U);
    if (g_h747_clock_probe.osc_status != static_cast<std::uint32_t>(HAL_OK)) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                    RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider = RCC_SYSCLK_DIV1;
    clk.AHBCLKDivider = RCC_HCLK_DIV2;
    clk.APB1CLKDivider = RCC_APB1_DIV2;
    clk.APB2CLKDivider = RCC_APB2_DIV2;
    clk.APB3CLKDivider = RCC_APB3_DIV2;
    clk.APB4CLKDivider = RCC_APB4_DIV2;

    clock_probe_snapshot(4U);
    g_h747_clock_probe.clock_status =
        static_cast<std::uint32_t>(HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4));
    clock_probe_snapshot(5U);
    if (g_h747_clock_probe.clock_status != static_cast<std::uint32_t>(HAL_OK)) {
        Error_Handler();
    }

    clock_probe_snapshot(6U);
}
