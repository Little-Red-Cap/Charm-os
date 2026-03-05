#include <array>
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
    PA7     ------> I2S1_SDO
    PA5     ------> I2S1_CK
    PC4     ------> I2S1_MCK
    PA4     ------> I2S1_WS

Mono TFT
    PK0     ------> SPI5_SCK
    PK1     ------> SPI5_NSS
    PJ10     ------> SPI5_MOSI
    PJ5     ------> GPIO RST
    PJ6     ------> GPIO DATA/CMD

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
import out.api;
import out.channel;
import player.stm32h7.audio_mp3_demo;
import player.stm32h7.display_st7305;
import player.stm32h7.fs_demo;
import player.stm32h7.ink_demo;
import platform.board.stn32h747xi;
import util.core;
import util.error;

extern "C" {
    void SystemClock_Config(void);
    void MX_GPIO_Init(void);
    void MX_DMA_Init(void);
    void MX_I2S1_Init(void);
    void MX_SDMMC1_SD_Init(void);
    void MX_SPI5_Init(void);
    void MX_USART1_UART_Init(void);
    void Error_Handler(void);
    extern UART_HandleTypeDef huart1;
}

namespace {
    constexpr util::usize kRxCap = 64;
    constexpr util::usize kTxCap = 640;
    constexpr bool kKeyActiveHigh = true;
    constexpr bool kBringupKeySelect = true;
    constexpr bool kBringupWaitKey = true;
    driver::usart::ChannelAdapter<kRxCap, kTxCap>* g_uart_adapter = nullptr;

    struct PumpConfig : kernel::KernelConfig {
        static constexpr std::size_t priority_levels = 1;
        static constexpr std::size_t evtq_capacity = 8;
    };

    struct DmaUartTx {
        static constexpr std::size_t kBufSize = 2048;
        static constexpr util::u32 kDmaTimeoutMs = 20;

        std::array<util::u8, kBufSize> buf{};
        volatile std::uint16_t head{0};
        volatile std::uint16_t tail{0};
        volatile std::uint16_t dma_len{0};
        volatile bool dma_busy{false};
        volatile bool dma_failed{false};
        volatile util::u32 dma_start_ms{0};

        bool push(util::u8 byte) noexcept {
            __disable_irq();
            const std::uint16_t next = static_cast<std::uint16_t>((head + 1u) % kBufSize);
            if (next == tail) {
                __enable_irq();
                return false;
            }
            buf[head] = byte;
            head = next;
            const bool need_kick = !dma_busy;
            __enable_irq();
            if (need_kick) {
                kick();
            }
            return true;
        }

        bool idle() const noexcept {
            return !dma_busy && (head == tail);
        }

        void check_timeout() noexcept {
            if (dma_failed || !dma_busy) return;
            const util::u32 now = HAL_GetTick();
            if ((now - dma_start_ms) <= kDmaTimeoutMs) return;
            __disable_irq();
            dma_failed = true;
            dma_busy = false;
            dma_len = 0;
            __enable_irq();
            (void)HAL_UART_AbortTransmit(&huart1);
            flush_blocking();
        }

        void kick() noexcept {
            __disable_irq();
            if (dma_failed) {
                __enable_irq();
                return;
            }
            if (dma_busy || head == tail) {
                __enable_irq();
                return;
            }
            std::uint16_t len = 0;
            if (head > tail) {
                len = static_cast<std::uint16_t>(head - tail);
            } else {
                len = static_cast<std::uint16_t>(kBufSize - tail);
            }
            if (len == 0) {
                __enable_irq();
                return;
            }
            dma_len = len;
            dma_busy = true;
            __enable_irq();
            if (HAL_UART_Transmit_DMA(&huart1, &buf[tail], len) != HAL_OK) {
                if (HAL_UART_Transmit(&huart1, &buf[tail], len, 100) == HAL_OK) {
                    __disable_irq();
                    tail = static_cast<std::uint16_t>((tail + len) % kBufSize);
                    dma_len = 0;
                    dma_busy = false;
                    __enable_irq();
                    return;
                }
                __disable_irq();
                dma_busy = false;
                dma_len = 0;
                dma_failed = true;
                __enable_irq();
            }
            dma_start_ms = HAL_GetTick();
        }

        void on_complete() noexcept {
            __disable_irq();
            if (dma_len > 0) {
                tail = static_cast<std::uint16_t>((tail + dma_len) % kBufSize);
            }
            dma_len = 0;
            dma_busy = false;
            __enable_irq();
            kick();
        }

