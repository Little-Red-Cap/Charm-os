module;

#define CHARM_ALLOW_HAL 1

#include <cstdint>

#include "stm32h7xx_hal.h"
#include <span>

#include "sdmmc.h"

export module player.runtime.hqzy_cm7.sdmmc_glue;

import charm.system.time;
import block.device;
import block.sdmmc;
import util.core;
import util.error;

export namespace player::app_test_hqzy::sdmmc_glue {
    struct SdmmcRuntime {
        SD_HandleTypeDef* sd{nullptr};
        util::u32 block_size{512};
        util::Errc last_err{util::Errc::ok};
    };

    inline block::Status status_from_hal(HAL_StatusTypeDef st) noexcept {
        switch (st) {
        case HAL_OK:
            return block::Status{block::Errc::ok};
        case HAL_BUSY:
            return block::Status{block::Errc::busy};
        case HAL_TIMEOUT:
            return block::Status{block::Errc::timeout};
        default:
            return block::Status{block::Errc::io};
        }
    }

    inline util::Errc project_block_err(block::Errc err) noexcept {
        switch (err) {
        case block::Errc::ok: return util::Errc::ok;
        case block::Errc::busy: return util::Errc::busy;
        case block::Errc::timeout: return util::Errc::timeout;
        case block::Errc::invalid_arg: return util::Errc::invalid_arg;
        default: return util::Errc::io;
        }
    }

    inline bool wait_ready(SD_HandleTypeDef* sd, util::u64 deadline_ms) noexcept {
        if (!sd || !charm::system::time::bound()) return false;
        while (HAL_SD_GetCardState(sd) != HAL_SD_CARD_TRANSFER) {
            if (charm::system::time::now_ms() >= deadline_ms) {
                return false;
            }
        }
        return true;
    }

    inline block::Status init(void* ctx, const block::SdmmcConfig& cfg,
                              block::SdmmcInfo& out) noexcept {
        auto* rt = static_cast<SdmmcRuntime*>(ctx);
        if (!rt || !rt->sd) return block::Status{block::Errc::invalid_arg};
        auto* sd = rt->sd;

        const auto init_status = HAL_SD_Init(sd);
        if (init_status != HAL_OK) {
            auto st = status_from_hal(init_status);
            rt->last_err = project_block_err(st.err);
            return st;
        }

        if (cfg.bus_width == 4) {
            (void)HAL_SD_ConfigWideBusOperation(sd, SDMMC_BUS_WIDE_4B);
        } else if (cfg.bus_width == 1) {
            (void)HAL_SD_ConfigWideBusOperation(sd, SDMMC_BUS_WIDE_1B);
        }

        HAL_SD_CardInfoTypeDef info{};
        if (HAL_SD_GetCardInfo(sd, &info) != HAL_OK) {
            rt->last_err = util::Errc::io;
            return block::Status{block::Errc::io};
        }

        const util::u32 block_size = info.LogBlockSize ? info.LogBlockSize : info.BlockSize;
        const util::u32 block_count = info.LogBlockNbr ? info.LogBlockNbr : info.BlockNbr;
        if (block_size == 0 || block_count == 0) {
            rt->last_err = util::Errc::invalid_arg;
            return block::Status{block::Errc::invalid_arg};
        }

        rt->block_size = block_size;
        rt->last_err = util::Errc::ok;
        out.block_size = block_size;
        out.block_count = block_count;
        return block::Status{block::Errc::ok};
    }

    inline block::Status read(void* ctx, util::u64 lba,
                              std::span<util::u8> data) noexcept {
        auto* rt = static_cast<SdmmcRuntime*>(ctx);
        if (!rt || !rt->sd) return block::Status{block::Errc::invalid_arg};
        auto* sd = rt->sd;
        if (data.empty() || rt->block_size == 0) {
            rt->last_err = util::Errc::invalid_arg;
            return block::Status{block::Errc::invalid_arg};
        }

        const util::u32 blocks = static_cast<util::u32>(data.size() / rt->block_size);
        if (blocks == 0) {
            rt->last_err = util::Errc::invalid_arg;
            return block::Status{block::Errc::invalid_arg};
        }

        const auto st = HAL_SD_ReadBlocks(sd, data.data(),
            static_cast<uint32_t>(lba), blocks, 1000);
        if (st != HAL_OK) {
            auto status = status_from_hal(st);
            rt->last_err = project_block_err(status.err);
            return status;
        }
        if (!charm::system::time::bound()) {
            rt->last_err = util::Errc::bad_state;
            return block::Status{block::Errc::io};
        }
        const auto deadline = charm::system::time::now_ms() + 1000u;
        if (!wait_ready(sd, deadline)) {
            rt->last_err = util::Errc::timeout;
            return block::Status{block::Errc::timeout};
        }
        rt->last_err = util::Errc::ok;
        return block::Status{block::Errc::ok};
    }

    inline const block::SdmmcOps kOps{
        &init,
        &read,
        nullptr,
        nullptr,
        nullptr
    };
} // namespace player::app_test_hqzy::sdmmc_glue
