module;

#include <cstdio>
#include <cstdint>

#include "stm32h7xx_hal.h"
#include "fmc.h"
#include "i2s.h"
#include "spi.h"
#include "tim.h"

export module player.stm32h7.app_pre_bringup;

import init.graph;
import init.materialize;
import init.meta;
import init.node;
import init.plan;
import init.recipe;
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
        using FmcCap = init::cap_c<"hw.fmc">;
        using SdramCap = init::cap_c<"hw.sdram">;
        using SdmmcCap = init::cap_c<"hw.sdmmc">;
        using UsbCap = init::cap_c<"hw.usb">;
        using I2sCap = init::cap_c<"hw.i2s1">;
        using Spi5Cap = init::cap_c<"hw.spi5">;

        util::Result<void> init_fmc(Context& ctx) noexcept {
            if (!ctx.cfg.fmc_init_on_boot) {
                if (ctx.print) ctx.print("boot: fmc init skip\n");
                return {};
            }
            if (ctx.print) ctx.print("boot: fmc init begin\n");
            MX_FMC_Init();
            if (ctx.print) ctx.print("boot: fmc init ok\n");
            return {};
        }

        util::Result<void> init_sdram(Context& ctx) noexcept {
            if (!ctx.cfg.sdram_selftest_on_boot) return {};
            if (ctx.print) ctx.print("boot: sdram test begin\n");
            ctx.sdram_ready = player::stm32h7::board::sdram_selftest_early(
                ctx.print,
                ctx.sleep
            );
            if (ctx.print) ctx.print("boot: sdram test end\n");
            return {};
        }

        util::Result<void> init_sdmmc(Context& ctx) noexcept {
            if (!ctx.cfg.enable_sdmmc_init) return {};
            player::stm32h7::board::sdmmc_hw_init();
            return {};
        }

        util::Result<void> init_usb(Context& ctx) noexcept {
            player::stm32h7::board::usb_init_early(ctx.cfg.use_st_usb_stack);
            return {};
        }

        util::Result<void> init_i2s(Context& ctx) noexcept {
            if (!ctx.cfg.enable_i2s_init) return {};
            MX_I2S1_Init();
#if defined(RCC_PERIPHCLK_SPI123)
            const util::u32 i2s_clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI123);
#elif defined(RCC_PERIPHCLK_SPI1)
            const util::u32 i2s_clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI1);
#else
            const util::u32 i2s_clk = 0;
#endif
            if (ctx.print) {
                char buf[96]{};
                const int n = std::snprintf(
                    buf,
                    sizeof(buf),
                    "boot: i2s ker_clk=%luHz target_mclk=%luHz freq=%lu\n",
                    static_cast<unsigned long>(i2s_clk),
                    static_cast<unsigned long>(48000U * 256U),
                    static_cast<unsigned long>(hi2s1.Init.AudioFreq));
                if (n > 0) ctx.print(buf);
            }
            return {};
        }

        util::Result<void> init_spi5(Context& ctx) noexcept {
            if (!ctx.cfg.enable_spi5_init) return {};
            MX_SPI5_Init();
            return {};
        }

        using FmcRecipe = init::recipe_desc<
            "fmc.init",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            init::cap_list<FmcCap>,
            init::cap_list<>,
            Context,
            &init_fmc>;

        using SdramRecipe = init::recipe_desc<
            "sdram.selftest",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            init::cap_list<SdramCap>,
            init::cap_list<FmcCap>,
            Context,
            &init_sdram>;

        using SdmmcRecipe = init::recipe_desc<
            "sdmmc.init",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            init::cap_list<SdmmcCap>,
            init::cap_list<>,
            Context,
            &init_sdmmc>;

        using UsbRecipe = init::recipe_desc<
            "usb.init_early",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            init::cap_list<UsbCap>,
            init::cap_list<>,
            Context,
            &init_usb>;

        using I2sRecipe = init::recipe_desc<
            "i2s1.init",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            init::cap_list<I2sCap>,
            init::cap_list<>,
            Context,
            &init_i2s>;

        using Spi5Recipe = init::recipe_desc<
            "spi5.init",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            init::cap_list<Spi5Cap>,
            init::cap_list<>,
            Context,
            &init_spi5>;
    } // namespace detail

    util::Result<void> run(Context& ctx) noexcept {
        const auto bringup_plan = init::compose(
            init::bind<detail::FmcRecipe>(ctx),
            init::bind<detail::SdramRecipe>(ctx),
            init::bind<detail::SdmmcRecipe>(ctx),
            init::bind<detail::UsbRecipe>(ctx),
            init::bind<detail::I2sRecipe>(ctx),
            init::bind<detail::Spi5Recipe>(ctx));
        auto materialized = init::materialize<10, 20>(bringup_plan);
        if (!materialized) {
            return util::unexpected(materialized.error());
        }
        init::Graph<10, 20> graph{};
        auto r = graph.build(materialized->node_ptr_span(),
                             materialized->build_runlevel_mask(),
                             materialized->build_max_phase());
        if (!r) return r;
        return graph.start();
    }
}
