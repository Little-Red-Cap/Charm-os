module;

#include <array>
#include <cstdio>
#include <cstdint>
#include <span>

#include "stm32h7xx_hal.h"
#include "fmc.h"
#include "i2s.h"
#include "spi.h"
#include "tim.h"

export module player.stm32h7.app_pre_bringup;

import init.graph;
import init.node;
import charm.port;
import player.stm32h7.board_keys;
import player.stm32h7.board_sdram;
import player.stm32h7.board_sdmmc;
import player.stm32h7.board_usb;
import util.core;
import util.error;

export namespace player::stm32h7::app::pre_bringup {
    struct Config {
        bool fmc_init_on_boot{true};
        bool sdram_selftest_on_boot{true};
        bool enable_sdmmc_init{false};
        bool use_st_usb_stack{true};
        bool enable_i2s_init{true};
        bool enable_spi5_init{true};
    };

    using PrintFn = void (*)(const char*) noexcept;
    using SleepFn = void (*)(util::u32) noexcept;

    struct Context {
        Config cfg{};
        PrintFn print{nullptr};
        SleepFn sleep{nullptr};
        bool sdram_ready{false};
    };

    inline void encoder_test(PrintFn print) noexcept {
        if (print) print("encoder: init begin\n");
        MX_TIM8_Init();
        __HAL_RCC_GPIOI_CLK_ENABLE();
        GPIO_InitTypeDef gpio_init = {};
        gpio_init.Pin = player::stm32h7::board::kEncoderKey.pin;
        gpio_init.Mode = GPIO_MODE_INPUT;
        gpio_init.Pull = GPIO_PULLUP;
        gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(player::stm32h7::board::kEncoderKey.port, &gpio_init);
        if (HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL) != HAL_OK) {
            if (print) print("encoder: start failed\n");
            return;
        }
        if (print) print("encoder: start ok\n");

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
                if (n > 0 && print) print(buf);
                last = now;
                last_key = key;
                last_print = tick;
            }
        }
        if (print) print("encoder: test end\n");
    }

    namespace detail {
        static constexpr init::CapId kCapFmc = init::cap_id("hw.fmc");
        static constexpr init::CapId kCapSdram = init::cap_id("hw.sdram");
        static constexpr init::CapId kCapSdmmc = init::cap_id("hw.sdmmc");
        static constexpr init::CapId kCapUsb = init::cap_id("hw.usb");
        static constexpr init::CapId kCapI2s = init::cap_id("hw.i2s1");
        static constexpr init::CapId kCapSpi5 = init::cap_id("hw.spi5");

        util::Result<void> init_fmc(void* ctx) noexcept {
            auto* c = static_cast<Context*>(ctx);
            if (!c) return util::unexpected(util::Errc::invalid_arg);
            if (!c->cfg.fmc_init_on_boot) {
                if (c->print) c->print("boot: fmc init skip\n");
                return {};
            }
            if (c->print) c->print("boot: fmc init begin\n");
            MX_FMC_Init();
            if (c->print) c->print("boot: fmc init ok\n");
            return {};
        }

        util::Result<void> init_sdram(void* ctx) noexcept {
            auto* c = static_cast<Context*>(ctx);
            if (!c) return util::unexpected(util::Errc::invalid_arg);
            if (!c->cfg.sdram_selftest_on_boot) return {};
            if (c->print) c->print("boot: sdram test begin\n");
            c->sdram_ready = player::stm32h7::board::sdram_selftest_early(
                c->print,
                c->sleep
            );
            if (c->print) c->print("boot: sdram test end\n");
            return {};
        }

        util::Result<void> init_sdmmc(void* ctx) noexcept {
            auto* c = static_cast<Context*>(ctx);
            if (!c) return util::unexpected(util::Errc::invalid_arg);
            if (!c->cfg.enable_sdmmc_init) return {};
            player::stm32h7::board::sdmmc_hw_init();
            return {};
        }

        util::Result<void> init_usb(void* ctx) noexcept {
            auto* c = static_cast<Context*>(ctx);
            if (!c) return util::unexpected(util::Errc::invalid_arg);
            player::stm32h7::board::usb_init_early(c->cfg.use_st_usb_stack);
            return {};
        }

        util::Result<void> init_i2s(void* ctx) noexcept {
            auto* c = static_cast<Context*>(ctx);
            if (!c) return util::unexpected(util::Errc::invalid_arg);
            if (!c->cfg.enable_i2s_init) return {};
            MX_I2S1_Init();
#if defined(RCC_PERIPHCLK_SPI123)
            const util::u32 i2s_clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI123);
#elif defined(RCC_PERIPHCLK_SPI1)
            const util::u32 i2s_clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI1);
