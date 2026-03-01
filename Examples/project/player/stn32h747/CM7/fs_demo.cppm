module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "stm32h7xx_hal.h"
#include <span>

#include "stm32h7xx_hal.h"
#include "stm32h7xx_ll_sdmmc.h"
#include "sdmmc.h"

export module player.stm32h7.fs_demo;

import util.core;
import fs_block;
import fs_core;
import fs_errno;
import fs_fatfs;
import fs_stream;
import fs_vfs;
import out.api;

namespace {
    constexpr util::u32 kTimeoutMs = 1000;
    constexpr bool kSdmmcVerbose = true;
    constexpr util::u32 kSdmmcInitClockDiv = 480; // ~400kHz when SDMMC clock is 192MHz
    constexpr bool kSdmmcTry4Bit = false;

    void sdmmc_diag_after_fail() noexcept {
        SDMMC_InitTypeDef init = {};
        init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
        init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
        init.BusWide = SDMMC_BUS_WIDE_1B;
        init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
        init.ClockDiv = kSdmmcInitClockDiv;
        (void)SDMMC_Init(hsd2.Instance, init);
        (void)SDMMC_PowerState_ON(hsd2.Instance);
        HAL_Delay(50);

        const auto e0 = SDMMC_CmdGoIdleState(hsd2.Instance);
        const auto e8 = SDMMC_CmdOperCond(hsd2.Instance);
        const auto r8 = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
        const auto e55 = SDMMC_CmdAppCommand(hsd2.Instance, 0);
        const auto r55 = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
        const auto e41 = SDMMC_CmdAppOperCommand(
            hsd2.Instance,
            SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY);
        const auto r41 = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
        const auto e41s = SDMMC_CmdAppOperCommand(
            hsd2.Instance,
            SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY | SD_SWITCH_1_8V_CAPACITY);
        const auto r41s = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);

        util::u32 e41_loop = e41;
        util::u32 r41_loop = r41;
        util::u32 e41_nov = 0;
        util::u32 r41_nov = 0;
        util::u32 e41_nohcs = 0;
        util::u32 r41_nohcs = 0;
        util::u32 e41_raw = 0;
        util::u32 r41_raw = 0;
        constexpr util::u32 kOcrRaw = 0x00FF8000u;
        for (int i = 0; i < 1000 && ((r41_loop & 0x80000000u) == 0u); ++i) {
            e41_loop = SDMMC_CmdAppCommand(hsd2.Instance, 0);
            r41_loop = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
            e41_loop = SDMMC_CmdAppOperCommand(
                hsd2.Instance,
                SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY);
            r41_loop = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
            HAL_Delay(1);
        }
        if ((r41_loop & 0x80000000u) == 0u) {
            for (int i = 0; i < 1000 && ((r41_nov & 0x80000000u) == 0u); ++i) {
                e41_nov = SDMMC_CmdAppCommand(hsd2.Instance, 0);
                r41_nov = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
                e41_nov = SDMMC_CmdAppOperCommand(
                    hsd2.Instance,
                    SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY);
                r41_nov = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
                HAL_Delay(1);
            }
        }
        if ((r41_loop & 0x80000000u) == 0u && (r41_nov & 0x80000000u) == 0u) {
            for (int i = 0; i < 1000 && ((r41_nohcs & 0x80000000u) == 0u); ++i) {
                e41_nohcs = SDMMC_CmdAppCommand(hsd2.Instance, 0);
                r41_nohcs = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
                e41_nohcs = SDMMC_CmdAppOperCommand(
                    hsd2.Instance,
                    SDMMC_VOLTAGE_WINDOW_SD);
                r41_nohcs = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
                HAL_Delay(1);
            }
        }
        if ((r41_loop & 0x80000000u) == 0u && (r41_nov & 0x80000000u) == 0u &&
            (r41_nohcs & 0x80000000u) == 0u) {
            for (int i = 0; i < 1000 && ((r41_raw & 0x80000000u) == 0u); ++i) {
                e41_raw = SDMMC_CmdAppCommand(hsd2.Instance, 0);
                r41_raw = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
                e41_raw = SDMMC_CmdAppOperCommand(hsd2.Instance, kOcrRaw);
                r41_raw = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
                HAL_Delay(1);
            }
        }

