module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "sdio.h"
#include "usart.h"

export module player.stm32.fs_demo;

import util.core;
import fs_block;
import fs_core;
import fs_errno;
import fs_fatfs;
import fs_stream;
import fs_vfs;

namespace {
    constexpr util::u32 kBlockSize = 512;
    constexpr util::u32 kTimeoutMs = 1000;

    void uart_write(const char* text) noexcept {
        if (!text) return;
        HAL_UART_Transmit(&huart1,
            reinterpret_cast<uint8_t*>(const_cast<char*>(text)),
            static_cast<uint16_t>(std::strlen(text)), kTimeoutMs);
    }

    void uart_write_span(const char* text, std::size_t len) noexcept {
        if (!text || len == 0) return;
        HAL_UART_Transmit(&huart1,
            reinterpret_cast<uint8_t*>(const_cast<char*>(text)),
            static_cast<uint16_t>(len), kTimeoutMs);
    }

    std::size_t name_len(std::string_view name) noexcept {
        if (!name.data()) return 0;
        if (name.size() > 0) return name.size();
        return std::strlen(name.data());
    }

    void uart_write_uint(util::u64 value) noexcept {
        char buf[32]{};
        std::size_t pos = 0;
        if (value == 0) {
            buf[pos++] = '0';
        } else {
            char tmp[32]{};
            std::size_t len = 0;
            while (value > 0 && len < sizeof(tmp)) {
                tmp[len++] = static_cast<char>('0' + (value % 10));
                value /= 10;
            }
            while (len > 0) {
                buf[pos++] = tmp[--len];
            }
        }
        buf[pos++] = '\r';
        buf[pos++] = '\n';
        buf[pos] = '\0';
        uart_write(buf);
    }

    void uart_write_int(util::i32 value) noexcept {
        char buf[32]{};
        std::size_t pos = 0;
        if (value < 0) {
            buf[pos++] = '-';
            value = -value;
        }
        util::u32 v = static_cast<util::u32>(value);
        if (v == 0) {
            buf[pos++] = '0';
        } else {
            char tmp[32]{};
            std::size_t len = 0;
            while (v > 0 && len < sizeof(tmp)) {
                tmp[len++] = static_cast<char>('0' + (v % 10));
                v /= 10;
            }
            while (len > 0) {
                buf[pos++] = tmp[--len];
            }
        }
        buf[pos++] = '\r';
        buf[pos++] = '\n';
        buf[pos] = '\0';
        uart_write(buf);
    }

    struct SdBlockDevice {
        bool init() noexcept {
            uart_write("sdio: init begin\r\n");
            hsd.Instance = SDIO;
            hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
            hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
            hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
            hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
            hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
            hsd.Init.ClockDiv = 118;

            auto st = HAL_SD_Init(&hsd);
            if (st != HAL_OK) {
                uart_write("sdio: HAL_SD_Init failed\r\n");
                uart_write_uint(static_cast<util::u32>(st));
                return false;
            }
            st = HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_1B);
            if (st != HAL_OK) {
                uart_write("sdio: bus width failed\r\n");
                uart_write_uint(static_cast<util::u32>(st));
                return false;
            }
            uart_write("sdio: init ok\r\n");

