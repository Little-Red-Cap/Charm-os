module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#if defined(STM32H7xx) || defined(STM32H747xx)
#include "sdmmc.h"
#else
#include "sdio.h"
#endif
#include "usart.h"

export module player.stm32.fs_demo;

import util.core;
import fs_block;
import fs_core;
import fs_errno;
import fs_fatfs;
import fs_stream;
import fs_vfs;
import lcd_driver;

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
#if defined(STM32H7xx) || defined(STM32H747xx)
            MX_SDMMC2_SD_Init();
            auto st = HAL_SD_Init(&hsd2);
            if (st != HAL_OK) {
                uart_write("sdio: HAL_SD_Init failed\r\n");
                uart_write_uint(static_cast<util::u32>(st));
                return false;
            }
#else
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
#endif
            uart_write("sdio: init ok\r\n");

#if defined(STM32H7xx) || defined(STM32H747xx)
            if (HAL_SD_GetCardInfo(&hsd2, &info_) != HAL_OK) {
#else
            if (HAL_SD_GetCardInfo(&hsd, &info_) != HAL_OK) {
#endif
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
#if defined(STM32H7xx) || defined(STM32H747xx)
                if (HAL_SD_ReadBlocks_DMA(&hsd2, data.data(), static_cast<uint32_t>(lba), count) != HAL_OK) {
#else
                if (HAL_SD_ReadBlocks_DMA(&hsd, data.data(), static_cast<uint32_t>(lba), count) != HAL_OK) {
#endif
                    return fs::Status{fs::Err::io};
                }
            } else {
#if defined(STM32H7xx) || defined(STM32H747xx)
                if (HAL_SD_ReadBlocks(&hsd2, data.data(), static_cast<uint32_t>(lba), count, kTimeoutMs) != HAL_OK) {
#else
                if (HAL_SD_ReadBlocks(&hsd, data.data(), static_cast<uint32_t>(lba), count, kTimeoutMs) != HAL_OK) {
#endif
                    return fs::Status{fs::Err::io};
                }
            }
            const util::u32 start = HAL_GetTick();
#if defined(STM32H7xx) || defined(STM32H747xx)
            while (HAL_SD_GetCardState(&hsd2) != HAL_SD_CARD_TRANSFER) {
#else
            while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {
#endif
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
#if defined(STM32H7xx) || defined(STM32H747xx)
                if (HAL_SD_WriteBlocks_DMA(&hsd2, const_cast<uint8_t*>(data.data()),
                        static_cast<uint32_t>(lba), count) != HAL_OK) {
#else
                if (HAL_SD_WriteBlocks_DMA(&hsd, const_cast<uint8_t*>(data.data()),
                        static_cast<uint32_t>(lba), count) != HAL_OK) {
#endif
                    return fs::Status{fs::Err::io};
                }
            } else {
#if defined(STM32H7xx) || defined(STM32H747xx)
                if (HAL_SD_WriteBlocks(&hsd2, const_cast<uint8_t*>(data.data()),
                        static_cast<uint32_t>(lba), count, kTimeoutMs) != HAL_OK) {
#else
                if (HAL_SD_WriteBlocks(&hsd, const_cast<uint8_t*>(data.data()),
                        static_cast<uint32_t>(lba), count, kTimeoutMs) != HAL_OK) {
#endif
                    return fs::Status{fs::Err::io};
                }
            }
            const util::u32 start = HAL_GetTick();
#if defined(STM32H7xx) || defined(STM32H747xx)
            while (HAL_SD_GetCardState(&hsd2) != HAL_SD_CARD_TRANSFER) {
#else
            while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {
#endif
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
        const auto len = name_len(entry.name);
        uart_write_span(entry.name.data(), len);
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

    struct BmpFileCtx {
        char path[128]{};
        util::u32 count{0};
        bool found{false};
    };

    constexpr const char kPicturePrefix[] = "/PICTURE/";

    char ascii_lower(char c) noexcept {
        if (c >= 'A' && c <= 'Z') return static_cast<char>(c + ('a' - 'A'));
        return c;
    }

    bool is_bmp_name(std::string_view name) noexcept {
        const auto len = name_len(name);
        if (len < 4) return false;
        const char c0 = ascii_lower(name.data()[len - 4]);
        const char c1 = ascii_lower(name.data()[len - 3]);
        const char c2 = ascii_lower(name.data()[len - 2]);
        const char c3 = ascii_lower(name.data()[len - 1]);
        return c0 == '.' && c1 == 'b' && c2 == 'm' && c3 == 'p';
    }

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

    fs::Status list_first_bmp(void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
        auto* out = static_cast<BmpFileCtx*>(ctx);
        if (!out) return fs::Status{fs::Err::inval};
        out->count++;
        if (entry.type != fs::NodeType::file || out->found) {
            return fs::Status{fs::Err::ok};
        }
        if (!is_bmp_name(entry.name)) {
            return fs::Status{fs::Err::ok};
        }
        const auto len = name_len(entry.name);
        std::size_t pos = 0;
        for (std::size_t i = 0; i < sizeof(kPicturePrefix) - 1 && pos + 1 < sizeof(out->path); ++i) {
            out->path[pos++] = kPicturePrefix[i];
        }
        for (std::size_t i = 0; i < len && pos + 1 < sizeof(out->path); ++i) {
            out->path[pos++] = entry.name.data()[i];
        }
        out->path[pos] = '\0';
        out->found = true;
        return fs::Status{fs::Err::ok};
    }

    util::u16 read_u16_le(const util::u8* data) noexcept {
        return static_cast<util::u16>(data[0] | (static_cast<util::u16>(data[1]) << 8));
    }

    util::u32 read_u32_le(const util::u8* data) noexcept {
        return static_cast<util::u32>(data[0])
            | (static_cast<util::u32>(data[1]) << 8)
            | (static_cast<util::u32>(data[2]) << 16)
            | (static_cast<util::u32>(data[3]) << 24);
    }

    util::i32 read_i32_le(const util::u8* data) noexcept {
        return static_cast<util::i32>(read_u32_le(data));
    }

    void dump_bmp_header(const util::u8* data, std::size_t len) noexcept {
        if (!data || len < 54) {
            uart_write("bmp: header too small\r\n");
            return;
        }
        if (data[0] != 'B' || data[1] != 'M') {
            uart_write("bmp: signature mismatch\r\n");
            return;
        }
        const util::u32 pixel_off = read_u32_le(data + 10);
        const util::u32 dib_size = read_u32_le(data + 14);
        const util::u32 width = read_u32_le(data + 18);
        const util::u32 height = read_u32_le(data + 22);
        const util::u16 bpp = read_u16_le(data + 28);
        uart_write("bmp: w=");
        uart_write_uint(width);
        uart_write("bmp: h=");
        uart_write_uint(height);
        uart_write("bmp: bpp=");
        uart_write_uint(bpp);
        uart_write("bmp: off=");
        uart_write_uint(pixel_off);
        uart_write("bmp: dib=");
        uart_write_uint(dib_size);
    }

    bool render_bmp_24(fs::File& f, util::u32 width, util::i32 height, util::u32 pixel_off) noexcept {
        if (width == 0 || height == 0) return false;
        const bool top_down = height < 0;
        const util::u32 abs_h = static_cast<util::u32>(top_down ? -height : height);
        const util::u32 row_bytes = ((width * 3u) + 3u) & ~3u;

        static std::array<util::u8, 2048> row_buf{};
        static std::array<util::u16, 512> line_buf{};
        if (row_bytes > row_buf.size() || width > line_buf.size()) {
            uart_write("bmp: row buffer too small\r\n");
            return false;
        }

        auto st = fs::vfs_seek(f, static_cast<util::i64>(pixel_off));
        if (!st) return false;
        for (util::u32 row = 0; row < abs_h; ++row) {
            auto row_span = std::span<util::u8>{row_buf.data(), row_bytes};
            st = fs::vfs_read(f, row_span);
            if (!st) return false;
            for (util::u32 x = 0; x < width; ++x) {
                const util::u32 idx = x * 3u;
                const util::u8 b = row_buf[idx];
                const util::u8 g = row_buf[idx + 1];
                const util::u8 r = row_buf[idx + 2];
                line_buf[x] = static_cast<util::u16>(((r & 0xF8) << 8)
                    | ((g & 0xFC) << 3)
                    | (b >> 3));
            }
            const util::u32 dst_y = top_down ? row : (abs_h - 1u - row);
            LCD_BlitRect565(0, static_cast<util::u16>(dst_y), static_cast<util::u16>(width), 1, line_buf.data());
        }
        return true;
    }

    struct ListCtx {
        const char* base{nullptr};
        int depth{0};
    };

    constexpr int kMaxListDepth = 4;

    void uart_write_indent(int depth) noexcept {
        for (int i = 0; i < depth; ++i) {
            uart_write("  ");
        }
    }

    bool is_dot_dir(std::string_view name) noexcept {
        if (name.size() == 1 && name[0] == '.') return true;
        if (name.size() == 2 && name[0] == '.' && name[1] == '.') return true;
        return false;
    }

    bool is_system_volume_info(std::string_view name) noexcept {
        constexpr std::string_view target = "System Volume Information";
        if (name.size() != target.size()) return false;
        for (std::size_t i = 0; i < target.size(); ++i) {
            const char a = ascii_lower(name[i]);
            const char b = ascii_lower(target[i]);
            if (a != b) return false;
        }
        return true;
    }

    fs::Status list_recursive(const char* path, int depth) noexcept;

    fs::Status list_recursive_cb(void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
        auto* info = static_cast<ListCtx*>(ctx);
        if (!info || !info->base) return fs::Status{fs::Err::inval};
        const auto len = name_len(entry.name);
        const std::string_view name{entry.name.data(), len};

        uart_write_indent(info->depth);
        uart_write_span(entry.name.data(), len);
        uart_write(" (");
        uart_write(entry.type == fs::NodeType::dir ? "dir" : "file");
        uart_write(") ");
        uart_write_uint(entry.size);

        if (entry.type != fs::NodeType::dir || info->depth >= kMaxListDepth) {
            return fs::Status{fs::Err::ok};
        }
        if (is_dot_dir(name) || is_system_volume_info(name)) {
            return fs::Status{fs::Err::ok};
        }

        char child[256]{};
        std::size_t pos = 0;
        const auto base_len = std::strlen(info->base);
        if (base_len + 1 + len + 1 >= sizeof(child)) {
            return fs::Status{fs::Err::nametoolong};
        }
        std::memcpy(child, info->base, base_len);
        pos = base_len;
        if (pos == 0 || child[pos - 1] != '/') {
            child[pos++] = '/';
        }
        for (std::size_t i = 0; i < len; ++i) {
            child[pos++] = entry.name.data()[i];
        }
        child[pos] = '\0';
        return list_recursive(child, info->depth + 1);
    }

    fs::Status list_recursive(const char* path, int depth) noexcept {
        if (!path) return fs::Status{fs::Err::inval};
        uart_write("fs demo: list ");
        uart_write(path);
        uart_write("\r\n");
        ListCtx ctx{path, depth};
        return fs::vfs_list(path, &ctx, &list_recursive_cb);
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
    st = list_recursive("/", 0);
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

    uart_write("fs demo: bmp demo disabled\r\n");

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

export bool fs_boot_init() noexcept {
    static SdBlockDevice sd{};
    if (!sd.init()) {
        return false;
    }
    uart_write("fs boot: mount begin\r\n");
    static fs::FatFsMount mount{};
    static std::array<util::u8, 4096> cache{};
    auto st = mount.mount(sd.device(), std::span<util::u8>{cache}, false, 0);
    if (!st) {
        uart_write("fs boot: mount failed\r\n");
        return false;
    }
    fs::set_mount(mount.mount_point());
    uart_write("fs boot: mount ok\r\n");
    return true;
}