        const auto e2 = SDMMC_CmdSendCID(hsd2.Instance);
        const auto r2_1 = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
        const auto r2_2 = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP2);
        const auto r2_3 = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP3);
        const auto r2_4 = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP4);

        uint16_t rca = 0;
        const auto e3 = SDMMC_CmdSetRelAdd(hsd2.Instance, &rca);
        const auto r3 = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
        const util::u32 rca_arg = static_cast<util::u32>(rca) << 16;

        const auto e9 = SDMMC_CmdSendCSD(hsd2.Instance, rca_arg);
        const auto r9_1 = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
        const auto r9_2 = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP2);
        const auto r9_3 = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP3);
        const auto r9_4 = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP4);

        const auto e7 = SDMMC_CmdSelDesel(hsd2.Instance, rca_arg);
        const auto r7 = SDMMC_GetResponse(hsd2.Instance, SDMMC_RESP1);
        hsd2.Instance->ICR = 0xFFFFFFFFu;

        out::println<"fs sdmmc: diag e0=0x{:08X} e8=0x{:08X} r8=0x{:08X}">(
            static_cast<util::u32>(e0),
            static_cast<util::u32>(e8),
            static_cast<util::u32>(r8));
        out::println<"fs sdmmc: diag e55=0x{:08X} r55=0x{:08X}">(
            static_cast<util::u32>(e55),
            static_cast<util::u32>(r55));
        out::println<"fs sdmmc: diag e41=0x{:08X} r41=0x{:08X} e41s=0x{:08X} r41s=0x{:08X}">(
            static_cast<util::u32>(e41),
            static_cast<util::u32>(r41),
            static_cast<util::u32>(e41s),
            static_cast<util::u32>(r41s));
        out::println<"fs sdmmc: diag e41l=0x{:08X} r41l=0x{:08X}">(
            static_cast<util::u32>(e41_loop),
            static_cast<util::u32>(r41_loop));
        out::println<"fs sdmmc: diag e41n=0x{:08X} r41n=0x{:08X}">(
            static_cast<util::u32>(e41_nov),
            static_cast<util::u32>(r41_nov));
        out::println<"fs sdmmc: diag e41h=0x{:08X} r41h=0x{:08X}">(
            static_cast<util::u32>(e41_nohcs),
            static_cast<util::u32>(r41_nohcs));
        out::println<"fs sdmmc: diag e41r=0x{:08X} r41r=0x{:08X}">(
            static_cast<util::u32>(e41_raw),
            static_cast<util::u32>(r41_raw));
        out::println<"fs sdmmc: diag e2=0x{:08X} r2={:08X} {:08X} {:08X} {:08X}">(
            static_cast<util::u32>(e2),
            static_cast<util::u32>(r2_1),
            static_cast<util::u32>(r2_2),
            static_cast<util::u32>(r2_3),
            static_cast<util::u32>(r2_4));
        out::println<"fs sdmmc: diag e3=0x{:08X} r3=0x{:08X} rca=0x{:04X}">(
            static_cast<util::u32>(e3),
            static_cast<util::u32>(r3),
            static_cast<util::u32>(rca));
        out::println<"fs sdmmc: diag e9=0x{:08X} r9={:08X} {:08X} {:08X} {:08X}">(
            static_cast<util::u32>(e9),
            static_cast<util::u32>(r9_1),
            static_cast<util::u32>(r9_2),
            static_cast<util::u32>(r9_3),
            static_cast<util::u32>(r9_4));
        out::println<"fs sdmmc: diag e7=0x{:08X} r7=0x{:08X}">(
            static_cast<util::u32>(e7),
            static_cast<util::u32>(r7));
    }

    struct SdBlockDevice {
        bool init() noexcept {
            if constexpr (kSdmmcVerbose) {
                out::println<"fs sdmmc: init begin">();
            }

            MX_SDMMC2_SD_Init();
            hsd2.Init.ClockDiv = kSdmmcInitClockDiv;
            hsd2.Init.BusWide = SDMMC_BUS_WIDE_1B;
            if constexpr (kSdmmcVerbose) {
                GPIO_InitTypeDef gpio_init = {};
                gpio_init.Pin = GPIO_PIN_0;
                gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
                gpio_init.Pull = GPIO_NOPULL;
                gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
                HAL_GPIO_Init(GPIOA, &gpio_init);
                for (int i = 0; i < 10; ++i) {
                    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_0);
                    HAL_Delay(100);
                }
                gpio_init.Mode = GPIO_MODE_AF_PP;
                gpio_init.Pull = GPIO_PULLUP;
                gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
                gpio_init.Alternate = GPIO_AF9_SDIO2;
                HAL_GPIO_Init(GPIOA, &gpio_init);
            }
            hsd2.State = HAL_SD_STATE_RESET;
            const auto init_status = HAL_SD_Init(&hsd2);
            if constexpr (kSdmmcVerbose) {
                const auto sdmmc_src = static_cast<util::u32>(__HAL_RCC_GET_SDMMC_SOURCE());
                const auto ahb2enr = static_cast<util::u32>(RCC->AHB2ENR);
                const auto ahb3enr = static_cast<util::u32>(RCC->AHB3ENR);
                const auto sdmmc2_en = (__HAL_RCC_SDMMC2_IS_CLK_ENABLED() != 0u) ? 1 : 0;
                out::println<"fs sdmmc: rcc sdmmc_src=0x{:08X} ahb2enr=0x{:08X} ahb3enr=0x{:08X} sdmmc2_en={}">(
                    sdmmc_src,
                    ahb2enr,
                    ahb3enr,
                    sdmmc2_en);
                out::println<"fs sdmmc: gpioa moder=0x{:08X} pupd=0x{:08X} afr0=0x{:08X} afr1=0x{:08X}">(
                    static_cast<util::u32>(GPIOA->MODER),
                    static_cast<util::u32>(GPIOA->PUPDR),
                    static_cast<util::u32>(GPIOA->AFR[0]),
                    static_cast<util::u32>(GPIOA->AFR[1]));
                out::println<"fs sdmmc: gpiob moder=0x{:08X} pupd=0x{:08X} afr0=0x{:08X} afr1=0x{:08X}">(
                    static_cast<util::u32>(GPIOB->MODER),
                    static_cast<util::u32>(GPIOB->PUPDR),
                    static_cast<util::u32>(GPIOB->AFR[0]),
                    static_cast<util::u32>(GPIOB->AFR[1]));
                out::println<"fs sdmmc: gpiod moder=0x{:08X} pupd=0x{:08X} afr0=0x{:08X} afr1=0x{:08X}">(
                    static_cast<util::u32>(GPIOD->MODER),
                    static_cast<util::u32>(GPIOD->PUPDR),
                    static_cast<util::u32>(GPIOD->AFR[0]),
                    static_cast<util::u32>(GPIOD->AFR[1]));
                out::println<"fs sdmmc: gpiog moder=0x{:08X} pupd=0x{:08X} afr0=0x{:08X} afr1=0x{:08X}">(
                    static_cast<util::u32>(GPIOG->MODER),
                    static_cast<util::u32>(GPIOG->PUPDR),
                    static_cast<util::u32>(GPIOG->AFR[0]),
                    static_cast<util::u32>(GPIOG->AFR[1]));
                const auto cmd = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
                const auto d0 = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14);
                const auto d1 = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15);
                const auto d2 = HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_11);
                const auto d3 = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4);
                out::println<"fs sdmmc: pins cmd={} d0={} d1={} d2={} d3={}">(
                    static_cast<int>(cmd),
                    static_cast<int>(d0),
                    static_cast<int>(d1),
                    static_cast<int>(d2),
                    static_cast<int>(d3));
            }
            if (init_status != HAL_OK) {
                if constexpr (kSdmmcVerbose) {
                    const auto err = static_cast<util::u32>(HAL_SD_GetError(&hsd2));
                    const auto clk = static_cast<util::u32>(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC));
                    const auto clkcr = static_cast<util::u32>(hsd2.Instance->CLKCR);
                    const auto sta = static_cast<util::u32>(hsd2.Instance->STA);
                    const auto power = static_cast<util::u32>(hsd2.Instance->POWER);
                    const auto cmd = static_cast<util::u32>(hsd2.Instance->CMD);
                    const auto arg = static_cast<util::u32>(hsd2.Instance->ARG);
                    const auto resp1 = static_cast<util::u32>(hsd2.Instance->RESP1);
                    out::println<"fs sdmmc: HAL_SD_Init failed err=0x{:08X}">(
                        err);
                    out::println<"fs sdmmc: ker_ck={}Hz clkcr=0x{:08X} sta=0x{:08X}">(
                        clk, clkcr, sta);
                    out::println<"fs sdmmc: power=0x{:08X} cmd=0x{:08X} arg=0x{:08X} resp1=0x{:08X}">(
                        power, cmd, arg, resp1);
                    sdmmc_diag_after_fail();
                }
                return false;
            }

            if constexpr (kSdmmcVerbose) {
                const auto clk = static_cast<util::u32>(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC));
                const auto clkcr = static_cast<util::u32>(hsd2.Instance->CLKCR);
                out::println<"fs sdmmc: ker_ck={}Hz clkcr=0x{:08X}">(
                    clk, clkcr);
            }

            if constexpr (kSdmmcTry4Bit) {
                if (HAL_SD_ConfigWideBusOperation(&hsd2, SDMMC_BUS_WIDE_4B) != HAL_OK) {
                    if constexpr (kSdmmcVerbose) {
                        out::println<"fs sdmmc: 4b failed, try 1b">();
                    }
                    if (HAL_SD_ConfigWideBusOperation(&hsd2, SDMMC_BUS_WIDE_1B) != HAL_OK) {
                        if constexpr (kSdmmcVerbose) {
                            out::println<"fs sdmmc: 1b failed">();
                        }
                        return false;
                    }
                }
            } else {
                if (HAL_SD_ConfigWideBusOperation(&hsd2, SDMMC_BUS_WIDE_1B) != HAL_OK) {
                    if constexpr (kSdmmcVerbose) {
                        out::println<"fs sdmmc: 1b failed">();
                    }
                    return false;
                }
            }

            if (HAL_SD_GetCardInfo(&hsd2, &info_) != HAL_OK) {
                if constexpr (kSdmmcVerbose) {
                    out::println<"fs sdmmc: card info failed">();
                }
                return false;
            }
            block_size_ = info_.LogBlockSize;
            block_count_ = info_.LogBlockNbr;
            device_.ctx = this;
            device_.read = &SdBlockDevice::read_impl;
            device_.write = &SdBlockDevice::write_impl;
            device_.erase = &SdBlockDevice::erase_impl;
            device_.flush = &SdBlockDevice::flush_impl;
            device_.block_size = block_size_;
            device_.block_count = block_count_;
            return true;
        }

        [[nodiscard]] fs::BlockDevice& device() noexcept { return device_; }

    private:
        static bool can_dma(const void* data, std::size_t size) noexcept {
            return ((reinterpret_cast<std::uintptr_t>(data) & 0x3u) == 0u) && ((size & 0x3u) == 0u);
        }

        static fs::Status read_impl(void* ctx, util::u64 lba, std::span<util::u8> data) noexcept {
            auto* self = static_cast<SdBlockDevice*>(ctx);
            if (!self || data.empty() || (data.size() % self->block_size_) != 0) {
                return fs::Status{fs::Err::inval};
            }
            const util::u32 count = static_cast<util::u32>(data.size() / self->block_size_);
            const bool use_dma = can_dma(data.data(), data.size());
            if (use_dma) {
                if (HAL_SD_ReadBlocks_DMA(&hsd2, data.data(), static_cast<uint32_t>(lba), count) != HAL_OK) {
                    return fs::Status{fs::Err::io};
                }
            } else {
                if (HAL_SD_ReadBlocks(&hsd2, data.data(), static_cast<uint32_t>(lba), count, kTimeoutMs) != HAL_OK) {
                    return fs::Status{fs::Err::io};
                }
            }
            const util::u32 start = HAL_GetTick();
            while (HAL_SD_GetCardState(&hsd2) != HAL_SD_CARD_TRANSFER) {
                if ((HAL_GetTick() - start) > kTimeoutMs) return fs::Status{fs::Err::timeout};
            }
            return fs::Status{fs::Err::ok};
        }

        static fs::Status write_impl(void* ctx, util::u64 lba, std::span<const util::u8> data) noexcept {
            auto* self = static_cast<SdBlockDevice*>(ctx);
            if (!self || data.empty() || (data.size() % self->block_size_) != 0) {
                return fs::Status{fs::Err::inval};
            }
            const util::u32 count = static_cast<util::u32>(data.size() / self->block_size_);
            const bool use_dma = can_dma(data.data(), data.size());
            if (use_dma) {
                if (HAL_SD_WriteBlocks_DMA(&hsd2, const_cast<uint8_t*>(data.data()),
                        static_cast<uint32_t>(lba), count) != HAL_OK) {
                    return fs::Status{fs::Err::io};
                }
            } else {
                if (HAL_SD_WriteBlocks(&hsd2, const_cast<uint8_t*>(data.data()),
                        static_cast<uint32_t>(lba), count, kTimeoutMs) != HAL_OK) {
                    return fs::Status{fs::Err::io};
                }
            }
            const util::u32 start = HAL_GetTick();
            while (HAL_SD_GetCardState(&hsd2) != HAL_SD_CARD_TRANSFER) {
                if ((HAL_GetTick() - start) > kTimeoutMs) return fs::Status{fs::Err::timeout};
            }
            return fs::Status{fs::Err::ok};
        }

        static fs::Status erase_impl(void*, util::u64, util::u64) noexcept {
            return fs::Status{fs::Err::nosys};
        }

        static fs::Status flush_impl(void*) noexcept {
            return fs::Status{fs::Err::ok};
        }

        fs::BlockDevice device_{};
        HAL_SD_CardInfoTypeDef info_{};
        util::u32 block_size_{512};
        util::u32 block_count_{0};
    };
} // namespace

export bool fs_boot_init() noexcept {
    static SdBlockDevice sd{};
    if (!sd.init()) {
        out::println<"fs boot: sd init failed">();
        return false;
    }
    static fs::FatFsMount mount{};
    static std::array<util::u8, 4096> cache{};
    auto st = mount.mount(sd.device(), std::span<util::u8>{cache}, false, 0);
    if (!st) {
        out::println<"fs boot: mount failed {}">(static_cast<int>(st.err));
        return false;
    }
    fs::set_mount(mount.mount_point());
    out::println<"fs boot: mount ok">();
    return true;
}

export void fs_demo_run() noexcept {
    (void)0;
}
