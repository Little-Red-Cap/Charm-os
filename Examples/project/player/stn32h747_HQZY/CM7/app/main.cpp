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
#include "i2s.h"
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
import init.plan;
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
import player.stm32h7.board_sdram;
import player.stm32h7.board_sdmmc;
import player.stm32h7.board_usb;
import player.stm32h7.display_st7305;
import player.stm32h7.fs_demo;
import player.stm32h7.ink_demo;
import player.stm32h7.app_config;
import player.stm32h7.app_boot_debug;
import player.stm32h7.app_boot_fs;
import player.runtime.hqzy_cm7.usb_storage_bridge;
import player.stm32h7.app_init_graph;
import player.stm32h7.app_pre_bringup;
import player.stm32h7.app_post_bringup;
import player.stm32h7.app_run_modes;
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
    void MX_SPI5_Init(void);
    void MX_USART1_UART_Init(void);
    void Error_Handler(void);
    extern UART_HandleTypeDef huart1;
    extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
}

using player::stm32h7::board::early_uart_print;

namespace {
    bool g_display_ready = false;

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
    constexpr bool kEnableUsbAudio = player::stm32h7::app::config::kEnableUsbAudio;
    constexpr bool kUseUsbAudioOnBoot = player::stm32h7::app::config::kUseUsbAudioOnBoot;
    constexpr bool kEnableDisplay = player::stm32h7::app::config::kEnableDisplay;
    // Debug flags now live in app_boot_debug.
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

    using BringupMode = player::stm32h7::app::run_modes::Mode;
    constexpr BringupMode kDefaultBringupMode = BringupMode::full;

    void wait_key_press() noexcept;

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



extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    if (huart == &huart1) {
        g_uart1_dma_tx.on_complete();
    }
}

int charm_player_selected_profile_main() {
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
    player::stm32h7::app::pre_bringup::Context pre_ctx{};
    pre_ctx.cfg.fmc_init_on_boot = kFmcInitOnBoot;
    pre_ctx.cfg.sdram_selftest_on_boot = kSdramSelftestOnBoot;
    pre_ctx.cfg.enable_sdmmc_init = kEnableSdmmcInit;
    pre_ctx.cfg.use_st_usb_stack = kUseStUsbStack;
    pre_ctx.cfg.enable_i2s_init = player::stm32h7::app::config::kEnableI2sInit;
    pre_ctx.cfg.enable_spi5_init = player::stm32h7::app::config::kEnableSpi5Init;
    pre_ctx.print = &early_uart_print;
    pre_ctx.sleep = &early_sleep_ms;
    if (auto pre = player::stm32h7::app::pre_bringup::run(pre_ctx); !pre) {
        early_uart_print("boot: pre-bringup failed\n");
        early_uart_print_err("boot: pre-bringup", pre.error());
        Error_Handler();
    }
    audio_mp3_set_sdram_ready(pre_ctx.sdram_ready);
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
    player::stm32h7::app::init_graph::UsbMscInitConfig usb_cfg{};
    usb_cfg.enable = kEnableUsbMsc;
    usb_cfg.use_st_stack = kUseStUsbStack;
    usb_cfg.strings = std::span<const std::span<const usb::u8>>(
        kUsbStrings.entries.data(), kUsbStrings.entries.size());
    auto usb_plan = player::stm32h7::app::init_graph::build_usb_msc_plan(
        bringup.block_registry(), usb_cfg);

    auto r = bringup.start_plan(
        init::legacy(usb_plan),
        static_cast<util::u32>(init::Runlevel::all),
        init::Phase::app);
    if (!r) {
        early_uart_print("boot: bringup failed\n");
        early_uart_print_err("boot: bringup", r.error());
        Error_Handler();
    }
    early_uart_print("boot: bringup ok\n");
    player::stm32h7::app::post_bringup::Context post_ctx{};
    post_ctx.cfg.enable_usb_msc = kEnableUsbMsc;
    post_ctx.cfg.use_st_usb_stack = kUseStUsbStack;
    post_ctx.cfg.enable_display = kEnableDisplay;
    post_ctx.print = &early_uart_print;
    if (auto post = player::stm32h7::app::post_bringup::run(post_ctx); !post) {
        early_uart_print("boot: post-bringup failed\n");
        early_uart_print_err("boot: post-bringup", post.error());
        Error_Handler();
    }
    g_display_ready = post_ctx.display_ready;
    charm::system::time::bind(bringup.clock());
    const auto debug_cfg = player::stm32h7::app::boot_debug::default_config();
    player::stm32h7::app::boot_debug::stop_if(debug_cfg.stop_after_bringup);

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
    player::stm32h7::app::boot_debug::stop_if(debug_cfg.stop_after_channel);
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
    const auto mode = player::stm32h7::app::run_modes::select_mode(
        kBringupKeySelect,
        player::stm32h7::board::boot_key_pressed(player::stm32h7::board::kBootKey0),
        player::stm32h7::board::boot_key_pressed(player::stm32h7::board::kBootKey1),
        kDefaultBringupMode);
    early_uart_print("boot: mode select ok\n");
    if (kEncoderTestOnBoot) {
        player::stm32h7::app::pre_bringup::encoder_test(early_uart_print);
    }
    player::stm32h7::app::boot_debug::log_mode(console_sink, mode, debug_cfg);

    player::stm32h7::app::run_modes::Context mode_ctx{
        .cfg = {
            .enable_audio = kEnableAudio,
            .enable_display = kEnableDisplay,
            .enable_usb_audio = kEnableUsbAudio,
            .use_usb_audio_on_boot = kUseUsbAudioOnBoot,
            .bringup_wait_key = kBringupWaitKey,
            .debug_dump_root = debug_cfg.dump_root,
            .debug_stop_after_fs = debug_cfg.stop_after_fs,
            .use_out_logger_early = debug_cfg.use_out_logger_early,
            .sdram_selftest_on_boot = kSdramSelftestOnBoot,
            .sdram_selftest_in_bringup = kSdramSelftestInBringup
        },
        .display_ready = g_display_ready
    };
    if (!player::stm32h7::app::run_modes::run(mode_ctx,
                                              mode,
                                              console_sink,
                                              early_uart_print)) {
        Error_Handler();
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
