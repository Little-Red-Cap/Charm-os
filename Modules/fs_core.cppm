module;

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <span>
#include <string_view>

export module fs_core;

import util.core;
import fs_errno;
import fs_stream;

export namespace fs {
    enum class NodeType : util::u8 { file, dir, device };

    struct Node;

    struct NodeOps {
        Status (*read)(Node&, std::span<util::u8>) noexcept { nullptr };
        Status (*write)(Node&, std::span<const util::u8>) noexcept { nullptr };
        Status (*seek)(Node&, util::i64) noexcept { nullptr };
        Status (*flush)(Node&) noexcept { nullptr };
    };

    struct Node {
        NodeType type{NodeType::file};
        NodeOps* ops{nullptr};
        void* data{nullptr};
        util::i64 size{0};
        util::i64 offset{0};
    };

    struct File {
        Node node{};
    };

    struct Mount;

    struct MountOps {
        Status (*open)(std::string_view path, File&) noexcept { nullptr };
        Status (*flush)(Mount*) noexcept { nullptr };
        Status (*unmount)(Mount*, bool force) noexcept { nullptr };
        Status (*unlink)(Mount*, std::string_view path) noexcept { nullptr };
        Status (*rename)(Mount*, std::string_view from, std::string_view to) noexcept { nullptr };
        Status (*truncate)(Mount*, std::string_view path, util::u64 size) noexcept { nullptr };
    };

    struct Mount {
        MountOps* ops{nullptr};
        void* data{nullptr};
    };

    inline Status read(File& f, std::span<util::u8> buf) noexcept {
        if (!f.node.ops || !f.node.ops->read) return Status{Err::nosys};
        const auto before = f.node.offset;
        auto st = f.node.ops->read(f.node, buf);
        if (st && f.node.offset >= 0) {
            if (f.node.offset == before) {
                f.node.offset += static_cast<util::i64>(buf.size());
            }
        }
        return st;
    }

    inline Status write(File& f, std::span<const util::u8> buf) noexcept {
        if (!f.node.ops || !f.node.ops->write) return Status{Err::nosys};
        const auto before = f.node.offset;
        auto st = f.node.ops->write(f.node, buf);
        if (st && f.node.offset >= 0) {
            if (f.node.offset == before) {
                f.node.offset += static_cast<util::i64>(buf.size());
            }
            if (f.node.offset > f.node.size) f.node.size = f.node.offset;
        }
        return st;
    }

    inline Status seek(File& f, util::i64 off) noexcept {
        if (!f.node.ops || !f.node.ops->seek) return Status{Err::nosys};
        f.node.offset = off;
        return f.node.ops->seek(f.node, off);
    }

    inline Status flush(File& f) noexcept {
        if (!f.node.ops || !f.node.ops->flush) return Status{Err::nosys};
        return f.node.ops->flush(f.node);
    }
}
