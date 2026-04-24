module;

#include <cstdint>
#include <span>

#include "stm32h7xx_hal.h"
#include "sdmmc.h"

export module player.stm32h7.board_sdmmc;

import block.device;
import block.sdmmc;
import util.core;
import player.stm32h7.board_config;

namespace {
    constexpr util::u32 kTimeoutMs = 1000;

    bool g_sdmmc_hw_inited = false;

    void sdmmc_hw_init_once() noexcept;

    block::Status sdmmc_wait_ready_mmc(MMC_HandleTypeDef* card) noexcept {
        const util::u32 start = HAL_GetTick();
        while (HAL_MMC_GetCardState(card) != HAL_MMC_CARD_TRANSFER) {
            if ((HAL_GetTick() - start) > kTimeoutMs) {
                return block::Status{block::Errc::timeout};
            }
        }
        return block::Status{block::Errc::ok};
    }

    block::Status sdmmc_wait_ready_sd(SD_HandleTypeDef* card) noexcept {
        const util::u32 start = HAL_GetTick();
        while (HAL_SD_GetCardState(card) != HAL_SD_CARD_TRANSFER) {
            if ((HAL_GetTick() - start) > kTimeoutMs) {
                return block::Status{block::Errc::timeout};
            }
        }
        return block::Status{block::Errc::ok};
    }

    block::Status mmc_init(void* ctx, const block::SdmmcConfig& cfg, block::SdmmcInfo& out) noexcept {
        auto* card = static_cast<MMC_HandleTypeDef*>(ctx);
        if (!card) return block::Status{block::Errc::inval};
        sdmmc_hw_init_once();

        if (cfg.bus_width == 8) {
            (void)HAL_MMC_ConfigWideBusOperation(card, SDMMC_BUS_WIDE_8B);
        } else if (cfg.bus_width == 4) {
            (void)HAL_MMC_ConfigWideBusOperation(card, SDMMC_BUS_WIDE_4B);
        } else {
            (void)HAL_MMC_ConfigWideBusOperation(card, SDMMC_BUS_WIDE_1B);
        }

        MODIFY_REG(card->Instance->CLKCR, SDMMC_CLKCR_CLKDIV,
                   player::stm32h7::board::kSdmmc.xfer_clock_div);
        card->Init.ClockDiv = player::stm32h7::board::kSdmmc.xfer_clock_div;

        HAL_MMC_CardInfoTypeDef info{};
        if (HAL_MMC_GetCardInfo(card, &info) != HAL_OK) {
            return block::Status{block::Errc::io};
        }
        const util::u64 block_size = info.LogBlockSize ? info.LogBlockSize : info.BlockSize;
        const util::u64 block_count = info.LogBlockNbr ? info.LogBlockNbr : info.BlockNbr;
        if (block_size == 0 || block_count == 0) {
            return block::Status{block::Errc::inval};
        }
        out.block_size = block_size;
        out.block_count = block_count;
        return block::Status{block::Errc::ok};
    }

    block::Status mmc_read(void* ctx, util::u64 lba, std::span<util::u8> data) noexcept {
        auto* card = static_cast<MMC_HandleTypeDef*>(ctx);
        if (!card) return block::Status{block::Errc::inval};
        const util::u64 block_size =
            card->MmcCard.LogBlockSize ? card->MmcCard.LogBlockSize : card->MmcCard.BlockSize;
        if (data.empty() || block_size == 0 || (data.size() % block_size) != 0) {
            return block::Status{block::Errc::inval};
        }
        const util::u32 count = static_cast<util::u32>(data.size() / block_size);
        if (HAL_MMC_ReadBlocks(card, data.data(), static_cast<uint32_t>(lba), count, kTimeoutMs) != HAL_OK) {
            return block::Status{block::Errc::io};
        }
        return sdmmc_wait_ready_mmc(card);
    }

    block::Status mmc_write(void* ctx, util::u64 lba, std::span<const util::u8> data) noexcept {
        auto* card = static_cast<MMC_HandleTypeDef*>(ctx);
        if (!card) return block::Status{block::Errc::inval};
        const util::u64 block_size =
            card->MmcCard.LogBlockSize ? card->MmcCard.LogBlockSize : card->MmcCard.BlockSize;
        if (data.empty() || block_size == 0 || (data.size() % block_size) != 0) {
            return block::Status{block::Errc::inval};
        }
        const util::u32 count = static_cast<util::u32>(data.size() / block_size);
        if (HAL_MMC_WriteBlocks(card, const_cast<uint8_t*>(data.data()),
                static_cast<uint32_t>(lba), count, kTimeoutMs) != HAL_OK) {
            return block::Status{block::Errc::io};
        }
        return sdmmc_wait_ready_mmc(card);
    }

