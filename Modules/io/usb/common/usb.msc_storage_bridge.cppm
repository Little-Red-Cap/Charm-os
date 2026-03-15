module;

#include <cstddef>
#include <cstdint>
#include <span>

export module usb.msc_storage_bridge;

import util.core;
import block.device;

block::Device* g_dev = nullptr;
bool g_read_only = false;

export namespace usb::msc::bridge {
    inline void set_block_device(block::Device* dev, bool read_only = false) noexcept {
        g_dev = dev;
        g_read_only = read_only;
    }

    inline block::Device* block_device() noexcept {
        return g_dev;
    }

    inline bool is_read_only() noexcept {
        return g_read_only;
    }
}

extern "C" bool charm_usb_storage_init(void) {
    return g_dev != nullptr;
}

extern "C" bool charm_usb_storage_is_ready(void) {
    return g_dev != nullptr;
}

extern "C" bool charm_usb_storage_is_write_protected(void) {
    return g_read_only;
}

extern "C" bool charm_usb_storage_get_capacity(std::uint32_t* block_num,
                                              std::uint16_t* block_size) {
    if (!g_dev || !block_num || !block_size) return false;
    if (g_dev->block_size == 0 || g_dev->block_count == 0) return false;
    if (g_dev->block_size > 0xFFFFu) return false;
    *block_num = static_cast<std::uint32_t>(g_dev->block_count);
    *block_size = static_cast<std::uint16_t>(g_dev->block_size);
    return true;
}

extern "C" bool charm_usb_storage_read(std::uint8_t* buf,
                                       std::uint32_t blk_addr,
                                       std::uint16_t blk_len) {
    if (!g_dev || !g_dev->read || !buf || blk_len == 0) return false;
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
    if (g_read_only) return false;
    if (!g_dev || !g_dev->write || !buf || blk_len == 0) return false;
    const std::size_t bytes = static_cast<std::size_t>(blk_len) *
        static_cast<std::size_t>(g_dev->block_size);
    auto st = g_dev->write(g_dev->ctx,
        static_cast<util::u64>(blk_addr),
        std::span<const util::u8>(buf, bytes));
    return static_cast<bool>(st);
}
