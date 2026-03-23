#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <span>
#include <string_view>
#include <utility>

#include "stm32h7xx_hal.h"
#include "fmc.h"
#include "tim.h"
#include "usb_device.h"
#include "stm32h7xx_hal_pcd.h"

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
import charm.system.run_loop;
import charm.system.time;
import charm.system.reactor_pump;
import charm.system.init_usb;
import init.node;
import charm.port;
import driver.usart_channel;
import io.channel;
import io.reactor;
import io.registry;
import block.registry;
import fs_core;
import fs_stream;
import fs_vfs;
import fs_errno;
import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.scheduler;
import out.api;
import out.channel;
import player.stm32h7.audio_mp3_demo;
import player.stm32h7.board_console;
import player.stm32h7.board_keys;
import player.stm32h7.board_config;
import player.stm32h7.board_sdram;
import player.stm32h7.board_sdmmc;
import player.stm32h7.board_usb;
import player.stm32h7.display_st7305;
import player.stm32h7.fs_demo;
import player.stm32h7.ink_demo;
import player.stm32h7.app_config;
import player.stm32h7.usb_system;
import platform.board.stn32h747xi;
import usb.class_msc_block;
import usb.class_msc_block.node;
import usb.common;
import usb.device_driver;
import usb.dsl;
import util.core;
import util.error;

extern "C" {
    void MPU_Config(void);
    void SystemClock_Config(void);
    void MX_GPIO_Init(void);
    void MX_FMC_Init(void);
    void MX_I2S1_Init(void);
    void MX_SDMMC2_SD_Init(void);
    void MX_DMA_Init(void);
    void MX_USB_OTG_FS_PCD_Init(void);
    void MX_SPI5_Init(void);
    void MX_TIM8_Init(void);
    void MX_USART1_UART_Init(void);
    void Error_Handler(void);
    extern UART_HandleTypeDef huart1;
    extern TIM_HandleTypeDef htim8;
    extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
}

void early_uart_print(const char* s) noexcept;

namespace {
    void early_uart_print_sv(std::string_view s) noexcept {
        char buf[64]{};
        std::size_t pos = 0;
        for (std::size_t i = 0; i < s.size(); ++i) {
            buf[pos++] = s[i];
            if (pos + 1 >= sizeof(buf)) {
                buf[pos] = '\0';
                early_uart_print(buf);
                pos = 0;
            }
        }
        if (pos > 0) {
            buf[pos] = '\0';
            early_uart_print(buf);
        }
    }

    struct RootDumpCtx {
        const char* prefix{nullptr};
        std::size_t count{0};
    };

    fs::Status dump_entry_early(void* p, const fs::MountOps::ListEntry& entry) noexcept {
        auto* out = static_cast<RootDumpCtx*>(p);
        if (!out || !out->prefix) return fs::Status{fs::Errc::inval};
        early_uart_print("fs: ");
        early_uart_print(out->prefix);
        early_uart_print(" ");
        early_uart_print_sv(entry.name);
        early_uart_print(" type=");
        char buf[12]{};
        std::snprintf(buf, sizeof(buf), "%d\n", static_cast<int>(entry.type));
        early_uart_print(buf);
        ++out->count;
        return fs::Status{fs::Errc::ok};
    }

    void dump_dir_early(const char* path) noexcept {
        if (!path || *path == '\0') return;
        RootDumpCtx ctx{path, 0};
        const auto st = fs::vfs_list(path, &ctx, dump_entry_early);
        if (!st) {
            char buf[64]{};
            std::snprintf(buf, sizeof(buf), "fs: list failed %s err=%d\n",
                path, static_cast<int>(st.err));
            early_uart_print(buf);
            return;
        }
        char count_buf[64]{};
        std::snprintf(count_buf, sizeof(count_buf), "fs: %s entries=%u\n",
            path, static_cast<unsigned>(ctx.count));
        early_uart_print(count_buf);
    }
} // namespace

