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
    struct Mount;

    struct NodeOps {
        Status (*read)(Node&, std::span<util::u8>) noexcept { nullptr };
        Status (*write)(Node&, std::span<const util::u8>) noexcept { nullptr };
        Status (*seek)(Node&, util::i64) noexcept { nullptr };
        Status (*flush)(Node&) noexcept { nullptr };
        Status (*close)(Node&) noexcept { nullptr };
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
        Mount* mount{nullptr};
    };

    enum class OpenFlags : util::u32 {
        read = 1u << 0,
        write = 1u << 1,
        create = 1u << 2,
        trunc = 1u << 3,
    };

    [[nodiscard]] inline bool has_flag(OpenFlags value, OpenFlags flag) noexcept {
        return (static_cast<util::u32>(value) & static_cast<util::u32>(flag)) != 0u;
    }

    struct MountOps {
        Status (*open)(Mount*, std::string_view path, File&, OpenFlags flags) noexcept { nullptr };
        Status (*flush)(Mount*) noexcept { nullptr };
        Status (*unmount)(Mount*, bool force) noexcept { nullptr };
        Status (*unlink)(Mount*, std::string_view path) noexcept { nullptr };
        Status (*rename)(Mount*, std::string_view from, std::string_view to) noexcept { nullptr };
        Status (*truncate)(Mount*, std::string_view path, util::u64 size) noexcept { nullptr };
        Status (*mkdir)(Mount*, std::string_view path) noexcept { nullptr };
        struct ListEntry {
            std::string_view name{};
            NodeType type{NodeType::file};
            util::u64 size{0};
        };
        using ListFn = Status (*)(void* ctx, const ListEntry& entry) noexcept;
        Status (*list)(Mount*, std::string_view path, void* ctx, ListFn fn) noexcept { nullptr };
    };

    struct Mount {
        MountOps* ops{nullptr};
        void* data{nullptr};
        bool dirty{false};
    };

    inline void mark_dirty(Mount* m) noexcept {
        if (m) m->dirty = true;
    }

    inline void clear_dirty(Mount* m) noexcept {
        if (m) m->dirty = false;
    }

    [[nodiscard]] inline bool is_dirty(const Mount* m) noexcept {
        return m && m->dirty;
    }

    inline Status read(File& f, std::span<util::u8> buf) noexcept {
        if (!f.node.ops || !f.node.ops->read) return Status{Errc::nosys};
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
        if (!f.node.ops || !f.node.ops->write) return Status{Errc::nosys};
        const auto before = f.node.offset;
        auto st = f.node.ops->write(f.node, buf);
        if (st && f.node.offset >= 0) {
            if (f.node.offset == before) {
                f.node.offset += static_cast<util::i64>(buf.size());
            }
            if (f.node.offset > f.node.size) f.node.size = f.node.offset;
        }
        if (st) mark_dirty(f.mount);
        return st;
    }

    inline Status seek(File& f, util::i64 off) noexcept {
        if (!f.node.ops || !f.node.ops->seek) return Status{Errc::nosys};
        f.node.offset = off;
        return f.node.ops->seek(f.node, off);
    }

    inline Status flush(File& f) noexcept {
        if (!f.node.ops || !f.node.ops->flush) return Status{Errc::nosys};
        return f.node.ops->flush(f.node);
    }

    inline Status close(File& f) noexcept {
        if (!f.node.ops || !f.node.ops->close) return Status{Errc::nosys};
        auto st = f.node.ops->close(f.node);
        if (st) {
            f.node = {};
            f.mount = nullptr;
        }
        return st;
    }
}
