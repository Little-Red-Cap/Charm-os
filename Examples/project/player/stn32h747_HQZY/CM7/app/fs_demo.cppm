module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

#include "stm32h7xx_hal.h"
#include <span>

#include "stm32h7xx_hal.h"
#include "stm32h7xx_ll_sdmmc.h"
#include "sdmmc.h"

export module player.stm32h7.fs_demo;

import util.core;
import boot_core;
import fs_block;
import fs_core;
import fs_errno;
import fs_fatfs;
import fs_stream;
import fs_vfs;
import out.api;
import out.channel;

namespace {
    static out::channel_sink* g_sink = nullptr;
    constexpr util::u32 kLogRetryMs = 20;
    template <out::fixed_string Fmt, typename... Args>
    inline void log(Args&&... args) noexcept {
        if (!g_sink) return;
        const util::u32 start = HAL_GetTick();
        while (true) {
            auto r = out::try_println<Fmt>(*g_sink, std::forward<Args>(args)...);
            if (r) break;
            if (r.error() != out::errc::would_block) break;
            if ((HAL_GetTick() - start) > kLogRetryMs) break;
            HAL_Delay(1);
        }
        const util::u32 flush_start = HAL_GetTick();
        while (true) {
            auto r = g_sink->flush();
            if (r) break;
            if (r.error() != out::errc::would_block) break;
            if ((HAL_GetTick() - flush_start) > kLogRetryMs) break;
            HAL_Delay(1);
        }
    }

    constexpr util::u32 kTimeoutMs = 1000;
    constexpr bool kSdmmcVerbose = true;
    constexpr bool kSdmmcVerboseGpio = true;
    constexpr util::u32 kSdmmcInitClockDiv = 480; // ~400kHz when SDMMC clock is 192MHz
    constexpr util::u32 kSdmmcXferClockDiv = 4; // 64MHz / (2 * 4) = 8MHz
    constexpr bool kSdmmcTry4Bit = false;
    constexpr bool kSdmmcAllowDma = false;
    constexpr bool kSdmmcProbeRead = true;

    void sdmmc_log_read_fail(util::u32 lba, util::u32 count, bool use_dma) noexcept {
        const auto err = static_cast<util::u32>(HAL_SD_GetError(&hsd1));
        const auto state = static_cast<util::u32>(HAL_SD_GetCardState(&hsd1));
        const auto sta = static_cast<util::u32>(hsd1.Instance->STA);
        const auto cmd = static_cast<util::u32>(hsd1.Instance->CMD);
        const auto arg = static_cast<util::u32>(hsd1.Instance->ARG);
        const auto resp1 = static_cast<util::u32>(hsd1.Instance->RESP1);
        log<"fs sdmmc: read fail lba={} cnt={} dma={} err=0x{:08X} state=0x{:08X} sta=0x{:08X} cmd=0x{:08X} arg=0x{:08X} resp1=0x{:08X}">(
            lba, count, static_cast<int>(use_dma), err, state, sta, cmd, arg, resp1);
    }

    void sdmmc_log_read_timeout(util::u32 lba, util::u32 count) noexcept {
        const auto state = static_cast<util::u32>(HAL_SD_GetCardState(&hsd1));
        const auto sta = static_cast<util::u32>(hsd1.Instance->STA);
        const auto cmd = static_cast<util::u32>(hsd1.Instance->CMD);
        const auto resp1 = static_cast<util::u32>(hsd1.Instance->RESP1);
        log<"fs sdmmc: read timeout lba={} cnt={} state=0x{:08X} sta=0x{:08X} cmd=0x{:08X} resp1=0x{:08X}">(
            lba, count, state, sta, cmd, resp1);
    }