// namespace {
    constexpr util::usize kRxCap = player::stm32h7::app::config::kRxCap;
    constexpr util::usize kTxCap = player::stm32h7::app::config::kTxCap;
    constexpr bool kBringupKeySelect = player::stm32h7::app::config::kBringupKeySelect;
    constexpr bool kBringupWaitKey = player::stm32h7::app::config::kBringupWaitKey;
    constexpr bool kFmcInitOnBoot = player::stm32h7::app::config::kFmcInitOnBoot;
    constexpr bool kSdramSelftestOnBoot = player::stm32h7::app::config::kSdramSelftestOnBoot;
    constexpr bool kSdramSelftestInBringup = player::stm32h7::app::config::kSdramSelftestInBringup;
    constexpr bool kEnableSdmmcInit = player::stm32h7::app::config::kEnableSdmmcInit;
    constexpr bool kEnableUsbMsc = player::stm32h7::app::config::kEnableUsbMsc;
    constexpr bool kUseStUsbStack = player::stm32h7::app::config::kUseStUsbStack;
    constexpr bool kEnableAudio = player::stm32h7::app::config::kEnableAudio;
    constexpr bool kEnableDisplay = player::stm32h7::app::config::kEnableDisplay;
    constexpr bool kDebugStopAfterBringup = player::stm32h7::app::config::kDebugStopAfterBringup;
    constexpr bool kDebugStopAfterChannel = player::stm32h7::app::config::kDebugStopAfterChannel;
    constexpr bool kDebugStopAfterFs = player::stm32h7::app::config::kDebugStopAfterFs;
    constexpr bool kDebugDumpRoot = player::stm32h7::app::config::kDebugDumpRoot;
    constexpr bool kUseOutLoggerEarly = player::stm32h7::app::config::kUseOutLoggerEarly;
    constexpr bool kUseDmaConsole = player::stm32h7::app::config::kUseDmaConsole;
    constexpr bool kEncoderTestOnBoot = player::stm32h7::app::config::kEncoderTestOnBoot;
    constexpr util::u32 kEncoderTestMs = player::stm32h7::app::config::kEncoderTestMs;
    driver::usart::ChannelAdapter<kRxCap, kTxCap>* g_uart_adapter = nullptr;

    constexpr usb::u16 kLangs[] = { 0x0409 };
    constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
    constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
    constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm MSC");
    constexpr auto kSerialStr = usb::make_ascii_string_descriptor("0001");

    static const usb::StringTable<4> kUsbStrings{
        std::array<std::span<const usb::u8>, 4>{
            std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
            std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
            std::span<const usb::u8>(kProductStr.data(), kProductStr.size()),
            std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
        }
    };

#if defined(__GNUC__)
#define CHARM_DMA_BUFFER __attribute__((section(".dma_buffer"), aligned(32)))
#define CHARM_WEAK __attribute__((weak))
#else
#define CHARM_DMA_BUFFER
#define CHARM_WEAK
#endif

    inline void clean_dcache(const void* addr, std::size_t size) noexcept {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
        if (!addr || size == 0) return;
        if ((SCB->CCR & SCB_CCR_DC_Msk) == 0U) return;
        std::uintptr_t start = reinterpret_cast<std::uintptr_t>(addr);
        std::uintptr_t end = start + size;
        start &= ~static_cast<std::uintptr_t>(31);
        end = (end + 31u) & ~static_cast<std::uintptr_t>(31);
        SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t*>(start),
            static_cast<int32_t>(end - start));
#else
        (void)addr;
        (void)size;
