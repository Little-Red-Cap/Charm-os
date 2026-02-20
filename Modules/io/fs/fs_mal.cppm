module;

#include <cstddef>
#include <cstdint>
#include <span>

export module fs_mal;

import util.core;
import fs_errno;
import fs_stream;
import fs_block;

export namespace fs {
    enum class MalKind : util::u8 {
        block,
        flash,
        file,
    };

    struct MalOps {
        Status (*read)(void* ctx, util::u64 lba, std::span<util::u8>) noexcept { nullptr };
        Status (*write)(void* ctx, util::u64 lba, std::span<const util::u8>) noexcept { nullptr };
        Status (*erase)(void* ctx, util::u64 lba, util::u64 count) noexcept { nullptr };
        Status (*flush)(void* ctx) noexcept { nullptr };
    };

    struct MalDevice {
        void* ctx{nullptr};
        MalOps ops{};
        util::u64 block_size{0};
        util::u64 block_count{0};
        MalKind kind{MalKind::block};
    };

    inline MalDevice make_mal_from_block(const BlockDevice& dev,
                                         MalKind kind = MalKind::block) noexcept {
        MalDevice mal{};
        mal.ctx = dev.ctx;
        mal.ops.read = dev.read;
        mal.ops.write = dev.write;
        mal.ops.erase = dev.erase;
        mal.ops.flush = dev.flush;
        mal.block_size = dev.block_size;
        mal.block_count = dev.block_count;
        mal.kind = kind;
        return mal;
    }

    inline void mal_to_block(const MalDevice& mal, BlockDevice& out) noexcept {
        out.ctx = mal.ctx;
        out.read = mal.ops.read;
        out.write = mal.ops.write;
        out.erase = mal.ops.erase;
        out.flush = mal.ops.flush;
        out.block_size = mal.block_size;
        out.block_count = mal.block_count;
    }

    inline Status mal_read(MalDevice& dev, util::u64 lba, std::span<util::u8> out) noexcept {
        if (!dev.ops.read) return Status{Err::nosys};
        return dev.ops.read(dev.ctx, lba, out);
    }

    inline Status mal_write(MalDevice& dev, util::u64 lba, std::span<const util::u8> in) noexcept {
        if (!dev.ops.write) return Status{Err::nosys};
        return dev.ops.write(dev.ctx, lba, in);
    }

    inline Status mal_erase(MalDevice& dev, util::u64 lba, util::u64 count) noexcept {
        if (!dev.ops.erase) return Status{Err::nosys};
        return dev.ops.erase(dev.ctx, lba, count);
    }

    inline Status mal_flush(MalDevice& dev) noexcept {
        if (!dev.ops.flush) return Status{Err::nosys};
        return dev.ops.flush(dev.ctx);
    }
}
