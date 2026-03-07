#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <utility>

#include "stm32h7xx_hal.h"
#include "fmc.h"
#include "tim.h"

/*
LED
    PA3   ------> LED Green Enable:low
    PB1   ------> LED Blue  Enable:low

KEY (hardware pulled up)
    PA2   ------> PWR_WKUP2 Enable:high
    PA8   ------> KEY0      Enable:high

Encoder
    PI8     ------> KEY
    PC7     ------> TIM8_CH2
    PC6     ------> TIM8_CH1

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
    void MX_FMC_Init(void);
    void MX_I2S1_Init(void);
    void MX_SDMMC1_SD_Init(void);
    void MX_SPI5_Init(void);
    void MX_TIM8_Init(void);
    void MX_USART1_UART_Init(void);
    void Error_Handler(void);
    extern UART_HandleTypeDef huart1;
    extern TIM_HandleTypeDef htim8;
}

namespace {
    constexpr util::usize kRxCap = 64;
    constexpr util::usize kTxCap = 640;
    constexpr bool kKeyActiveHigh = true;
    constexpr bool kBringupKeySelect = true;
    constexpr bool kBringupWaitKey = true;
    constexpr bool kFmcInitOnBoot = true;
    constexpr bool kSdramSelftestOnBoot = true;
    constexpr bool kSdramSelftestInBringup = false;
    constexpr bool kUseOutLoggerEarly = false;
    constexpr bool kUseDmaConsole = false;
    constexpr bool kEncoderTestOnBoot = true;
    constexpr util::u32 kEncoderTestMs = 5000;
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

    void early_uart_print(const char* msg) noexcept {
        if (!msg) return;
        const std::size_t len = std::strlen(msg);
        if (len == 0) return;
        (void)HAL_UART_Transmit(&huart1,
            reinterpret_cast<uint8_t*>(const_cast<char*>(msg)),
            static_cast<uint16_t>(len),
            100);
    }

    void early_uart_print_err(const char* tag, util::Errc err) noexcept {
        char buf[64]{};
        const int n = std::snprintf(buf, sizeof(buf), "%s err=%d\n", tag, static_cast<int>(err));
        if (n > 0) {
            early_uart_print(buf);
        }
    }

    bool sdram_init_sequence() noexcept;

    bool sdram_selftest_early() noexcept {
        constexpr std::uintptr_t base = 0xD0000000u;
        constexpr std::size_t words = 16;
        constexpr std::uint32_t pattern = 0xA5A50000u;
        if (!sdram_init_sequence()) {
            early_uart_print("sdram: init sequence failed\n");
            return false;
        }
        auto* sdram = reinterpret_cast<volatile std::uint32_t*>(base);
        early_uart_print("sdram: test write0\n");
        sdram[0] = pattern;
        early_uart_print("sdram: test read0\n");
        const auto probe = sdram[0];
        if (probe != pattern) {
            early_uart_print("sdram: probe mismatch\n");
            return false;
        }
        early_uart_print("sdram: test writeN\n");
        for (std::size_t i = 0; i < words; ++i) {
            sdram[i] = pattern + static_cast<std::uint32_t>(i);
        }
        early_uart_print("sdram: test readN\n");
        for (std::size_t i = 0; i < words; ++i) {
            const std::uint32_t expect = pattern + static_cast<std::uint32_t>(i);
            if (sdram[i] != expect) {
                early_uart_print("sdram: mismatch\n");
                return false;
            }
        }
        early_uart_print("sdram: ok\n");
        return true;
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

    void encoder_test() noexcept {
        early_uart_print("encoder: init begin\n");
        MX_TIM8_Init();
        __HAL_RCC_GPIOI_CLK_ENABLE();
        GPIO_InitTypeDef gpio_init = {};
        gpio_init.Pin = GPIO_PIN_8;
        gpio_init.Mode = GPIO_MODE_INPUT;
        gpio_init.Pull = GPIO_PULLUP;
        gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOI, &gpio_init);
        if (HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL) != HAL_OK) {
            early_uart_print("encoder: start failed\n");
            return;
        }
        early_uart_print("encoder: start ok\n");

        const util::u32 start_ms = HAL_GetTick();
        std::uint16_t last = static_cast<std::uint16_t>(__HAL_TIM_GET_COUNTER(&htim8));
        util::u32 last_print = start_ms;
        GPIO_PinState last_key = HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_8);
        while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) == GPIO_PIN_RESET) {
            const std::uint16_t now = static_cast<std::uint16_t>(__HAL_TIM_GET_COUNTER(&htim8));
            const GPIO_PinState key = HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_8);
            const util::u32 tick = HAL_GetTick();
            if (now != last || key != last_key || (tick - last_print) >= 200u) {
                char buf[80]{};
                const int n = std::snprintf(
                    buf, sizeof(buf),
                    "encoder: cnt=%u delta=%d key=%u\n",
                    static_cast<unsigned>(now),
                    static_cast<int>(static_cast<std::int16_t>(now - last)),
                    static_cast<unsigned>(key == GPIO_PIN_SET ? 1 : 0)
                );
                if (n > 0) early_uart_print(buf);
                last = now;
                last_key = key;
                last_print = tick;
            }
        }
        early_uart_print("encoder: test end\n");
    }

    constexpr std::uintptr_t kSdramBase = 0xD0000000u;
    constexpr std::size_t kSdramTestWords = 1024;
    constexpr std::uint32_t kSdramPattern = 0xA5A50000u;
    constexpr std::uint32_t kSdramRefresh = 0x0603u;

    bool sdram_init_sequence() noexcept {
        early_uart_print("sdram: seq begin\n");
        FMC_SDRAM_CommandTypeDef cmd{};
        cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
        cmd.AutoRefreshNumber = 1;
        cmd.ModeRegisterDefinition = 0;

        cmd.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
        early_uart_print("sdram: seq clk\n");
        if (HAL_SDRAM_SendCommand(&hsdram1, &cmd, 100) != HAL_OK) return false;
        HAL_Delay(1);

        cmd.CommandMode = FMC_SDRAM_CMD_PALL;
        early_uart_print("sdram: seq pall\n");
        if (HAL_SDRAM_SendCommand(&hsdram1, &cmd, 100) != HAL_OK) return false;

        cmd.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
        cmd.AutoRefreshNumber = 8;
        early_uart_print("sdram: seq refresh\n");
        if (HAL_SDRAM_SendCommand(&hsdram1, &cmd, 100) != HAL_OK) return false;

        constexpr std::uint32_t kMode =
            0x0000u | // burst length 1
            0x0000u | // burst type sequential
            0x0030u | // CAS latency 3
            0x0000u | // standard
            0x0200u;  // single write burst

        cmd.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
        cmd.AutoRefreshNumber = 1;
        cmd.ModeRegisterDefinition = kMode;
        early_uart_print("sdram: seq mode\n");
        if (HAL_SDRAM_SendCommand(&hsdram1, &cmd, 100) != HAL_OK) return false;

        early_uart_print("sdram: seq rate\n");
        if (HAL_SDRAM_ProgramRefreshRate(&hsdram1, kSdramRefresh) != HAL_OK) return false;
        early_uart_print("sdram: seq ok\n");
        return true;
    }

    bool sdram_selftest(out::channel_sink& sink) noexcept {
        if (!sdram_init_sequence()) {
            (void)out::try_println<"sdram: init sequence failed">(sink);
            return false;
        }
        auto* sdram = reinterpret_cast<std::uint32_t*>(kSdramBase);
        for (std::size_t i = 0; i < kSdramTestWords; ++i) {
            sdram[i] = kSdramPattern + static_cast<std::uint32_t>(i);
        }
        const std::size_t bytes = kSdramTestWords * sizeof(std::uint32_t);
        SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t*>(kSdramBase), bytes);
        SCB_InvalidateDCache_by_Addr(reinterpret_cast<uint32_t*>(kSdramBase), bytes);
        for (std::size_t i = 0; i < kSdramTestWords; ++i) {
            const std::uint32_t expect = kSdramPattern + static_cast<std::uint32_t>(i);
            if (sdram[i] != expect) {
                (void)out::try_println<"sdram: mismatch at {} exp=0x{:08X} got=0x{:08X}">(
                    sink, static_cast<unsigned long>(i), expect, sdram[i]);
                return false;
            }
        }
        (void)out::try_println<"sdram: ok base=0x{:08X} words={}">(
            sink, static_cast<std::uint32_t>(kSdramBase), static_cast<unsigned long>(kSdramTestWords));
        return true;
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
    const char early_msg[] = "boot: uart ok\n";
    (void)HAL_UART_Transmit(&huart1,
        reinterpret_cast<uint8_t*>(const_cast<char*>(early_msg)),
        static_cast<uint16_t>(sizeof(early_msg) - 1),
        100);
    early_uart_print("boot: fmc init begin\n");
    if (kFmcInitOnBoot) {
        MX_FMC_Init();
        early_uart_print("boot: fmc init ok\n");
    } else {
        early_uart_print("boot: fmc init skip\n");
    }
    if (kSdramSelftestOnBoot) {
        early_uart_print("boot: sdram test begin\n");
        (void)sdram_selftest_early();
        early_uart_print("boot: sdram test end\n");
    }
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
    charm::system::BringupMinimal<24, 32, 16, kRxCap, kTxCap> bringup{
        caps.uart1,
        caps.clock,
        caps.input,
        caps.spi1,
        caps.i2c1,
        pump,
        post_fn,
        &running,
        pump_id,
        8
    };

    early_uart_print("boot: pre-bringup\n");
    auto r = bringup.start();
    if (!r) {
        early_uart_print("boot: bringup failed\n");
        early_uart_print_err("boot: bringup", r.error());
        Error_Handler();
    }
    early_uart_print("boot: bringup ok\n");

    auto* ch = bringup.registry().open_channel("io.uart1");
    if (!ch) {
        Error_Handler();
    }
    g_uart_adapter = static_cast<driver::usart::ChannelAdapter<kRxCap, kTxCap>*>(ch->ctx);

    g_dma_ctx.rx_channel = ch;
    g_dma_ctx.tx = &g_uart1_dma_tx;
    g_dma_console.ctx = &g_dma_ctx;
    g_dma_console.ops = kDmaConsoleOps;
    early_uart_print("boot: console setup v2\n");

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
    early_uart_print("boot: uart channel ok\n");
    auto* console_ep = reg.find_channel("io.console0");
    auto r_console = console_ep
        ? reg.replace_channel(console_desc, g_dma_console, &bringup.reactor())
        : reg.register_channel(console_desc, g_dma_console, &bringup.reactor());
    if (!r_console) {
        Error_Handler();
    }
    early_uart_print("boot: console channel ok\n");
    static out::channel_sink console_sink = out::make_channel_sink(g_dma_console);
    fs_set_console_sink(console_sink);
    audio_set_console_sink(console_sink);
    display_set_console_sink(console_sink);
    ink_set_console_sink(console_sink);
    early_uart_print("boot: sinks ok\n");

    if (kUseDmaConsole) {
        early_uart_print("boot: dma write begin\n");
        const char msg[] = "bringup ok\n";
        auto wr = g_dma_console.write(io::ByteView{
            reinterpret_cast<const util::u8*>(msg),
            sizeof(msg) - 1
        });
        if (!wr && wr.error() != util::Errc::would_block) {
            Error_Handler();
        }
        early_uart_print("boot: dma write ok\n");
    } else {
        early_uart_print("boot: dma write skip\n");
    }

    early_uart_print("boot: mode select begin\n");
    const auto mode = select_bringup_mode();
    early_uart_print("boot: mode select ok\n");
    if (kEncoderTestOnBoot) {
        encoder_test();
    }
    if (kUseOutLoggerEarly) {
        (void)out::try_println<"bringup: mode {}">(console_sink, bringup_mode_name(mode));
    }

    if (mode == BringupMode::sd) {
        if (kUseOutLoggerEarly) {
            (void)out::try_println<"bringup: sd selftest begin">(console_sink);
        }
        (void)fs_sd_selftest(kSdSelfStartLba, kSdSelfBlocks, kSdSelfStride);
    } else if (mode == BringupMode::decode) {
        early_uart_print("boot: fs init begin\n");
        if (!fs_boot_init()) {
            if (kUseOutLoggerEarly) {
                (void)out::try_println<"fs init failed">(console_sink);
            }
            Error_Handler();
        }
        early_uart_print("boot: fs init ok\n");
        if (kUseOutLoggerEarly) {
            (void)out::try_println<"bringup: wait key">(console_sink);
        }
        wait_key_press();
        if (kUseOutLoggerEarly) {
            (void)out::try_println<"bringup: decode selftest begin">(console_sink);
        }
        (void)audio_mp3_decode_selftest();
    } else if (mode == BringupMode::i2s) {
        if (kUseOutLoggerEarly) {
            (void)out::try_println<"bringup: wait key">(console_sink);
        }
        wait_key_press();
        if (kUseOutLoggerEarly) {
            (void)out::try_println<"bringup: i2s selftest begin">(console_sink);
        }
        audio_i2s_selftest(kI2sSelfMs);
    } else {
        if (kSdramSelftestOnBoot && kSdramSelftestInBringup) {
            if (kUseOutLoggerEarly) {
                (void)out::try_println<"bringup: sdram selftest begin">(console_sink);
            }
            (void)sdram_selftest(console_sink);
        } else if (kSdramSelftestOnBoot && !kSdramSelftestInBringup) {
            early_uart_print("boot: sdram selftest skip\n");
        }
        early_uart_print("boot: fs init begin\n");
        if (!fs_boot_init()) {
            (void)out::try_println<"fs init failed">(console_sink);
            Error_Handler();
        }
        early_uart_print("boot: fs init ok\n");
        early_uart_print("boot: display init begin\n");
        if (display_st7305_init()) {
            early_uart_print("boot: display init ok\n");
            display_st7305_selftest();
            if (ink_demo_render_once()) {
                (void)out::try_println<"display: ink demo done">(console_sink);
            } else {
                (void)out::try_println<"display: ink demo failed">(console_sink);
            }
            ink_demo_run();
        } else {
            early_uart_print("boot: display init failed\n");
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