#endif
    }

    inline void allow_unaligned_access() noexcept {
#if defined(SCB_CCR_UNALIGN_TRP_Msk)
        SCB->CCR &= ~SCB_CCR_UNALIGN_TRP_Msk;
#endif
    }

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
            const util::u32 now = static_cast<util::u32>(charm::port::now_ms(nullptr));
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
            clean_dcache(&buf[tail], len);
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
            dma_start_ms = static_cast<util::u32>(charm::port::now_ms(nullptr));
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

    static CHARM_DMA_BUFFER DmaUartTx g_uart1_dma_tx{};
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

    void wait_key_press() noexcept;

    void early_uart_print(const char* msg) noexcept {
        player::stm32h7::board::early_uart_print(msg);
    }

    void early_sleep_ms(util::u32 ms) noexcept {
        HAL_Delay(ms);
    }


    void early_uart_print_err(const char* tag, util::Errc err) noexcept {
        char buf[64]{};
        const int n = std::snprintf(buf, sizeof(buf), "%s err=%d\n", tag, static_cast<int>(err));
        if (n > 0) {
            early_uart_print(buf);
        }
    }

    void run_display_demo(out::channel_sink& console_sink) noexcept {
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
    }

    void run_audio_demo(out::channel_sink& console_sink) noexcept {
        (void)out::try_println<"boot: wait key">(console_sink);
        wait_key_press();
        audio_mp3_demo_run();
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
        const bool key0 = player::stm32h7::board::boot_key_pressed(
            player::stm32h7::board::kBootKey0);
        const bool wkup2 = player::stm32h7::board::boot_key_pressed(
            player::stm32h7::board::kBootKey1);
        if (key0 && wkup2) return BringupMode::i2s;
        if (key0) return BringupMode::sd;
        if (wkup2) return BringupMode::decode;
        return kDefaultBringupMode;
    }

    void wait_key_press() noexcept {
        if (!kBringupWaitKey) return;
        player::stm32h7::board::wait_for_boot_key();
    }

    void encoder_test() noexcept {
        early_uart_print("encoder: init begin\n");
        MX_TIM8_Init();
        __HAL_RCC_GPIOI_CLK_ENABLE();
        GPIO_InitTypeDef gpio_init = {};
        gpio_init.Pin = player::stm32h7::board::kEncoderKey.pin;
        gpio_init.Mode = GPIO_MODE_INPUT;
        gpio_init.Pull = GPIO_PULLUP;
        gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(player::stm32h7::board::kEncoderKey.port, &gpio_init);
        if (HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL) != HAL_OK) {
            early_uart_print("encoder: start failed\n");
            return;
        }
        early_uart_print("encoder: start ok\n");

        const util::u32 start_ms = static_cast<util::u32>(charm::port::now_ms(nullptr));
        std::uint16_t last = static_cast<std::uint16_t>(__HAL_TIM_GET_COUNTER(&htim8));
        util::u32 last_print = start_ms;
        GPIO_PinState last_key = HAL_GPIO_ReadPin(
            player::stm32h7::board::kEncoderKey.port,
            player::stm32h7::board::kEncoderKey.pin);
        while (HAL_GPIO_ReadPin(
                   player::stm32h7::board::kBootKey0.port,
                   player::stm32h7::board::kBootKey0.pin) == GPIO_PIN_RESET) {
            const std::uint16_t now = static_cast<std::uint16_t>(__HAL_TIM_GET_COUNTER(&htim8));
            const GPIO_PinState key = HAL_GPIO_ReadPin(
                player::stm32h7::board::kEncoderKey.port,
                player::stm32h7::board::kEncoderKey.pin);
            const util::u32 tick = static_cast<util::u32>(charm::port::now_ms(nullptr));
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


extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    if (huart == &huart1) {
        g_uart1_dma_tx.on_complete();
    }
}