        void flush_blocking() noexcept {
            while (true) {
                __disable_irq();
                if (head == tail) {
                    __enable_irq();
                    return;
                }
                const std::uint16_t start = tail;
                const std::uint16_t len = (head > tail)
                    ? static_cast<std::uint16_t>(head - tail)
                    : static_cast<std::uint16_t>(kBufSize - tail);
                __enable_irq();
                if (HAL_UART_Transmit(&huart1, &buf[start], len, 100) != HAL_OK) {
                    __disable_irq();
                    tail = head;
                    __enable_irq();
                    return;
                }
                __disable_irq();
                tail = static_cast<std::uint16_t>((start + len) % kBufSize);
                __enable_irq();
            }
        }
    };

    struct DmaConsoleCtx {
        io::Channel* rx_channel{nullptr};
        DmaUartTx* tx{nullptr};
    };

    static DmaUartTx g_uart1_dma_tx{};
    static io::Channel g_dma_console{};
    static DmaConsoleCtx g_dma_ctx{};

    io::result dma_read_trampoline(void* ctx, io::MutByteView buf) noexcept {
        auto* self = static_cast<DmaConsoleCtx*>(ctx);
        if (!self || !self->rx_channel) return io::fail(io::errc::invalid_arg);
        return self->rx_channel->read(buf);
    }

    io::result dma_write_trampoline(void* ctx, io::ByteView buf) noexcept {
        auto* self = static_cast<DmaConsoleCtx*>(ctx);
        if (!self || !self->tx) return io::fail(io::errc::invalid_arg);
        if (buf.empty()) return io::fail(io::errc::invalid_arg);
        self->tx->check_timeout();
        if (self->tx->dma_failed) {
            if (HAL_UART_Transmit(&huart1,
                    const_cast<util::u8*>(buf.data()),
                    static_cast<uint16_t>(buf.size()),
                    100) != HAL_OK) {
                return io::fail(io::errc::io_error);
            }
            return io::ok(buf.size());
        }
        util::usize pushed = 0;
        for (util::usize i = 0; i < buf.size(); ++i) {
            if (!self->tx->push(buf.data()[i])) {
                break;
            }
            ++pushed;
        }
        if (pushed == 0) return io::fail(io::errc::would_block);
        return io::ok(pushed);
    }

    io::result dma_flush_trampoline(void* ctx) noexcept {
        auto* self = static_cast<DmaConsoleCtx*>(ctx);
        if (!self || !self->tx) return io::fail(io::errc::invalid_arg);
        if (!self->tx->idle()) return io::fail(io::errc::would_block);
        return io::ok(0);
    }

    const io::ChannelOps kDmaConsoleOps{
        .read = &dma_read_trampoline,
        .write = &dma_write_trampoline,
        .flush = &dma_flush_trampoline,
    };

    void hqzy_gpio_init() noexcept {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();
        __HAL_RCC_GPIOE_CLK_ENABLE();
        __HAL_RCC_GPIOJ_CLK_ENABLE();
        __HAL_RCC_GPIOK_CLK_ENABLE();

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

        /* Display control: PJ5 reset, PJ6 data/cmd. */
        gpio_init.Pin = GPIO_PIN_5 | GPIO_PIN_6;
        gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
        gpio_init.Alternate = 0;
        HAL_GPIO_Init(GPIOJ, &gpio_init);
        HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);

        /* Display chip select: PK1. */
        gpio_init.Pin = GPIO_PIN_1;
        gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
        gpio_init.Alternate = 0;
        HAL_GPIO_Init(GPIOK, &gpio_init);
        HAL_GPIO_WritePin(GPIOK, GPIO_PIN_1, GPIO_PIN_SET);
    }

    enum class BringupMode : std::uint8_t {
        sd = 0,
        decode = 1,
        i2s = 2,
        full = 3
    };

    constexpr BringupMode kDefaultBringupMode = BringupMode::full;
    constexpr util::u32 kSdSelfStartLba = 0;
    constexpr util::u32 kSdSelfBlocks = 128;
    constexpr util::u32 kSdSelfStride = 1;
    constexpr util::u32 kI2sSelfMs = 2000;

    constexpr GPIO_PinState key_active() noexcept {
        return kKeyActiveHigh ? GPIO_PIN_SET : GPIO_PIN_RESET;
    }

    bool key_pressed(GPIO_TypeDef* port, std::uint16_t pin) noexcept {
        return HAL_GPIO_ReadPin(port, pin) == key_active();
    }

    const char* bringup_mode_name(BringupMode mode) noexcept {
        switch (mode) {
        case BringupMode::sd: return "sd";
        case BringupMode::decode: return "decode";
        case BringupMode::i2s: return "i2s";
        case BringupMode::full: return "full";
        default: return "unknown";
        }
    }

    BringupMode select_bringup_mode() noexcept {
        if (!kBringupKeySelect) return kDefaultBringupMode;
        const bool key0 = key_pressed(GPIOA, GPIO_PIN_8);
        const bool wkup2 = key_pressed(GPIOA, GPIO_PIN_2);
        if (key0 && wkup2) return BringupMode::i2s;
        if (key0) return BringupMode::sd;
        if (wkup2) return BringupMode::decode;
        return kDefaultBringupMode;
    }

    void wait_key_press() noexcept {
        if (!kBringupWaitKey) return;
        const auto active = key_active();
        while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) == active) {
            HAL_Delay(10);
        }
        while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) != active) {
            HAL_Delay(10);
        }
        HAL_Delay(20);
    }
}

