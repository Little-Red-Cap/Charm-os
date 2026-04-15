module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

export module fs_ramfs;

import util.core;
import fs_core;
import fs_errno;
import fs_stream;
import fs_path;

export namespace fs {
    template <util::usize BlockSize, util::usize MaxFiles, util::usize MaxDataBlocks>
    class RamFs {
    public:
        struct FileEntry {
            static constexpr util::usize max_name = 32;
            std::array<char, max_name> name_buf{};
            util::usize name_len{0};
            util::usize parent{0};
            util::usize start_block{0};
            util::usize size{0};
            util::usize blocks_used{1};
            bool used{false};
            bool is_dir{false};
            Node node{};
            RamFs* owner{nullptr};

            std::string_view name() const noexcept {
                return std::string_view{name_buf.data(), name_len};
            }
        };

        struct Block {
            std::array<std::byte, BlockSize> data{};
        };

        RamFs() {
            for (util::usize i = 0; i < MaxFiles; ++i) {
                files_[i].used = false;
            }
            for (util::usize i = 0; i < MaxDataBlocks; ++i) {
                block_used_[i] = false;
            }
            init_root();
        }

        Status open(std::string_view path, File& out, OpenFlags flags) noexcept {
            auto norm = normalize(path);
            auto trimmed = rstrip_seps(norm);
            PathView pv{trimmed.data, trimmed.size};
            if (pv.size == 0) return Status{Errc::inval};
            const bool want_write = has_flag(flags, OpenFlags::write);
            const bool want_create = has_flag(flags, OpenFlags::create);
            const bool want_trunc = has_flag(flags, OpenFlags::trunc);
            const bool want_excl = has_flag(flags, OpenFlags::excl);
            if (want_trunc && !want_write) return Status{Errc::perm};

            auto cur_idx = root_index;
            while (true) {
                auto [head, rest] = split_first(pv);
                if (head.size == 0) return Status{Errc::inval};
                const bool last = (rest.size == 0);
                auto* fe = find_child(cur_idx, head);
                if (!fe) {
                    if (last && want_create) {
                        fe = create_entry(cur_idx, head, false);
                        if (!fe) return Status{Errc::nomem};
                    } else {
                        return Status{Errc::noent};
                    }
                }
                if (last) {
                    if (want_create && want_excl && fe->used) return Status{Errc::exist};
                    if (fe->is_dir) return Status{Errc::isdir};
                    if (want_trunc) {
                        auto st = truncate(path, 0);
                        if (!st) return st;
                    }
                    out.node = fe->node;
                    out.node.data = fe;
                    out.node.ops = &node_ops;
                    out.node.offset = 0;
                    out.node.size = static_cast<util::i64>(fe->size);
                    return Status{Errc::ok};
                }
                if (!fe->is_dir) return Status{Errc::notdir};
                cur_idx = static_cast<util::usize>(fe - files_.data());
                pv = rest;
            }
        }

        Status mkdir(std::string_view path) noexcept {
            util::usize parent = root_index;
            PathView leaf{};
            auto st = resolve_parent(path, parent, leaf);
            if (!st) return st;
            auto* fe = find_child(parent, leaf);
            if (fe && fe->used) return Status{Errc::exist};
            if (!create_entry(parent, leaf, true)) return Status{Errc::nomem};
            return Status{Errc::ok};
        }

        Status unlink(std::string_view path) noexcept {
            util::usize parent = root_index;
            PathView leaf{};
            auto st = resolve_parent(path, parent, leaf);
            if (!st) return st;
            auto* fe = find_child(parent, leaf);
            if (!fe || !fe->used) return Status{Errc::noent};
            if (fe->is_dir) {
                const util::usize idx = static_cast<util::usize>(fe - files_.data());
                for (const auto& child : files_) {
                    if (child.used && child.parent == idx) return Status{Errc::busy};
                }
            } else if (fe->blocks_used > 0) {
                free_blocks(fe->start_block, fe->blocks_used);
            }
            *fe = FileEntry{};
            fe->owner = this;
            return Status{Errc::ok};
        }