    void sdmmc_probe_read(util::u32 lba) noexcept {
        alignas(4) std::array<util::u8, 512> buf{};
        if (HAL_SD_ReadBlocks(&hsd1, buf.data(), lba, 1, kTimeoutMs) != HAL_OK) {
            sdmmc_log_read_fail(lba, 1, false);
            return;
        }
        const util::u32 start = HAL_GetTick();
        while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {
            if ((HAL_GetTick() - start) > kTimeoutMs) {
                sdmmc_log_read_timeout(lba, 1);
                return;
            }
        }
        util::u32 non_ff = 0;
        for (const auto v : buf) {
            if (v != 0xFFu) ++non_ff;
        }
        log<"fs sdmmc: probe lba{} non_ff={} head {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}">(
            lba,
            non_ff,
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
            buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
        log<"fs sdmmc: probe lba{} tail {:02X} {:02X}">(
            lba, buf[510], buf[511]);
    }

    void sdmmc_diag_after_fail() noexcept {
        SDMMC_InitTypeDef init = {};
        init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
        init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
        init.BusWide = SDMMC_BUS_WIDE_1B;
        init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
        init.ClockDiv = kSdmmcInitClockDiv;
        (void)SDMMC_Init(hsd1.Instance, init);
        (void)SDMMC_PowerState_ON(hsd1.Instance);
        HAL_Delay(50);

        const auto e0 = SDMMC_CmdGoIdleState(hsd1.Instance);
        const auto e8 = SDMMC_CmdOperCond(hsd1.Instance);
        const auto r8 = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
        const auto e55 = SDMMC_CmdAppCommand(hsd1.Instance, 0);
        const auto r55 = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
        const auto e41 = SDMMC_CmdAppOperCommand(
            hsd1.Instance,
            SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY);
        const auto r41 = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
        const auto e41s = SDMMC_CmdAppOperCommand(
            hsd1.Instance,
            SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY | SD_SWITCH_1_8V_CAPACITY);
        const auto r41s = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);

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
            e41_loop = SDMMC_CmdAppCommand(hsd1.Instance, 0);
            r41_loop = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
            e41_loop = SDMMC_CmdAppOperCommand(
                hsd1.Instance,
                SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY);
            r41_loop = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
            HAL_Delay(1);
        }
        if ((r41_loop & 0x80000000u) == 0u) {
            for (int i = 0; i < 1000 && ((r41_nov & 0x80000000u) == 0u); ++i) {
                e41_nov = SDMMC_CmdAppCommand(hsd1.Instance, 0);
                r41_nov = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
                e41_nov = SDMMC_CmdAppOperCommand(
                    hsd1.Instance,
                    SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY);
                r41_nov = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
                HAL_Delay(1);
            }
        }
        if ((r41_loop & 0x80000000u) == 0u && (r41_nov & 0x80000000u) == 0u) {
            for (int i = 0; i < 1000 && ((r41_nohcs & 0x80000000u) == 0u); ++i) {
                e41_nohcs = SDMMC_CmdAppCommand(hsd1.Instance, 0);
                r41_nohcs = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
                e41_nohcs = SDMMC_CmdAppOperCommand(
                    hsd1.Instance,
                    SDMMC_VOLTAGE_WINDOW_SD);
                r41_nohcs = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
                HAL_Delay(1);
            }
        }
        if ((r41_loop & 0x80000000u) == 0u && (r41_nov & 0x80000000u) == 0u &&
            (r41_nohcs & 0x80000000u) == 0u) {
            for (int i = 0; i < 1000 && ((r41_raw & 0x80000000u) == 0u); ++i) {
                e41_raw = SDMMC_CmdAppCommand(hsd1.Instance, 0);
                r41_raw = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
                e41_raw = SDMMC_CmdAppOperCommand(hsd1.Instance, kOcrRaw);
                r41_raw = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
                HAL_Delay(1);
            }
        }

