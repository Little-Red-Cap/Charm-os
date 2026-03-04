#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

#include "stm32h7xx_hal.h"

/*
LED
    PA3   ------> LED Green Enable:low
    PB1   ------> LED Blue  Enable:low

KEY (hardware pulled up)
    PA2   ------> PWR_WKUP2 Enable:high
    PA8   ------> KEY0      Enable:high

UART
    PA10     ------> USART1_RX
    PA9     ------> USART1_TX

TF Card (hardware pulled up)
    PC10     ------> SDMMC1_D2
    PC11     ------> SDMMC1_D3
    PC12     ------> SDMMC1_CK
    PD2     ------> SDMMC1_CMD
    PC8     ------> SDMMC1_D0
    PC9     ------> SDMMC1_D1

Audio
    PB5     ------> I2S1_SDO
    PG10     ------> I2S1_WS
    PA5     ------> I2S1_CK
    PC4     ------> I2S1_MCK

Debug
    PA14 (JTCK/SWCLK)   ------> DEBUG_JTCK-SWCLK
    PA13 (JTMS/SWDIO)   ------> DEBUG_JTMS-SWDIO

SDRAM

SD NAND W25Q256
    PF6
    PF7
    PF8
    PF9
    PF10
 */

import charm.system.bringup;
import charm.system.clock;
import charm.system.reactor_pump;
import driver.usart_channel;
import io.channel;
import io.registry;
import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.scheduler;
import out.channel;
import player.stm32h7.audio_mp3_demo;
import player.stm32h7.fs_demo;
import platform.board.stn32h747xi;
import util.core;
import util.error;

extern "C" {
    void SystemClock_Config(void);
    void MX_GPIO_Init(void);
    void MX_DMA_Init(void);
    void MX_I2S1_Init(void);
    void MX_SDMMC1_SD_Init(void);
    void MX_USART1_UART_Init(void);
    void Error_Handler(void);
}

namespace {
    constexpr util::usize kRxCap = 64;
    constexpr util::usize kTxCap = 640;
    constexpr bool kKeyActiveHigh = true;
    driver::usart::ChannelAdapter<kRxCap, kTxCap>* g_uart_adapter = nullptr;

    struct PumpConfig : kernel::KernelConfig {
        static constexpr std::size_t priority_levels = 1;
        static constexpr std::size_t evtq_capacity = 8;
    };

    void hqzy_gpio_init() noexcept {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();
        __HAL_RCC_GPIOE_CLK_ENABLE();

        GPIO_InitTypeDef gpio_init = {};

        /* SDMMC1: PC8/PC9/PC10/PC11/PC12 + PD2 (external pull-ups). */
        gpio_init.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = GPIO_AF12_SDIO1;
        HAL_GPIO_Init(GPIOC, &gpio_init);

        gpio_init.Pin = GPIO_PIN_2;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = GPIO_AF12_SDIO1;
        HAL_GPIO_Init(GPIOD, &gpio_init);

        /* Keys: PA2 (WKUP2), PA8 (KEY0), active high (external pull-ups). */
        gpio_init.Pin = GPIO_PIN_2;
        gpio_init.Mode = GPIO_MODE_INPUT;
        gpio_init.Pull = GPIO_PULLDOWN;
        gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
        gpio_init.Alternate = 0;
        HAL_GPIO_Init(GPIOA, &gpio_init);

        gpio_init.Pin = GPIO_PIN_8;
        HAL_GPIO_Init(GPIOA, &gpio_init);
    }
}

extern "C" void USART1_IRQHandler(void) {
    if (g_uart_adapter) {
        g_uart_adapter->on_irq();
    }
}

int main() {
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    // MX_SDMMC1_SD_Init();
    // MX_I2S1_Init();

    hqzy_gpio_init();

    auto caps = platform::board::stn32h747xi::make_board_caps();
    using PumpTask = charm::system::ReactorPumpTask;
    using Registry = kernel::TaskRegistry<PumpTask>;

    struct PumpCaps {
        using TimeSource = charm::system::ClockCaps::TimeSource;
        using IrqGuard = kernel::NoopIrqGuard;
        using Wakeup = kernel::NoopWakeup;
        using SwiTrigger = kernel::NoopSwiTrigger;
    };

    Registry registry{};
    PumpCaps pump_caps{};
    auto created = kernel::make_scheduler<PumpConfig>(registry, pump_caps);
    auto running = kernel::start(std::move(created));
    const auto pump_id = Registry::id_of<PumpTask>();
    auto& pump = registry.get<PumpTask>();

    using SchedulerT = decltype(running);
    auto post_fn = static_cast<charm::system::PostFn>(
        &charm::system::scheduler_post<SchedulerT>
    );
    charm::system::BringupMinimal<8, 16, 8, kRxCap, kTxCap> bringup{
        caps.uart1,
        caps.clock,
        caps.input,
        pump,
        post_fn,
        &running,
        pump_id,
        8
    };

    auto r = bringup.start();
    if (!r) {
        Error_Handler();
    }

    auto* ch = bringup.registry().open_channel("io.uart1");
    if (!ch) {
        Error_Handler();
    }
    g_uart_adapter = static_cast<driver::usart::ChannelAdapter<kRxCap, kTxCap>*>(ch->ctx);

    io::EndpointDesc console_desc{
        "io.console0",
        io::cap_id("io.console0"),
        io::EndpointKind::channel,
        io::EndpointCaps::duplex
    };
    auto& reg = bringup.registry();
    auto* console_ep = reg.find_channel("io.console0");
    auto r_console = console_ep
        ? reg.replace_channel(console_desc, *ch, &bringup.reactor())
        : reg.register_channel(console_desc, *ch, &bringup.reactor());
    if (!r_console) {
        Error_Handler();
    }
    static out::channel_sink console_sink = out::make_channel_sink(*ch);
    fs_set_console_sink(console_sink);
    audio_set_console_sink(console_sink);

    const char msg[] = "bringup ok\n";
    auto wr = ch->write(io::ByteView{
        reinterpret_cast<const util::u8*>(msg),
        sizeof(msg) - 1
    });
    if (!wr && wr.error() != util::Errc::would_block) {
        Error_Handler();
    }

    if (!fs_boot_init()) {
        const char fail_msg[] = "fs init failed\n";
        (void)ch->write(io::ByteView{
            reinterpret_cast<const util::u8*>(fail_msg),
            sizeof(fail_msg) - 1
        });
        Error_Handler();
    }

    {
        const char wait_msg[] = "boot: wait key\n";
        (void)ch->write(io::ByteView{
            reinterpret_cast<const util::u8*>(wait_msg),
            sizeof(wait_msg) - 1
        });
        constexpr GPIO_PinState active = kKeyActiveHigh ? GPIO_PIN_SET : GPIO_PIN_RESET;
        while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) != active) {
            HAL_Delay(10);
        }
        HAL_Delay(20);
    }

    audio_mp3_demo_run();

    while (true) {
        (void)running.run_once();
    }
}
