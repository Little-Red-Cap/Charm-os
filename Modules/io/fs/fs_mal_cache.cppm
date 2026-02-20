module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

export module fs_mal_cache;

import util.core;
import fs_errno;
import fs_stream;
import fs_mal;

export namespace fs {
    template <util::usize MaxEntries>
    class CachedMal {
    public:
        CachedMal() = default;
        CachedMal(const CachedMal&) = delete;
        CachedMal& operator=(const CachedMal&) = delete;

        Status bind(const MalDevice& dev, std::span<util::u8> cache_buf) noexcept {
            if (dev.block_size == 0 || dev.block_count == 0) return Status{Err::inval};
            if (!dev.ops.read || !dev.ops.write) return Status{Err::nosys};
            base_ = dev;
            const util::usize max_by_buf = cache_buf.size() / dev.block_size;
            entry_count_ = max_by_buf < MaxEntries ? max_by_buf : MaxEntries;
            if (entry_count_ == 0) return Status{Err::inval};
            cache_buf_ = cache_buf.subspan(0, entry_count_ * dev.block_size);
            clear();
            cached_ = dev;
            cached_.ctx = this;
            cached_.ops.read = &CachedMal::read_impl;
            cached_.ops.write = &CachedMal::write_impl;
            cached_.ops.erase = &CachedMal::erase_impl;
            cached_.ops.flush = &CachedMal::flush_impl;
            return Status{Err::ok};
        }

        void clear() noexcept {
            for (auto& e : entries_) {
                e.valid = false;
                e.stamp = 0;
                e.sector = 0;
            }
            stamp_ = 1;
        }

        [[nodiscard]] MalDevice& device() noexcept { return cached_; }
        [[nodiscard]] const MalDevice& device() const noexcept { return cached_; }
        [[nodiscard]] util::usize entry_count() const noexcept { return entry_count_; }

    private:
        struct Entry {
            util::u64 sector{0};
            util::u32 stamp{0};
            bool valid{false};
        };

        static Status read_impl(void* ctx, util::u64 lba, std::span<util::u8> out) noexcept {
            auto* self = static_cast<CachedMal*>(ctx);
            if (!self) return Status{Err::inval};
            if (self->base_.block_size == 0) return Status{Err::inval};
            if ((out.size() % self->base_.block_size) != 0) return Status{Err::inval};
            const util::u64 blocks = out.size() / self->base_.block_size;
            if (lba + blocks > self->base_.block_count) return Status{Err::inval};
            for (util::u64 i = 0; i < blocks; ++i) {
                auto st = self->read_sector(lba + i, out.subspan(i * self->base_.block_size,
                    self->base_.block_size));
                if (!st) return st;
            }
            return Status{Err::ok};
        }

        static Status write_impl(void* ctx, util::u64 lba, std::span<const util::u8> in) noexcept {
            auto* self = static_cast<CachedMal*>(ctx);
            if (!self) return Status{Err::inval};
            if (self->base_.block_size == 0) return Status{Err::inval};
            if ((in.size() % self->base_.block_size) != 0) return Status{Err::inval};
            const util::u64 blocks = in.size() / self->base_.block_size;
            if (lba + blocks > self->base_.block_count) return Status{Err::inval};
            for (util::u64 i = 0; i < blocks; ++i) {
                auto st = self->write_sector(lba + i, in.subspan(i * self->base_.block_size,
                    self->base_.block_size));
                if (!st) return st;
            }
            return Status{Err::ok};
        }

        static Status erase_impl(void* ctx, util::u64 lba, util::u64 count) noexcept {
            auto* self = static_cast<CachedMal*>(ctx);
            if (!self) return Status{Err::inval};
            if (!self->base_.ops.erase) return Status{Err::nosys};
            if (self->base_.block_size == 0) return Status{Err::inval};
            if (lba + count > self->base_.block_count) return Status{Err::inval};
            auto st = self->base_.ops.erase(self->base_.ctx, lba, count);
            if (st) self->invalidate_range(lba, count);
            return st;
        }

        static Status flush_impl(void* ctx) noexcept {
            auto* self = static_cast<CachedMal*>(ctx);
            if (!self) return Status{Err::inval};
            if (!self->base_.ops.flush) return Status{Err::nosys};
            return self->base_.ops.flush(self->base_.ctx);
        }

        Status read_sector(util::u64 lba, std::span<util::u8> out) noexcept {
            auto idx = find(lba);
            if (idx < entry_count_) {
                touch(idx);
                std::memcpy(out.data(), entry_data(idx), out.size());
                return Status{Err::ok};
            }
            auto st = base_.ops.read(base_.ctx, lba, out);
            if (!st) return st;
            idx = alloc();
            if (idx < entry_count_) {
                entries_[idx].sector = lba;
                entries_[idx].valid = true;
                touch(idx);
                std::memcpy(entry_data(idx), out.data(), out.size());
            }
            return Status{Err::ok};
        }

        Status write_sector(util::u64 lba, std::span<const util::u8> in) noexcept {
            auto st = base_.ops.write(base_.ctx, lba, in);
            if (!st) return st;
            auto idx = find(lba);
            if (idx >= entry_count_) {
                idx = alloc();
            }
            if (idx < entry_count_) {
                entries_[idx].sector = lba;
                entries_[idx].valid = true;
                touch(idx);
                std::memcpy(entry_data(idx), in.data(), in.size());
            }
            return Status{Err::ok};
        }

        util::usize find(util::u64 lba) noexcept {
            for (util::usize i = 0; i < entry_count_; ++i) {
                if (entries_[i].valid && entries_[i].sector == lba) return i;
            }
            return entry_count_;
        }

        util::usize alloc() noexcept {
            for (util::usize i = 0; i < entry_count_; ++i) {
                if (!entries_[i].valid) return i;
            }
            util::usize victim = 0;
            util::u32 best = entries_[0].stamp;
            for (util::usize i = 1; i < entry_count_; ++i) {
                if (entries_[i].stamp < best) {
                    best = entries_[i].stamp;
                    victim = i;
                }
            }
            return victim;
        }

        void touch(util::usize idx) noexcept {
            entries_[idx].stamp = stamp_++;
            if (stamp_ == 0) stamp_ = 1;
        }

        void invalidate_range(util::u64 lba, util::u64 count) noexcept {
            const auto end = lba + count;
            for (util::usize i = 0; i < entry_count_; ++i) {
                if (!entries_[i].valid) continue;
                const auto s = entries_[i].sector;
                if (s >= lba && s < end) entries_[i].valid = false;
            }
        }

        util::u8* entry_data(util::usize idx) noexcept {
            return cache_buf_.data() + idx * base_.block_size;
        }

        MalDevice base_{};
        MalDevice cached_{};
        std::span<util::u8> cache_buf_{};
        std::array<Entry, MaxEntries> entries_{};
        util::usize entry_count_{0};
        util::u32 stamp_{1};
    };
}