            if (HAL_SD_GetCardInfo(&hsd, &info_) != HAL_OK) {
                uart_write("sdio: card info failed\r\n");
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
        static fs::Status read_impl(void* ctx, util::u64 lba, std::span<util::u8> data) noexcept {
            auto* self = static_cast<SdBlockDevice*>(ctx);
            if (!self || data.empty() || (data.size() % self->block_size_) != 0) {
                return fs::Status{fs::Err::inval};
            }
            const util::u32 count = static_cast<util::u32>(data.size() / self->block_size_);
            if (HAL_SD_ReadBlocks(&hsd, data.data(), static_cast<uint32_t>(lba), count, kTimeoutMs) != HAL_OK) {
                return fs::Status{fs::Err::io};
            }
            const util::u32 start = HAL_GetTick();
            while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {
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
            if (HAL_SD_WriteBlocks(&hsd, const_cast<uint8_t*>(data.data()),
                    static_cast<uint32_t>(lba), count, kTimeoutMs) != HAL_OK) {
                return fs::Status{fs::Err::io};
            }
            const util::u32 start = HAL_GetTick();
            while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {
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

    fs::Status list_cb(void*, const fs::MountOps::ListEntry& entry) noexcept {
        uart_write(entry.name.data());
        uart_write(" (");
        uart_write(entry.type == fs::NodeType::dir ? "dir" : "file");
        uart_write(") ");
        uart_write_uint(entry.size);
        return fs::Status{fs::Err::ok};
    }

    struct FirstFileCtx {
        char path[128]{};
        util::u32 count{0};
        bool found{false};
    };

    fs::Status list_first_file(void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
        auto* out = static_cast<FirstFileCtx*>(ctx);
        if (!out) return fs::Status{fs::Err::inval};
        out->count++;
        const auto len = name_len(entry.name);
        uart_write_span(entry.name.data(), len);
        uart_write(" (");
        uart_write(entry.type == fs::NodeType::dir ? "dir" : "file");
        uart_write(") ");
        uart_write_uint(entry.size);
        if (entry.type != fs::NodeType::file || out->found || len == 0) {
            return fs::Status{fs::Err::ok};
        }
        constexpr const char prefix[] = "/PICTURE/";
        std::size_t pos = 0;
        for (std::size_t i = 0; i < sizeof(prefix) - 1 && pos + 1 < sizeof(out->path); ++i) {
            out->path[pos++] = prefix[i];
        }
        for (std::size_t i = 0; i < len && pos + 1 < sizeof(out->path); ++i) {
            out->path[pos++] = entry.name.data()[i];
        }
        out->path[pos] = '\0';
        out->found = true;
        return fs::Status{fs::Err::ok};
    }
} // namespace

export void fs_demo_run() noexcept {
    uart_write("fs demo: init sd\r\n");

    static SdBlockDevice sd{};
    if (!sd.init()) {
        uart_write("fs demo: sd init failed\r\n");
        return;
    }

    static fs::FatFsMount mount{};
    static std::array<util::u8, 4096> cache{};
    auto st = mount.mount(sd.device(), std::span<util::u8>{cache}, false, 0);
    if (!st) {
        uart_write("fs demo: mount failed\r\n");
        uart_write_int(static_cast<util::i32>(st.err));
        return;
    }
    fs::set_mount(mount.mount_point());

    uart_write("fs demo: list /\r\n");
    st = fs::vfs_list("/", nullptr, &list_cb);
    if (!st) {
        uart_write("fs demo: list failed\r\n");
        uart_write_int(static_cast<util::i32>(st.err));
        return;
    }

    FirstFileCtx first{};
    uart_write("fs demo: list /PICTURE\r\n");
    st = fs::vfs_list("/PICTURE", &first, &list_first_file);
    if (!st) {
        uart_write("fs demo: list /PICTURE failed\r\n");
        uart_write_int(static_cast<util::i32>(st.err));
    }
    if (first.count == 0) {
        uart_write("fs demo: /PICTURE empty\r\n");
    }

    fs::File f{};
    const char* target = first.found ? first.path : "/readme.txt";
    uart_write("fs demo: open ");
    uart_write(target);
    uart_write("\r\n");
    st = fs::vfs_open(target, f);
    if (st) {
        std::array<util::u8, 64> buf{};
        st = fs::vfs_read(f, std::span<util::u8>{buf});
        (void)fs::vfs_close(f);
        if (st) {
            uart_write("fs demo: read ok\r\n");
        } else {
            uart_write("fs demo: read failed\r\n");
            uart_write_int(static_cast<util::i32>(st.err));
        }
    } else {
        uart_write("fs demo: open failed\r\n");
        uart_write_int(static_cast<util::i32>(st.err));
    }
}