    block::Status mmc_erase(void*, util::u64, util::u64) noexcept {
        return block::Status{block::Errc::nosys};
    }

    block::Status mmc_flush(void*) noexcept {
        return block::Status{block::Errc::ok};
    }

    const block::SdmmcOps kMmcOps{
        &mmc_init,
        &mmc_read,
        &mmc_write,
        &mmc_erase,
        &mmc_flush
    };

    block::Status sd_init(void* ctx, const block::SdmmcConfig& cfg, block::SdmmcInfo& out) noexcept {
        auto* card = static_cast<SD_HandleTypeDef*>(ctx);
        if (!card) return block::Status{block::Errc::inval};
        sdmmc_hw_init_once();

        if (cfg.bus_width == 4) {
            (void)HAL_SD_ConfigWideBusOperation(card, SDMMC_BUS_WIDE_4B);
        } else {
            (void)HAL_SD_ConfigWideBusOperation(card, SDMMC_BUS_WIDE_1B);
        }

        MODIFY_REG(card->Instance->CLKCR, SDMMC_CLKCR_CLKDIV,
                   player::stm32h7::board::kSdmmc.xfer_clock_div);
        card->Init.ClockDiv = player::stm32h7::board::kSdmmc.xfer_clock_div;

        HAL_SD_CardInfoTypeDef info{};
        if (HAL_SD_GetCardInfo(card, &info) != HAL_OK) {
            return block::Status{block::Errc::io};
        }
        const util::u64 block_size = info.LogBlockSize ? info.LogBlockSize : info.BlockSize;
        const util::u64 block_count = info.LogBlockNbr ? info.LogBlockNbr : info.BlockNbr;
        if (block_size == 0 || block_count == 0) {
            return block::Status{block::Errc::inval};
        }
        out.block_size = block_size;
        out.block_count = block_count;
        return block::Status{block::Errc::ok};
    }

    block::Status sd_read(void* ctx, util::u64 lba, std::span<util::u8> data) noexcept {
        auto* card = static_cast<SD_HandleTypeDef*>(ctx);
        if (!card) return block::Status{block::Errc::inval};
        const util::u64 block_size =
            card->SdCard.LogBlockSize ? card->SdCard.LogBlockSize : card->SdCard.BlockSize;
        if (data.empty() || block_size == 0 || (data.size() % block_size) != 0) {
            return block::Status{block::Errc::inval};
        }
        const util::u32 count = static_cast<util::u32>(data.size() / block_size);
        if (HAL_SD_ReadBlocks(card, data.data(), static_cast<uint32_t>(lba), count, kTimeoutMs) != HAL_OK) {
            return block::Status{block::Errc::io};
        }
        return sdmmc_wait_ready_sd(card);
    }

    block::Status sd_write(void* ctx, util::u64 lba, std::span<const util::u8> data) noexcept {
        auto* card = static_cast<SD_HandleTypeDef*>(ctx);
        if (!card) return block::Status{block::Errc::inval};
        const util::u64 block_size =
            card->SdCard.LogBlockSize ? card->SdCard.LogBlockSize : card->SdCard.BlockSize;
        if (data.empty() || block_size == 0 || (data.size() % block_size) != 0) {
            return block::Status{block::Errc::inval};
        }
        const util::u32 count = static_cast<util::u32>(data.size() / block_size);
        if (HAL_SD_WriteBlocks(card, const_cast<uint8_t*>(data.data()),
                static_cast<uint32_t>(lba), count, kTimeoutMs) != HAL_OK) {
            return block::Status{block::Errc::io};
        }
        return sdmmc_wait_ready_sd(card);
    }

    block::Status sd_erase(void*, util::u64, util::u64) noexcept {
        return block::Status{block::Errc::nosys};
    }

    block::Status sd_flush(void*) noexcept {
        return block::Status{block::Errc::ok};
    }

    const block::SdmmcOps kSdOps{
        &sd_init,
        &sd_read,
        &sd_write,
        &sd_erase,
        &sd_flush
    };

    void sdmmc_hw_init_once() noexcept {
        if (g_sdmmc_hw_inited) return;
        g_sdmmc_hw_inited = true;
        if (player::stm32h7::board::kSdmmc.card == player::stm32h7::board::SdmmcCard::mmc) {
            MX_SDMMC1_MMC_Init();
        } else {
            MX_SDMMC2_SD_Init();
        }
    }
} // namespace

