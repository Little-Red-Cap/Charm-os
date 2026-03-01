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
        HAL_Delay(2);

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
    }

    struct SdBlockDevice {
        bool init() noexcept {
            if constexpr (kSdmmcVerbose) {
                out::println<"fs sdmmc: init begin">();
            }

            if constexpr (kSdmmcVerbose) {
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

            MX_SDMMC2_SD_Init();
            hsd2.Init.ClockDiv = kSdmmcInitClockDiv;
            hsd2.Init.BusWide = SDMMC_BUS_WIDE_1B;
            if (HAL_SD_Init(&hsd2) != HAL_OK) {
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
