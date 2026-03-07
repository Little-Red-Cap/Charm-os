module;

#include <cstddef>
#include <span>

export module block.cache;

import block.device;
import fs_errno;
import fs_mal;
import fs_mal_block;
import fs_mal_cache;
import util.core;

export namespace block {
    template <util::usize MaxEntries>
    class CachedDevice {
    public:
        CachedDevice() = default;
        CachedDevice(const CachedDevice&) = delete;
        CachedDevice& operator=(const CachedDevice&) = delete;

        Status bind(Device& dev, std::span<util::u8> cache_buf) noexcept {
            if (!dev.read) return Status{Errc::nosys};
            if (!dev.write) return Status{Errc::rofs};
            if (dev.block_size == 0 || dev.block_count == 0) return Status{Errc::inval};
            auto st = mal_block_.bind(dev);
            if (!st) return st;
            st = cache_.bind(mal_block_.device(), cache_buf);
            if (!st) return st;
            cached_ = {};
            fs::mal_to_block(cache_.device(), cached_);
            cached_.caps = dev.caps ? dev.caps : caps_from_ops(dev);
            cached_.caps |= to_bits(Caps::cached);
            return Status{Errc::ok};
        }

        void unbind() noexcept {
            cached_ = {};
            mal_block_.unbind();
        }

        [[nodiscard]] Device& device() noexcept { return cached_; }
        [[nodiscard]] const Device& device() const noexcept { return cached_; }

    private:
        fs::MalBlock mal_block_{};
        fs::CachedMal<MaxEntries> cache_{};
        Device cached_{};
    };
}