        Status truncate(std::string_view path, util::u64 size) noexcept {
            util::usize parent = root_index;
            PathView leaf{};
            auto st = resolve_parent(path, parent, leaf);
            if (!st) return st;
            auto* fe = find_child(parent, leaf);
            if (!fe || !fe->used) return Status{Errc::noent};
            if (fe->is_dir) return Status{Errc::inval};
            const util::usize new_size = static_cast<util::usize>(size);
            util::usize new_blocks = (new_size + BlockSize - 1) / BlockSize;
            if (new_blocks == 0) new_blocks = 1;
            if (new_blocks < fe->blocks_used) {
                free_blocks(fe->start_block + new_blocks, fe->blocks_used - new_blocks);
                fe->blocks_used = new_blocks;
            } else if (new_blocks > fe->blocks_used) {
                const util::usize extra = new_blocks - fe->blocks_used;
                if (!alloc_blocks(fe->start_block + fe->blocks_used, extra)) {
                    return Status{Errc::nomem};
                }
                fe->blocks_used = new_blocks;
            }
            fe->size = new_size;
            fe->node.size = static_cast<util::i64>(fe->size);
            if (fe->node.offset > fe->node.size) fe->node.offset = fe->node.size;
            return Status{Errc::ok};
        }

        Status rename(std::string_view from, std::string_view to) noexcept {
            util::usize parent_from = root_index;
            PathView leaf_from{};
            auto st = resolve_parent(from, parent_from, leaf_from);
            if (!st) return st;
            util::usize parent_to = root_index;
            PathView leaf_to{};
            st = resolve_parent(to, parent_to, leaf_to);
            if (!st) return st;
            auto* fe = find_child(parent_from, leaf_from);
            if (!fe || !fe->used) return Status{Errc::noent};
            if (find_child(parent_to, leaf_to)) return Status{Errc::busy};
            const util::usize n = (leaf_to.size > FileEntry::max_name) ? FileEntry::max_name : leaf_to.size;
            fe->name_len = n;
            std::memset(fe->name_buf.data(), 0, FileEntry::max_name);
            if (n > 0) {
                std::memcpy(fe->name_buf.data(), leaf_to.data, n);
            }
            fe->parent = parent_to;
            return Status{Errc::ok};
        }

        Status list(std::string_view path, void* ctx, MountOps::ListFn fn) noexcept {
            if (!fn) return Status{Errc::inval};
            util::usize dir_idx = root_index;
            auto st = resolve_dir(path, dir_idx);
            if (!st) return st;
            for (const auto& f : files_) {
                if (!f.used || f.parent != dir_idx) continue;
                MountOps::ListEntry entry{};
                entry.name = f.name();
                entry.type = f.is_dir ? NodeType::dir : NodeType::file;
                entry.size = f.is_dir ? 0 : static_cast<util::u64>(f.size);
                st = fn(ctx, entry);
                if (!st) return st;
            }
            return Status{Errc::ok};
        }

        Status mkdir_mount(std::string_view path) noexcept {
            return mkdir(path);
        }

        Status read(Node& n, std::span<util::u8> buf) noexcept {
            auto* fe = static_cast<FileEntry*>(n.data);
            return read_impl(fe, n, buf);
        }

        Status write(Node& n, std::span<const util::u8> buf) noexcept {
            auto* fe = static_cast<FileEntry*>(n.data);
            return write_impl(fe, n, buf);
        }

        static NodeOps node_ops;

    private:
        static Status read_impl(FileEntry* fe, Node& n, std::span<util::u8> buf) noexcept {
            if (!fe || !fe->used || fe->owner == nullptr) return Status{Errc::noent};
            if (fe->is_dir) return Status{Errc::inval};
            return fe->owner->read_impl_instance(fe, n, buf);
        }

        static Status write_impl(FileEntry* fe, Node& n, std::span<const util::u8> buf) noexcept {
            if (!fe || !fe->used || fe->owner == nullptr) return Status{Errc::noent};
            if (fe->is_dir) return Status{Errc::inval};
            return fe->owner->write_impl_instance(fe, n, buf);
        }