int main() {
    HAL_Init();
    // MPU_Config();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();


    auto kit = charm::port::init();
    allow_unaligned_access();
    player::stm32h7::board::set_console_sink(kit.console);
    out::Scope scope{kit.console};

    early_uart_print("boot: uart ok\n");
    {
        char buf[64]{};
        const int n = std::snprintf(buf, sizeof(buf), "boot: ccr=0x%08lX\n",
            static_cast<unsigned long>(SCB->CCR));
        if (n > 0) {
            (void)HAL_UART_Transmit(&huart1,
                reinterpret_cast<uint8_t*>(buf),
                static_cast<uint16_t>(n),
                100);
        }
    }
    early_uart_print("boot: fmc init begin\n");
    if (kFmcInitOnBoot) {
        MX_FMC_Init();
        early_uart_print("boot: fmc init ok\n");
    } else {
        early_uart_print("boot: fmc init skip\n");
    }
    bool sdram_ready = false;
    if (kSdramSelftestOnBoot) {
        early_uart_print("boot: sdram test begin\n");
        sdram_ready = player::stm32h7::board::sdram_selftest_early(
            &early_uart_print,
            &early_sleep_ms);
        early_uart_print("boot: sdram test end\n");
    }
    audio_mp3_set_sdram_ready(sdram_ready);
    if (kEnableSdmmcInit) {
        player::stm32h7::board::sdmmc_hw_init();
    }
    MX_I2S1_Init();
    MX_SPI5_Init();
    if (!kUseStUsbStack) {
        player::stm32h7::board::usb_hw_init();
    }

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
        caps.can0,
        caps.sdmmc0,
        caps.flash0,
        pump,
        post_fn,
        &running,
        pump_id,
        8
    };

    early_uart_print("boot: pre-bringup\n");
    std::span<const init::Node* const> extra_nodes{};
    charm::system::UsbMscBlockInitChain<block::Registry<16>> usb_chain{
        bringup.block_registry(),
        usb::device::MscBlockDesc{},
        init::Phase::app,
        static_cast<util::u32>(init::Runlevel::all)
    };
    if (kEnableUsbMsc && !kUseStUsbStack) {
        auto& dcd_ops = player::stm32h7::board::usb_dcd_ops();
        usb_chain.binding.desc.cap_name = "usb.msc0";
        usb_chain.binding.desc.block_cap = "block.sd0";
        usb_chain.binding.desc.dcd = dcd_ops;
        usb_chain.binding.desc.dcd_ctx = &hpcd_USB_OTG_FS;
        usb_chain.binding.desc.adapter = &player::stm32h7::board::usb_adapter();
        usb_chain.binding.desc.dev_info.vendor_id = 0x1209;
        usb_chain.binding.desc.dev_info.product_id = 0x0002;
        usb_chain.binding.desc.dev_info.i_manufacturer = 1;
        usb_chain.binding.desc.dev_info.i_product = 2;
        usb_chain.binding.desc.dev_info.i_serial = 3;
        usb_chain.binding.desc.msc_cfg.ep_out = 0x01;
        usb_chain.binding.desc.msc_cfg.ep_in = 0x81;
        usb_chain.binding.desc.msc_cfg.ep_mps = 64;
        usb_chain.binding.desc.strings = std::span<const std::span<const usb::u8>>(
            kUsbStrings.entries.data(), kUsbStrings.entries.size());
        usb_chain.binding.desc.storage_cfg.read_only = true;
        usb_chain.binding.desc.on_ready = &player::stm32h7::board::usb_set_ready;
        usb_chain.binding.desc.on_ready_ctx = nullptr;
        extra_nodes = usb_chain.node_span();
    }

    auto r = bringup.start(
        static_cast<util::u32>(init::Runlevel::all),
        init::Phase::app,
        extra_nodes
        );
    if (!r) {
        early_uart_print("boot: bringup failed\n");
        early_uart_print_err("boot: bringup", r.error());
        Error_Handler();
    }
    early_uart_print("boot: bringup ok\n");
    if (kEnableUsbMsc) {
        player::stm32h7::board::usb_enable_hooks(!kUseStUsbStack);
        if (kUseStUsbStack) {
            auto* usb_dev = fs_sd_block_device();
            if (!usb_dev) {
                early_uart_print("boot: usb block device not ready\n");
            }
            usb_system_init(usb_dev, false);
            early_uart_print("boot: usb device init ok\n");
            if (HAL_PCD_Start(&hpcd_USB_OTG_FS) == HAL_OK) {
                early_uart_print("boot: usb pcd start ok\n");
            } else {
                early_uart_print("boot: usb pcd start failed\n");
            }
        } else {
            auto& dcd_ops = player::stm32h7::board::usb_dcd_ops();
            (void)dcd_ops.connect(&hpcd_USB_OTG_FS, true);
            early_uart_print("boot: usb pcd start ok\n");
        }
    }
    charm::system::time::bind(bringup.clock());
    if (kDebugStopAfterBringup) {
        while (true) {}
    }

    auto* ch = bringup.registry().open_channel("io.uart1");
    if (!ch) {
        Error_Handler();
    }
    g_uart_adapter = static_cast<driver::usart::ChannelAdapter<kRxCap, kTxCap>*>(ch->ctx);
    player::stm32h7::board::set_uart_irq_handler(
        {g_uart_adapter, [](void* ctx) noexcept {
            auto* adapter = static_cast<driver::usart::ChannelAdapter<kRxCap, kTxCap>*>(ctx);
            if (adapter) adapter->on_irq();
        }}
    );

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
    early_uart_print("boot: channels ok\n");
    if (kDebugStopAfterChannel) {
        while (true) {}
    }
    static out::channel_sink console_sink = out::make_channel_sink(g_dma_console);
    fs_set_console_sink(console_sink);
    audio_set_console_sink(console_sink);
    display_set_console_sink(console_sink);
    display_st7305_set_dma(!kEnableAudio);
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
        if (kDebugDumpRoot) {
            dump_dir_early("/");
            dump_dir_early("/SDNAND~1");
        }
        if (kDebugStopAfterFs) {
            while (true) {}
        }
        if (kEnableAudio) {
            if (kUseOutLoggerEarly) {
                (void)out::try_println<"bringup: wait key">(console_sink);
            }
            wait_key_press();
            if (kUseOutLoggerEarly) {
                (void)out::try_println<"bringup: decode selftest begin">(console_sink);
            }
            (void)audio_mp3_decode_selftest();
        } else {
            early_uart_print("boot: audio disabled\n");
        }
    } else if (mode == BringupMode::i2s) {
        if (kUseOutLoggerEarly) {
            (void)out::try_println<"bringup: wait key">(console_sink);
        }
        wait_key_press();
        if (kEnableAudio) {
            if (kUseOutLoggerEarly) {
                (void)out::try_println<"bringup: i2s selftest begin">(console_sink);
            }
            audio_i2s_selftest(kI2sSelfMs);
        } else {
            early_uart_print("boot: audio disabled\n");
        }
    } else {
        if (kSdramSelftestOnBoot && kSdramSelftestInBringup) {
            if (kUseOutLoggerEarly) {
                (void)out::try_println<"bringup: sdram selftest begin">(console_sink);
            }
            (void)player::stm32h7::board::sdram_selftest(console_sink);
        } else if (kSdramSelftestOnBoot && !kSdramSelftestInBringup) {
            early_uart_print("boot: sdram selftest skip\n");
        }
        early_uart_print("boot: fs init begin\n");
        if (!fs_boot_init()) {
            (void)out::try_println<"fs init failed">(console_sink);
            Error_Handler();
        }
        early_uart_print("boot: fs init ok\n");
        if (kDebugDumpRoot) {
            dump_dir_early("/");
            dump_dir_early("/SDNAND~1");
        }
        if (kDebugStopAfterFs) {
            while (true) {}
        }
        if (kEnableDisplay) {
            run_display_demo(console_sink);
        } else {
            early_uart_print("boot: display disabled\n");
        }
        if (kEnableAudio) {
            run_audio_demo(console_sink);
        } else {
            early_uart_print("boot: audio disabled\n");
        }
    }

    charm::system::RunLoop<4> loop{};
    loop.bind_clock(bringup.clock());
    charm::system::SchedulerLoopStep<SchedulerT> sched_step{};
    charm::system::ReactorLoopStep<io::Reactor> reactor_step{};
    (void)charm::system::add_reactor_step(loop,
                                          bringup.reactor(),
                                          reactor_step,
                                          charm::system::LoopPhase::io,
                                          8,
                                          "io_reactor");
    (void)charm::system::add_scheduler_step(loop,
                                            running,
                                            sched_step,
                                            charm::system::LoopPhase::update,
                                            0,
                                            "sched");

    while (true) {
        loop.run_once();
        if (kEnableUsbMsc && !kUseStUsbStack) {
            player::stm32h7::board::usb_poll_msc(&hpcd_USB_OTG_FS);
        }
    }
}