        const auto e2 = SDMMC_CmdSendCID(hsd1.Instance);
        const auto r2_1 = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
        const auto r2_2 = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP2);
        const auto r2_3 = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP3);
        const auto r2_4 = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP4);

        uint16_t rca = 0;
        const auto e3 = SDMMC_CmdSetRelAdd(hsd1.Instance, &rca);
        const auto r3 = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
        const util::u32 rca_arg = static_cast<util::u32>(rca) << 16;

        const auto e9 = SDMMC_CmdSendCSD(hsd1.Instance, rca_arg);
        const auto r9_1 = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
        const auto r9_2 = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP2);
        const auto r9_3 = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP3);
        const auto r9_4 = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP4);

        const auto e7 = SDMMC_CmdSelDesel(hsd1.Instance, rca_arg);
        const auto r7 = SDMMC_GetResponse(hsd1.Instance, SDMMC_RESP1);
        hsd1.Instance->ICR = 0xFFFFFFFFu;

        log<"fs sdmmc: diag e0=0x{:08X} e8=0x{:08X} r8=0x{:08X}">(
            static_cast<util::u32>(e0),
            static_cast<util::u32>(e8),
            static_cast<util::u32>(r8));
        log<"fs sdmmc: diag e55=0x{:08X} r55=0x{:08X}">(
            static_cast<util::u32>(e55),
            static_cast<util::u32>(r55));
        log<"fs sdmmc: diag e41=0x{:08X} r41=0x{:08X} e41s=0x{:08X} r41s=0x{:08X}">(
            static_cast<util::u32>(e41),
            static_cast<util::u32>(r41),
            static_cast<util::u32>(e41s),
            static_cast<util::u32>(r41s));
        log<"fs sdmmc: diag e41l=0x{:08X} r41l=0x{:08X}">(
            static_cast<util::u32>(e41_loop),
            static_cast<util::u32>(r41_loop));
        log<"fs sdmmc: diag e41n=0x{:08X} r41n=0x{:08X}">(
            static_cast<util::u32>(e41_nov),
            static_cast<util::u32>(r41_nov));
        log<"fs sdmmc: diag e41h=0x{:08X} r41h=0x{:08X}">(
            static_cast<util::u32>(e41_nohcs),
            static_cast<util::u32>(r41_nohcs));
        log<"fs sdmmc: diag e41r=0x{:08X} r41r=0x{:08X}">(
            static_cast<util::u32>(e41_raw),
            static_cast<util::u32>(r41_raw));
        log<"fs sdmmc: diag e2=0x{:08X} r2={:08X} {:08X} {:08X} {:08X}">(
            static_cast<util::u32>(e2),
            static_cast<util::u32>(r2_1),
            static_cast<util::u32>(r2_2),
            static_cast<util::u32>(r2_3),
            static_cast<util::u32>(r2_4));
        log<"fs sdmmc: diag e3=0x{:08X} r3=0x{:08X} rca=0x{:04X}">(
            static_cast<util::u32>(e3),
            static_cast<util::u32>(r3),
            static_cast<util::u32>(rca));
        log<"fs sdmmc: diag e9=0x{:08X} r9={:08X} {:08X} {:08X} {:08X}">(
            static_cast<util::u32>(e9),
            static_cast<util::u32>(r9_1),
            static_cast<util::u32>(r9_2),
            static_cast<util::u32>(r9_3),
            static_cast<util::u32>(r9_4));
        log<"fs sdmmc: diag e7=0x{:08X} r7=0x{:08X}">(
            static_cast<util::u32>(e7),
            static_cast<util::u32>(r7));
    }

    struct SdBlockDevice {
        bool init() noexcept {
            if constexpr (kSdmmcVerbose) {
                log<"fs sdmmc: init begin">();
            }

            MX_SDMMC1_SD_Init();
            hsd1.Init.ClockDiv = kSdmmcInitClockDiv;
            hsd1.Init.BusWide = SDMMC_BUS_WIDE_1B;
            hsd1.State = HAL_SD_STATE_RESET;
            const auto init_status = HAL_SD_Init(&hsd1);
            if constexpr (kSdmmcVerbose) {
                const auto sdmmc_src = static_cast<util::u32>(__HAL_RCC_GET_SDMMC_SOURCE());
                const auto ahb2enr = static_cast<util::u32>(RCC->AHB2ENR);
                const auto ahb3enr = static_cast<util::u32>(RCC->AHB3ENR);
                const auto SDMMC1_en = (__HAL_RCC_SDMMC1_IS_CLK_ENABLED() != 0u) ? 1 : 0;
                log<"fs sdmmc: rcc src=0x{:08X} SDMMC1_en={}">(
                    sdmmc_src,
                    SDMMC1_en);
                log<"fs sdmmc: rcc ahb2=0x{:08X} ahb3=0x{:08X}">(
                    ahb2enr,
                    ahb3enr);
                if constexpr (kSdmmcVerboseGpio) {
                    log<"fs sdmmc: gpioa moder=0x{:08X} pupd=0x{:08X} afr0=0x{:08X} afr1=0x{:08X}">(
                        static_cast<util::u32>(GPIOA->MODER),
                        static_cast<util::u32>(GPIOA->PUPDR),
                        static_cast<util::u32>(GPIOA->AFR[0]),
                        static_cast<util::u32>(GPIOA->AFR[1]));
                    log<"fs sdmmc: gpioc moder=0x{:08X} pupd=0x{:08X} afr0=0x{:08X} afr1=0x{:08X}">(
                        static_cast<util::u32>(GPIOC->MODER),
                        static_cast<util::u32>(GPIOC->PUPDR),
                        static_cast<util::u32>(GPIOC->AFR[0]),
                        static_cast<util::u32>(GPIOC->AFR[1]));
                    log<"fs sdmmc: gpiod moder=0x{:08X} pupd=0x{:08X} afr0=0x{:08X} afr1=0x{:08X}">(
                        static_cast<util::u32>(GPIOD->MODER),
                        static_cast<util::u32>(GPIOD->PUPDR),
                        static_cast<util::u32>(GPIOD->AFR[0]),
                        static_cast<util::u32>(GPIOD->AFR[1]));
                    log<"fs sdmmc: gpioe moder=0x{:08X} pupd=0x{:08X} afr0=0x{:08X} afr1=0x{:08X}">(
                        static_cast<util::u32>(GPIOE->MODER),
                        static_cast<util::u32>(GPIOE->PUPDR),
                        static_cast<util::u32>(GPIOE->AFR[0]),
                        static_cast<util::u32>(GPIOE->AFR[1]));
                }
                const auto cmd = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2);
                const auto d0 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_8);
                const auto d1 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9);
                const auto d2 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_10);
                const auto d3 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_11);
                const auto ck = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_12);
                log<"fs sdmmc: pins cmd={} d0={} d1={} d2={} d3={} ck={}">(
                    static_cast<int>(cmd),
                    static_cast<int>(d0),
                    static_cast<int>(d1),
                    static_cast<int>(d2),
                    static_cast<int>(d3),
                    static_cast<int>(ck));
            }
            if (init_status != HAL_OK) {
                if constexpr (kSdmmcVerbose) {
                    const auto err = static_cast<util::u32>(HAL_SD_GetError(&hsd1));
                    const auto clk = static_cast<util::u32>(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC));
                    const auto clkcr = static_cast<util::u32>(hsd1.Instance->CLKCR);
                    const auto sta = static_cast<util::u32>(hsd1.Instance->STA);
                    const auto power = static_cast<util::u32>(hsd1.Instance->POWER);
                    const auto cmd = static_cast<util::u32>(hsd1.Instance->CMD);
                    const auto arg = static_cast<util::u32>(hsd1.Instance->ARG);
                    const auto resp1 = static_cast<util::u32>(hsd1.Instance->RESP1);
                    log<"fs sdmmc: HAL_SD_Init failed err=0x{:08X}">(
                        err);
                    log<"fs sdmmc: ker_ck={}Hz clkcr=0x{:08X} sta=0x{:08X}">(
                        clk, clkcr, sta);
                    log<"fs sdmmc: power=0x{:08X} cmd=0x{:08X} arg=0x{:08X} resp1=0x{:08X}">(
                        power, cmd, arg, resp1);
                    sdmmc_diag_after_fail();
                }
                return false;
            }

            if constexpr (kSdmmcVerbose) {
                const auto clk = static_cast<util::u32>(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC));
                const auto clkcr = static_cast<util::u32>(hsd1.Instance->CLKCR);
                log<"fs sdmmc: ker_ck={}Hz clkcr=0x{:08X}">(
                    clk, clkcr);
            }

            if constexpr (kSdmmcTry4Bit) {
                if (HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_4B) != HAL_OK) {
                    if constexpr (kSdmmcVerbose) {
                        log<"fs sdmmc: 4b failed, try 1b">();
                    }
                    if (HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_1B) != HAL_OK) {
                        if constexpr (kSdmmcVerbose) {
                            log<"fs sdmmc: 1b failed">();
                        }
                        return false;
                    }
                }
            } else {
                if (HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_1B) != HAL_OK) {
                    if constexpr (kSdmmcVerbose) {
                        log<"fs sdmmc: 1b failed">();
                    }
                    return false;
                }
            }
            MODIFY_REG(hsd1.Instance->CLKCR, SDMMC_CLKCR_CLKDIV, kSdmmcXferClockDiv);
            hsd1.Init.ClockDiv = kSdmmcXferClockDiv;
            if constexpr (kSdmmcVerbose) {
                const auto clk = static_cast<util::u32>(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC));
                const auto clkcr = static_cast<util::u32>(hsd1.Instance->CLKCR);
                log<"fs sdmmc: xfer ker_ck={}Hz clkcr=0x{:08X}">(
                    clk, clkcr);
            }

            if (HAL_SD_GetCardInfo(&hsd1, &info_) != HAL_OK) {
                if constexpr (kSdmmcVerbose) {
                    log<"fs sdmmc: card info failed">();
                }
                return false;
            }
            block_size_ = info_.LogBlockSize;
            block_count_ = info_.LogBlockNbr;
            if constexpr (kSdmmcVerbose) {
                log<"fs sdmmc: block_size={} block_count={}">(
                    block_size_, block_count_);
                log<"fs sdmmc: dma enabled={}">(
                    kSdmmcAllowDma ? 1 : 0);
            }
            if constexpr (kSdmmcProbeRead) {
                sdmmc_probe_read(0);
                sdmmc_probe_read(2048);
            }
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
        [[nodiscard]] fs::Status read_blocks(util::u64 lba, std::span<util::u8> data) noexcept {
            return read_impl(this, lba, data);
        }

    private:
        static bool can_dma(const void* data, std::size_t size) noexcept {
            return ((reinterpret_cast<std::uintptr_t>(data) & 0x3u) == 0u) && ((size & 0x3u) == 0u);
        }

        static fs::Status read_impl(void* ctx, util::u64 lba, std::span<util::u8> data) noexcept {
            auto* self = static_cast<SdBlockDevice*>(ctx);
            if (!self || data.empty() || (data.size() % self->block_size_) != 0) {
                return fs::Status{fs::Errc::inval};
            }
            const util::u32 count = static_cast<util::u32>(data.size() / self->block_size_);
            for (util::u32 i = 0; i < count; ++i) {
                auto* dst = data.data() + (static_cast<std::size_t>(i) * self->block_size_);
                const util::u32 lba_i = static_cast<util::u32>(lba + i);
                const bool use_dma = kSdmmcAllowDma && can_dma(dst, self->block_size_);
                if (use_dma) {
                    if (HAL_SD_ReadBlocks_DMA(&hsd1, dst, lba_i, 1) != HAL_OK) {
                        sdmmc_log_read_fail(lba_i, 1, true);
                        return fs::Status{fs::Errc::io};
                    }
                } else {
                    if (HAL_SD_ReadBlocks(&hsd1, dst, lba_i, 1, kTimeoutMs) != HAL_OK) {
                        sdmmc_log_read_fail(lba_i, 1, false);
                        return fs::Status{fs::Errc::io};
                    }
                }
                const util::u32 start = HAL_GetTick();
                while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {
                    if ((HAL_GetTick() - start) > kTimeoutMs) {
                        sdmmc_log_read_timeout(lba_i, 1);
                        return fs::Status{fs::Errc::timeout};
                    }
                }
            }
            return fs::Status{fs::Errc::ok};
        }

        static fs::Status write_impl(void* ctx, util::u64 lba, std::span<const util::u8> data) noexcept {
            auto* self = static_cast<SdBlockDevice*>(ctx);
            if (!self || data.empty() || (data.size() % self->block_size_) != 0) {
                return fs::Status{fs::Errc::inval};
            }
            const util::u32 count = static_cast<util::u32>(data.size() / self->block_size_);
            const bool use_dma = kSdmmcAllowDma && can_dma(data.data(), data.size());
            if (use_dma) {
                if (HAL_SD_WriteBlocks_DMA(&hsd1, const_cast<uint8_t*>(data.data()),
                        static_cast<uint32_t>(lba), count) != HAL_OK) {
                    return fs::Status{fs::Errc::io};
                }
            } else {
                if (HAL_SD_WriteBlocks(&hsd1, const_cast<uint8_t*>(data.data()),
                        static_cast<uint32_t>(lba), count, kTimeoutMs) != HAL_OK) {
                    return fs::Status{fs::Errc::io};
                }
            }
            const util::u32 start = HAL_GetTick();
            while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {
                if ((HAL_GetTick() - start) > kTimeoutMs) return fs::Status{fs::Errc::timeout};
            }
            return fs::Status{fs::Errc::ok};
        }

        static fs::Status erase_impl(void*, util::u64, util::u64) noexcept {
            return fs::Status{fs::Errc::nosys};
        }

        static fs::Status flush_impl(void*) noexcept {
            return fs::Status{fs::Errc::ok};
        }

        fs::BlockDevice device_{};
        HAL_SD_CardInfoTypeDef info_{};
        util::u32 block_size_{512};
        util::u32 block_count_{0};
    };

    static SdBlockDevice g_sd{};
    static bool g_sd_inited = false;

    bool sd_init_once() noexcept {
        if (g_sd_inited) return true;
        g_sd_inited = g_sd.init();
        return g_sd_inited;
    }
} // namespace