        Status resolve_parent(std::string_view path, util::usize& parent_out, PathView& leaf_out) noexcept {
            auto norm = normalize(path);
            auto trimmed = rstrip_seps(norm);
            PathView pv{trimmed.data, trimmed.size};
            if (pv.size == 0) return Status{Errc::inval};
            auto [dir, base] = split_last(pv);
            if (base.size == 0) return Status{Errc::inval};
            util::usize cur_idx = root_index;
            if (dir.size > 0) {
                PathView dv{dir.data, dir.size};
                while (dv.size > 0) {
                    auto [head, rest] = split_first(dv);
                    if (head.size == 0) return Status{Errc::inval};
                    auto* fe = find_child(cur_idx, head);
                    if (!fe) return Status{Errc::noent};
                    if (!fe->is_dir) return Status{Errc::notdir};
                    cur_idx = static_cast<util::usize>(fe - files_.data());
                    dv = rest;
                }
            }
            parent_out = cur_idx;
            leaf_out = base;
            return Status{Errc::ok};
        }

        Status resolve_dir(std::string_view path, util::usize& dir_out) noexcept {
            auto norm = normalize(path);
            auto trimmed = rstrip_seps(norm);
            if (trimmed.size == 0) {
                dir_out = root_index;
                return Status{Errc::ok};
            }
            PathView pv{trimmed.data, trimmed.size};
            util::usize cur_idx = root_index;
            while (pv.size > 0) {
                auto [head, rest] = split_first(pv);
                if (head.size == 0) return Status{Errc::inval};
                auto* fe = find_child(cur_idx, head);
                if (!fe) return Status{Errc::noent};
                if (!fe->is_dir) return Status{Errc::notdir};
                cur_idx = static_cast<util::usize>(fe - files_.data());
                pv = rest;
            }
            dir_out = cur_idx;
            return Status{Errc::ok};
        }

        Status read_impl_instance(FileEntry* fe, Node& n, std::span<util::u8> buf) noexcept {
            const util::usize off = static_cast<util::usize>(n.offset < 0 ? 0 : n.offset);
            if (off >= fe->size) {
                buf = buf.first(0);
                n.offset = static_cast<util::i64>(off);
                return Status{Errc::ok};
            }
            util::usize to_copy = buf.size();
            if (off + to_copy > fe->size) to_copy = fe->size - off;
            util::usize blk_idx = fe->start_block + off / BlockSize;
            util::usize blk_off = off % BlockSize;
            util::usize remain = to_copy;
            util::usize copied = 0;
            while (remain > 0 && blk_idx < fe->start_block + fe->blocks_used) {
                auto* blk = block_ptr(blk_idx);
                if (!blk) return Status{Errc::io};
                const util::usize chunk = (blk_off + remain > BlockSize) ? (BlockSize - blk_off) : remain;
                std::memcpy(buf.data() + copied, blk->data.data() + blk_off, chunk);
                copied += chunk;
                remain -= chunk;
                blk_idx++;
                blk_off = 0;
            }
            buf = buf.first(copied);
            n.offset = static_cast<util::i64>(off + copied);
            return Status{Errc::ok};
        }

        Status write_impl_instance(FileEntry* fe, Node& n, std::span<const util::u8> buf) noexcept {
            const util::usize off = static_cast<util::usize>(n.offset < 0 ? 0 : n.offset);
            const util::usize end = off + buf.size();
            const util::usize blocks_needed = (end + BlockSize - 1) / BlockSize;
            if (blocks_needed > MaxDataBlocks) return Status{Errc::nomem};
            if (blocks_needed > fe->blocks_used) {
                const util::usize additional = blocks_needed - fe->blocks_used;
                if (!alloc_blocks(fe->start_block + fe->blocks_used, additional)) {
                    return Status{Errc::nomem};
                }
                fe->blocks_used = blocks_needed;
            }
            util::usize blk_idx = fe->start_block + off / BlockSize;
            util::usize blk_off = off % BlockSize;
            util::usize remain = buf.size();
            util::usize copied = 0;
            while (remain > 0 && blk_idx < fe->start_block + fe->blocks_used) {
                auto* blk = block_ptr(blk_idx);
                if (!blk) return Status{Errc::io};
                const util::usize chunk = (blk_off + remain > BlockSize) ? (BlockSize - blk_off) : remain;
                std::memcpy(blk->data.data() + blk_off, buf.data() + copied, chunk);
                copied += chunk;
                remain -= chunk;
                blk_idx++;
                blk_off = 0;
            }
            if (end > fe->size) fe->size = end;
            n.size = fe->size;
            n.offset = static_cast<util::i64>(end);
            return Status{Errc::ok};
        }

