#include <cstddef>
#include <cstdint>
#include <span>

import util.core;
import fs_block;
import player.stm32h7.fs_demo_mmc;

namespace {
    fs::BlockDevice* g_dev = nullptr;
    bool g_inited = false;

    bool ensure_device() noexcept {
        if (g_inited) return g_dev != nullptr;
        g_dev = fs_sd_block_device();
        g_inited = true;
        return g_dev != nullptr;
    }
}

extern "C" bool charm_usb_storage_init(void) {
    return ensure_device();
}

extern "C" bool charm_usb_storage_is_ready(void) {
    return ensure_device();
}

extern "C" bool charm_usb_storage_is_write_protected(void) {
    return false;
}

extern "C" bool charm_usb_storage_get_capacity(std::uint32_t* block_num,
                                              std::uint16_t* block_size) {
    if (!ensure_device() || !block_num || !block_size) return false;
    if (g_dev->block_size == 0 || g_dev->block_count == 0) return false;
    if (g_dev->block_size > 0xFFFFu) return false;
    *block_num = static_cast<std::uint32_t>(g_dev->block_count);
    *block_size = static_cast<std::uint16_t>(g_dev->block_size);
    return true;
}

extern "C" bool charm_usb_storage_read(std::uint8_t* buf,
                                       std::uint32_t blk_addr,
                                       std::uint16_t blk_len) {
    if (!ensure_device() || !buf || blk_len == 0) return false;
    const std::size_t bytes = static_cast<std::size_t>(blk_len) *
        static_cast<std::size_t>(g_dev->block_size);
    auto st = g_dev->read(g_dev->ctx,
        static_cast<util::u64>(blk_addr),
        std::span<util::u8>(buf, bytes));
    return static_cast<bool>(st);
}

extern "C" bool charm_usb_storage_write(const std::uint8_t* buf,
                                        std::uint32_t blk_addr,
                                        std::uint16_t blk_len) {
    if (!ensure_device() || !g_dev->write || !blk_len) return false;
    const std::size_t bytes = static_cast<std::size_t>(blk_len) *
        static_cast<std::size_t>(g_dev->block_size);
    auto st = g_dev->write(g_dev->ctx,
        static_cast<util::u64>(blk_addr),
        std::span<const util::u8>(buf, bytes));
    return static_cast<bool>(st);
}