export bool fs_boot_init() noexcept {
    if (!sd_init_once()) {
        log<"fs boot: sd init failed">();
        return false;
    }
    static fs::FatFsMount mount{};
    auto st = mount.mount(g_sd.device(), false);
    if (!st) {
        log<"fs boot: mount failed {}">(static_cast<int>(st.err));
        return false;
    }
    fs::clear_mounts();
    (void)fs::add_mount("/", mount.mount_point());
    log<"fs boot: mount ok">();
    return true;
}

export fs::BlockDevice* fs_sd_block_device() noexcept {
    if (!sd_init_once()) return nullptr;
    return &g_sd.device();
}

export bool fs_sd_selftest(util::u32 start_lba, util::u32 blocks, util::u32 stride) noexcept {
    if (!g_sd.init()) {
        log<"fs sdmmc: selftest init failed">();
        return false;
    }
    std::array<util::u8, 512> buf{};
    util::u32 crc = 0;
    util::u32 ok_blocks = 0;
    const util::u32 t0 = HAL_GetTick();
    for (util::u32 i = 0; i < blocks; ++i) {
        const util::u32 lba = start_lba + (i * stride);
        auto st = g_sd.read_blocks(lba, std::span<util::u8>(buf.data(), buf.size()));
        if (!st) {
            log<"fs sdmmc: selftest read fail lba={} err={}">(
                lba, static_cast<int>(st.err));
            break;
        }
        crc = boot::crc32_update(crc, buf.data(), buf.size());
        ++ok_blocks;
    }
    const util::u32 ms = HAL_GetTick() - t0;
    const util::u32 bytes = ok_blocks * static_cast<util::u32>(buf.size());
    log<"fs sdmmc: selftest blocks={} stride={} crc=0x{:08X} ms={} bytes={}">(
        ok_blocks, stride, crc, ms, bytes);
    return ok_blocks == blocks;
}

export void fs_demo_run() noexcept {
    (void)0;
}

export void fs_set_console_sink(out::channel_sink& sink) noexcept {
    g_sink = &sink;
}