        void init_root() noexcept {
            auto& root = files_[root_index];
            root.used = true;
            root.is_dir = true;
            root.parent = root_index;
            root.name_len = 0;
            root.start_block = 0;
            root.blocks_used = 0;
            root.size = 0;
            root.node.type = NodeType::dir;
            root.node.ops = &node_ops;
            root.node.data = &root;
            root.owner = this;
        }

        FileEntry* find_child(util::usize parent, PathView name) noexcept {
            std::string_view key{name.data, name.size};
            for (auto& f : files_) {
                if (f.used && f.parent == parent && f.name() == key) {
                    return &f;
                }
            }
            return nullptr;
        }

        FileEntry* create_entry(util::usize parent, PathView name, bool dir) noexcept {
            auto* f = free_file();
            if (!f) return nullptr;
            util::usize blk = 0;
            if (!dir) {
                blk = alloc_block();
                if (blk == MaxDataBlocks) return nullptr;
            }
            f->used = true;
            f->parent = parent;
            f->name_len = (name.size > FileEntry::max_name) ? FileEntry::max_name : name.size;
            if (f->name_len > 0) {
                std::memcpy(f->name_buf.data(), name.data, f->name_len);
            }
            f->start_block = dir ? 0 : blk;
            f->size = 0;
            f->blocks_used = dir ? 0 : 1;
            f->is_dir = dir;
            f->node.type = dir ? NodeType::dir : NodeType::file;
            f->node.ops = &node_ops;
            f->node.data = f;
            f->owner = this;
            return f;
        }

        FileEntry* free_file() noexcept {
            for (util::usize i = 1; i < MaxFiles; ++i) {
                if (!files_[i].used) return &files_[i];
            }
            return nullptr;
        }

        util::usize alloc_block() noexcept {
            for (util::usize i = 0; i < MaxDataBlocks; ++i) {
                if (!block_used_[i]) {
                    block_used_[i] = true;
                    return i;
                }
            }
            return MaxDataBlocks;
        }

        bool alloc_blocks(util::usize start, util::usize count) noexcept {
            for (util::usize i = 0; i < count; ++i) {
                const auto idx = start + i;
                if (idx >= MaxDataBlocks || block_used_[idx]) {
                    return false;
                }
            }
            for (util::usize i = 0; i < count; ++i) {
                block_used_[start + i] = true;
            }
            return true;
        }

        void free_blocks(util::usize start, util::usize count) noexcept {
            for (util::usize i = 0; i < count; ++i) {
                const auto idx = start + i;
                if (idx < MaxDataBlocks) {
                    block_used_[idx] = false;
                }
            }
        }

        Block* block_ptr(util::usize idx) noexcept {
            if (idx >= MaxDataBlocks) return nullptr;
            return &blocks_[idx];
        }

        std::array<FileEntry, MaxFiles> files_{};
        std::array<Block, MaxDataBlocks> blocks_{};
        std::array<bool, MaxDataBlocks> block_used_{};

        static constexpr util::usize root_index = 0;
    };

    template <util::usize BlockSize, util::usize MaxFiles, util::usize MaxDataBlocks>
    NodeOps RamFs<BlockSize, MaxFiles, MaxDataBlocks>::node_ops{
        .read = [](Node& n, std::span<util::u8> buf) noexcept {
            auto* fe = static_cast<typename RamFs::FileEntry*>(n.data);
            if (!fe || !fe->used || fe->owner == nullptr) return Status{Errc::noent};
            if (fe->is_dir) return Status{Errc::inval};
            return fe->owner->read_impl_instance(fe, n, buf);
        },
        .write = [](Node& n, std::span<const util::u8> in) noexcept {
            auto* fe = static_cast<typename RamFs::FileEntry*>(n.data);
            if (!fe || !fe->used || fe->owner == nullptr) return Status{Errc::noent};
            if (fe->is_dir) return Status{Errc::inval};
            return fe->owner->write_impl_instance(fe, n, in);
        },
        .seek = [](Node& n, util::i64 off) noexcept {
            n.offset = off;
            return Status{Errc::ok};
        },
        .flush = [](Node&) noexcept { return Status{Errc::ok}; },
        .close = [](Node&) noexcept { return Status{Errc::ok}; }
    };
}