export namespace player::stm32h7::board {
    struct SdmmcDiag {
        std::uint32_t rcc_src;
        std::uint32_t sdmmc1_en;
        std::uint32_t rcc_ahb2;
        std::uint32_t rcc_ahb3;
        std::uint32_t gpioa_moder;
        std::uint32_t gpioa_pupd;
        std::uint32_t gpioa_afr0;
        std::uint32_t gpioa_afr1;
        std::uint32_t gpioc_moder;
        std::uint32_t gpioc_pupd;
        std::uint32_t gpioc_afr0;
        std::uint32_t gpioc_afr1;
        std::uint32_t gpiod_moder;
        std::uint32_t gpiod_pupd;
        std::uint32_t gpiod_afr0;
        std::uint32_t gpiod_afr1;
        std::uint32_t gpioe_moder;
        std::uint32_t gpioe_pupd;
        std::uint32_t gpioe_afr0;
        std::uint32_t gpioe_afr1;
        std::uint32_t pin_cmd;
        std::uint32_t pin_d0;
        std::uint32_t pin_d1;
        std::uint32_t pin_d2;
        std::uint32_t pin_d3;
        std::uint32_t pin_ck;
    };

    void sdmmc_hw_init() noexcept {
        sdmmc_hw_init_once();
    }

    block::SdmmcHandle sdmmc_handle() noexcept {
        if (kSdmmc.card == SdmmcCard::mmc) {
            return block::SdmmcHandle{&hmmc1, &kMmcOps};
        }
        return block::SdmmcHandle{&hsd2, &kSdOps};
    }

    block::SdmmcConfig sdmmc_config() noexcept {
        block::SdmmcConfig cfg{};
        cfg.clock_hz = 0;
        cfg.bus_width = kSdmmc.bus_width;
        cfg.use_dma = kSdmmc.use_dma;
        return cfg;
    }

    SdmmcDiag sdmmc_diag_snapshot() noexcept {
        SdmmcDiag diag{};
        diag.rcc_src = static_cast<std::uint32_t>(__HAL_RCC_GET_SDMMC_SOURCE());
        diag.sdmmc1_en = (__HAL_RCC_SDMMC1_IS_CLK_ENABLED() != 0u) ? 1u : 0u;
        diag.rcc_ahb2 = static_cast<std::uint32_t>(READ_REG(RCC->AHB2ENR));
        diag.rcc_ahb3 = static_cast<std::uint32_t>(READ_REG(RCC->AHB3ENR));

        diag.gpioa_moder = static_cast<std::uint32_t>(GPIOA->MODER);
        diag.gpioa_pupd = static_cast<std::uint32_t>(GPIOA->PUPDR);
        diag.gpioa_afr0 = static_cast<std::uint32_t>(GPIOA->AFR[0]);
        diag.gpioa_afr1 = static_cast<std::uint32_t>(GPIOA->AFR[1]);

        diag.gpioc_moder = static_cast<std::uint32_t>(GPIOC->MODER);
        diag.gpioc_pupd = static_cast<std::uint32_t>(GPIOC->PUPDR);
        diag.gpioc_afr0 = static_cast<std::uint32_t>(GPIOC->AFR[0]);
        diag.gpioc_afr1 = static_cast<std::uint32_t>(GPIOC->AFR[1]);

        diag.gpiod_moder = static_cast<std::uint32_t>(GPIOD->MODER);
        diag.gpiod_pupd = static_cast<std::uint32_t>(GPIOD->PUPDR);
        diag.gpiod_afr0 = static_cast<std::uint32_t>(GPIOD->AFR[0]);
        diag.gpiod_afr1 = static_cast<std::uint32_t>(GPIOD->AFR[1]);

        diag.gpioe_moder = static_cast<std::uint32_t>(GPIOE->MODER);
        diag.gpioe_pupd = static_cast<std::uint32_t>(GPIOE->PUPDR);
        diag.gpioe_afr0 = static_cast<std::uint32_t>(GPIOE->AFR[0]);
        diag.gpioe_afr1 = static_cast<std::uint32_t>(GPIOE->AFR[1]);

        diag.pin_cmd = static_cast<std::uint32_t>(HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2));
        diag.pin_d0 = static_cast<std::uint32_t>(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_8));
        diag.pin_d1 = static_cast<std::uint32_t>(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9));
        diag.pin_d2 = static_cast<std::uint32_t>(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_10));
        diag.pin_d3 = static_cast<std::uint32_t>(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_11));
        diag.pin_ck = static_cast<std::uint32_t>(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_12));
        return diag;
    }
}