#else
            const util::u32 i2s_clk = 0;
#endif
            if (c->print) {
                char buf[96]{};
                const int n = std::snprintf(
                    buf,
                    sizeof(buf),
                    "boot: i2s ker_clk=%luHz target_mclk=%luHz freq=%lu\n",
                    static_cast<unsigned long>(i2s_clk),
                    static_cast<unsigned long>(48000U * 256U),
                    static_cast<unsigned long>(hi2s1.Init.AudioFreq));
                if (n > 0) c->print(buf);
            }
            return {};
        }

        util::Result<void> init_spi5(void* ctx) noexcept {
            auto* c = static_cast<Context*>(ctx);
            if (!c) return util::unexpected(util::Errc::invalid_arg);
            if (!c->cfg.enable_spi5_init) return {};
            MX_SPI5_Init();
            return {};
        }
    } // namespace detail

    util::Result<void> run(Context& ctx) noexcept {
        static constexpr init::CapId kProvidesFmc[] = {detail::kCapFmc};
        static constexpr init::CapId kProvidesSdram[] = {detail::kCapSdram};
        static constexpr init::CapId kRequiresSdram[] = {detail::kCapFmc};
        static constexpr init::CapId kProvidesSdmmc[] = {detail::kCapSdmmc};
        static constexpr init::CapId kProvidesUsb[] = {detail::kCapUsb};
        static constexpr init::CapId kProvidesI2s[] = {detail::kCapI2s};
        static constexpr init::CapId kProvidesSpi5[] = {detail::kCapSpi5};

        const init::Node fmc_node{
            "fmc.init",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            std::span<const init::CapId>(kProvidesFmc, 1),
            {},
            &detail::init_fmc,
            nullptr,
            &ctx
        };
        const init::Node sdram_node{
            "sdram.selftest",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            std::span<const init::CapId>(kProvidesSdram, 1),
            std::span<const init::CapId>(kRequiresSdram, 1),
            &detail::init_sdram,
            nullptr,
            &ctx
        };
        const init::Node sdmmc_node{
            "sdmmc.init",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            std::span<const init::CapId>(kProvidesSdmmc, 1),
            {},
            &detail::init_sdmmc,
            nullptr,
            &ctx
        };
        const init::Node usb_node{
            "usb.init_early",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            std::span<const init::CapId>(kProvidesUsb, 1),
            {},
            &detail::init_usb,
            nullptr,
            &ctx
        };
        const init::Node i2s_node{
            "i2s1.init",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            std::span<const init::CapId>(kProvidesI2s, 1),
            {},
            &detail::init_i2s,
            nullptr,
            &ctx
        };
        const init::Node spi5_node{
            "spi5.init",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            std::span<const init::CapId>(kProvidesSpi5, 1),
            {},
            &detail::init_spi5,
            nullptr,
            &ctx
        };

        const init::Node* nodes[] = {
            &fmc_node,
            &sdram_node,
            &sdmmc_node,
            &usb_node,
            &i2s_node,
            &spi5_node
        };
        init::Graph<10, 20> graph{};
        auto r = graph.build(std::span<const init::Node* const>(nodes, 6),
                             static_cast<util::u32>(init::Runlevel::all),
                             init::Phase::early);
        if (!r) return r;
        return graph.start();
    }
}
