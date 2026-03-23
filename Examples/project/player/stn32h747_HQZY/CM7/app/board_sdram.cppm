module;

#include <cstddef>
#include <cstdint>

#include "stm32h7xx_hal.h"
#include "fmc.h"

export module player.stm32h7.board_sdram;

import out.api;
import out.channel;
import player.stm32h7.board_config;
import util.core;

extern "C" SDRAM_HandleTypeDef hsdram1;

export namespace player::stm32h7::board {
    using EarlyLogFn = void(*)(const char*) noexcept;
    using EarlySleepFn = void(*)(util::u32) noexcept;

    bool sdram_init_sequence(EarlyLogFn log, EarlySleepFn sleep) noexcept {
        if (log) log("sdram: seq begin\n");
        FMC_SDRAM_CommandTypeDef cmd{};
        cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
        cmd.AutoRefreshNumber = 1;
        cmd.ModeRegisterDefinition = 0;

        cmd.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
        if (log) log("sdram: seq clk\n");
        if (HAL_SDRAM_SendCommand(&hsdram1, &cmd, 100) != HAL_OK) return false;
        if (sleep) sleep(1);

        cmd.CommandMode = FMC_SDRAM_CMD_PALL;
        if (log) log("sdram: seq pall\n");
        if (HAL_SDRAM_SendCommand(&hsdram1, &cmd, 100) != HAL_OK) return false;

        cmd.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
        cmd.AutoRefreshNumber = 8;
        if (log) log("sdram: seq refresh\n");
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
        if (log) log("sdram: seq mode\n");
        if (HAL_SDRAM_SendCommand(&hsdram1, &cmd, 100) != HAL_OK) return false;

        if (log) log("sdram: seq rate\n");
        if (HAL_SDRAM_ProgramRefreshRate(&hsdram1, kSdram.refresh_rate) != HAL_OK) return false;
        if (log) log("sdram: seq ok\n");
        return true;
    }

    bool sdram_selftest_early(EarlyLogFn log, EarlySleepFn sleep) noexcept {
        constexpr std::uint32_t pattern = kSdram.test_pattern;
        constexpr std::size_t words = 16;
        if (!sdram_init_sequence(log, sleep)) {
            if (log) log("sdram: init sequence failed\n");
            return false;
        }
        auto* sdram = reinterpret_cast<volatile std::uint32_t*>(kSdram.base);
        if (log) log("sdram: test write0\n");
        sdram[0] = pattern;
        if (log) log("sdram: test read0\n");
        const auto probe = sdram[0];
        if (probe != pattern) {
            if (log) log("sdram: probe mismatch\n");
            return false;
        }
        if (log) log("sdram: test writeN\n");
        for (std::size_t i = 0; i < words; ++i) {
            sdram[i] = pattern + static_cast<std::uint32_t>(i);
        }
        if (log) log("sdram: test readN\n");
        for (std::size_t i = 0; i < words; ++i) {
            const std::uint32_t expect = pattern + static_cast<std::uint32_t>(i);
            if (sdram[i] != expect) {
                if (log) log("sdram: mismatch\n");
                return false;
            }
        }
        if (log) log("sdram: ok\n");
        return true;
    }

    bool sdram_selftest(out::channel_sink& sink) noexcept {
        if (!sdram_init_sequence(nullptr, nullptr)) {
            (void)out::try_println<"sdram: init sequence failed">(sink);
            return false;
        }
        auto* sdram = reinterpret_cast<std::uint32_t*>(kSdram.base);
        for (std::size_t i = 0; i < kSdram.test_words; ++i) {
            sdram[i] = kSdram.test_pattern + static_cast<std::uint32_t>(i);
        }
        const std::size_t bytes = kSdram.test_words * sizeof(std::uint32_t);
        SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t*>(kSdram.base), bytes);
        SCB_InvalidateDCache_by_Addr(reinterpret_cast<uint32_t*>(kSdram.base), bytes);
        for (std::size_t i = 0; i < kSdram.test_words; ++i) {
            const std::uint32_t expect = kSdram.test_pattern + static_cast<std::uint32_t>(i);
            if (sdram[i] != expect) {
                (void)out::try_println<"sdram: mismatch at {} exp=0x{:08X} got=0x{:08X}">(
                    sink, static_cast<unsigned long>(i), expect, sdram[i]);
                return false;
            }
        }
        (void)out::try_println<"sdram: ok base=0x{:08X} words={}">(
            sink, static_cast<std::uint32_t>(kSdram.base),
            static_cast<unsigned long>(kSdram.test_words));
        return true;
    }
}
