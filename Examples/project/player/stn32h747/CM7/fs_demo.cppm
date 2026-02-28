module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_sd.h"
#include "stm32h7xx_hal_uart.h"

export module player.stm32h7.fs_demo;

import util.core;
import fs_block;
import fs_core;
import fs_errno;
import fs_fatfs;
import fs_stream;
import fs_vfs;
import out.api;

extern "C" SD_HandleTypeDef hsd2;
extern "C" UART_HandleTypeDef huart1;

namespace {
    constexpr util::u32 kBlockSize = 512;
    constexpr util::u32 kTimeoutMs = 1000;

    void uart_write(const char* text) noexcept {
        if (!text) return;
        HAL_UART_Transmit(&huart1,
            reinterpret_cast<uint8_t*>(const_cast<char*>(text)),
            static_cast<uint16_t>(std::strlen(text)), kTimeoutMs);
    }

    bool sdmmc2_init_checked() noexcept {
        out::println<"sdio: mx init begin">();
        hsd2.Instance = SDMMC2;
        hsd2.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
        hsd2.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
        hsd2.Init.BusWide = SDMMC_BUS_WIDE_1B;
        hsd2.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
        hsd2.Init.ClockDiv = 255;
        out::println<"sdio: hal init begin">();
        const auto st = HAL_SD_Init(&hsd2);
        out::println<"sdio: hal init ret 0x{:02X}">(static_cast<util::u32>(st));
        if (st != HAL_OK) {
            out::println<"sdio: hal err 0x{:08X}">(static_cast<util::u32>(HAL_SD_GetError(&hsd2)));
            out::println<"sdio: hsd err 0x{:08X}">(static_cast<util::u32>(hsd2.ErrorCode));
            return false;
        }
        const auto wide = HAL_SD_ConfigWideBusOperation(&hsd2, SDMMC_BUS_WIDE_1B);
        out::println<"sdio: bus1 ret 0x{:02X}">(static_cast<util::u32>(wide));
        if (wide != HAL_OK) {
            out::println<"sdio: bus1 err 0x{:08X}">(static_cast<util::u32>(HAL_SD_GetError(&hsd2)));
            out::println<"sdio: hsd err 0x{:08X}">(static_cast<util::u32>(hsd2.ErrorCode));
            return false;
        }
        return wide == HAL_OK;
    }

    struct SdBlockDevice {
        bool init() noexcept {
            out::println<"sdio: init begin">();
            if (!sdmmc2_init_checked()) {
                out::println<"sdio: HAL_SD_Init failed">();
                return false;
            }
            out::println<"sdio: init ok">();

            if (HAL_SD_GetCardInfo(&hsd2, &info_) != HAL_OK) {
                out::println<"sdio: card info failed">();
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
        util::u32 block_size_{kBlockSize};
        util::u32 block_count_{0};
    };
} // namespace

export bool fs_boot_init() noexcept {
    static SdBlockDevice sd{};
    if (!sd.init()) {
        return false;
    }
    out::println<"fs boot: mount begin">();
    static fs::FatFsMount mount{};
    static std::array<util::u8, 4096> cache{};
    auto st = mount.mount(sd.device(), std::span<util::u8>{cache}, false, 0);
    if (!st) {
        out::println<"fs boot: mount failed">();
        return false;
    }
    fs::set_mount(mount.mount_point());
    out::println<"fs boot: mount ok">();
    return true;
}

export void fs_demo_run() noexcept {
    (void)0;
}