extern "C" void USART1_IRQHandler(void) {
    if (g_uart_adapter) {
        g_uart_adapter->on_irq();
    }
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    if (huart == &huart1) {
        g_uart1_dma_tx.on_complete();
    }
}

int main() {
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    // MX_SDMMC1_SD_Init();
    MX_I2S1_Init();
    MX_SPI5_Init();

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

    g_dma_ctx.rx_channel = ch;
    g_dma_ctx.tx = &g_uart1_dma_tx;
    g_dma_console.ctx = &g_dma_ctx;
    g_dma_console.ops = kDmaConsoleOps;

    io::EndpointDesc uart_desc{
        "io.uart1",
        io::cap_id("io.uart1"),
        io::EndpointKind::channel,
        io::EndpointCaps::duplex
    };
    io::EndpointDesc console_desc{
        "io.console0",
        io::cap_id("io.console0"),
        io::EndpointKind::channel,
        io::EndpointCaps::duplex
    };
    auto& reg = bringup.registry();
    auto* uart_ep = reg.find_channel("io.uart1");
    auto r_uart = uart_ep
        ? reg.replace_channel(uart_desc, g_dma_console, &bringup.reactor())
        : reg.register_channel(uart_desc, g_dma_console, &bringup.reactor());
    if (!r_uart) {
        Error_Handler();
    }
    auto* console_ep = reg.find_channel("io.console0");
    auto r_console = console_ep
        ? reg.replace_channel(console_desc, g_dma_console, &bringup.reactor())
        : reg.register_channel(console_desc, g_dma_console, &bringup.reactor());
    if (!r_console) {
        Error_Handler();
    }
    static out::channel_sink console_sink = out::make_channel_sink(g_dma_console);
    fs_set_console_sink(console_sink);
    audio_set_console_sink(console_sink);
    display_set_console_sink(console_sink);
    ink_set_console_sink(console_sink);

    const char msg[] = "bringup ok\n";
    auto wr = g_dma_console.write(io::ByteView{
        reinterpret_cast<const util::u8*>(msg),
        sizeof(msg) - 1
    });
    if (!wr && wr.error() != util::Errc::would_block) {
        Error_Handler();
    }

    const auto mode = select_bringup_mode();
    (void)out::try_println<"bringup: mode {}">(console_sink, bringup_mode_name(mode));

    if (mode == BringupMode::sd) {
        (void)out::try_println<"bringup: sd selftest begin">(console_sink);
        (void)fs_sd_selftest(kSdSelfStartLba, kSdSelfBlocks, kSdSelfStride);
    } else if (mode == BringupMode::decode) {
        if (!fs_boot_init()) {
            (void)out::try_println<"fs init failed">(console_sink);
            Error_Handler();
        }
        (void)out::try_println<"bringup: wait key">(console_sink);
        wait_key_press();
        (void)out::try_println<"bringup: decode selftest begin">(console_sink);
        (void)audio_mp3_decode_selftest();
    } else if (mode == BringupMode::i2s) {
        (void)out::try_println<"bringup: wait key">(console_sink);
        wait_key_press();
        (void)out::try_println<"bringup: i2s selftest begin">(console_sink);
        audio_i2s_selftest(kI2sSelfMs);
    } else {
        if (!fs_boot_init()) {
            (void)out::try_println<"fs init failed">(console_sink);
            Error_Handler();
        }
        if (display_st7305_init()) {
            display_st7305_selftest();
            if (ink_demo_render_once()) {
                (void)out::try_println<"display: ink demo done">(console_sink);
            } else {
                (void)out::try_println<"display: ink demo failed">(console_sink);
            }
            ink_demo_run();
        } else {
            (void)out::try_println<"display: init failed">(console_sink);
        }
        (void)out::try_println<"boot: wait key">(console_sink);
        wait_key_press();
        audio_mp3_demo_run();
    }

    while (true) {
        (void)running.run_once();
    }
}
