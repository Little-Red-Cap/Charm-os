module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

export module fs_blockfs;

import util.core;
import fs_core;
import fs_errno;
import fs_stream;
import fs_path;
import fs_block;

export namespace fs {
    template <util::usize BlockSize, util::usize MaxFiles, util::usize BlockCount>
    class BlockFs {
    public:
        struct DiskEntry {
            static constexpr util::usize name_cap = 12;
            std::array<char, name_cap> name_buf{};
            util::u32 block{0};
            util::u32 size{0};
            util::u16 parent{0};
            util::u8 used{0};
            util::u8 is_dir{0};
        };

        struct Entry {
            static constexpr util::usize name_cap = DiskEntry::name_cap;
            std::array<char, name_cap> name_buf{};
            util::u32 block{0};
            util::u32 size{0};
            util::u16 parent{0};
            util::u8 used{0};
            util::u8 is_dir{0};
            BlockFs* owner{nullptr};

            std::string_view name() const noexcept {
                const char* base = name_buf.data();
                util::usize len = 0;
                while (len < name_cap && base[len] != '\0') ++len;
                return std::string_view{base, len};
            }
        };

        struct Super {
            util::u32 magic{0x42465331}; // "BFS1"
            util::u32 block_size{BlockSize};
            util::u32 max_files{MaxFiles};
            util::u32 map_blocks{0};
            util::u32 data_start{0};
            util::u32 next_hint{0};
        };

        static constexpr util::u16 fat_free = 0;
        static constexpr util::u16 fat_end = 0xFFFF;
        static constexpr util::u16 fat_reserved = 0xFFFE;
        static constexpr util::u32 invalid_block = 0xFFFF'FFFFu;
        static constexpr util::usize map_bytes = BlockCount * sizeof(util::u16);
        static constexpr util::u32 map_blocks = static_cast<util::u32>((map_bytes + BlockSize - 1) / BlockSize);
        static constexpr util::u32 data_start = static_cast<util::u32>(1 + map_blocks);

        static constexpr util::usize meta_size = sizeof(Super) + sizeof(DiskEntry) * MaxFiles;
        static_assert(meta_size <= BlockSize, "BlockSize too small for metadata");

        explicit BlockFs(BlockDevice& dev) : dev_(&dev) {}

        Status mount() noexcept {
            if (!dev_ || dev_->block_size != BlockSize || dev_->block_count < data_start + 1) return Status{Errc::inval};
            std::array<util::u8, BlockSize> buf{};
            auto st = dev_->read(dev_->ctx, 0, std::span<util::u8>(buf.data(), buf.size()));
            if (!st) return st;
            const auto* sup = reinterpret_cast<const Super*>(buf.data());
            if (sup->magic != Super{}.magic || sup->block_size != BlockSize || sup->map_blocks != map_blocks) {
                return Status{Errc::noent};
            }
            super_ = *sup;
            const auto* ent = reinterpret_cast<const DiskEntry*>(buf.data() + sizeof(Super));
            for (util::usize i = 0; i < MaxFiles; ++i) {
                entries_[i].name_buf = ent[i].name_buf;
                entries_[i].block = ent[i].block;
                entries_[i].size = ent[i].size;
                entries_[i].parent = ent[i].parent;
                entries_[i].used = ent[i].used;
                entries_[i].is_dir = ent[i].is_dir;
                if (entries_[i].used) entries_[i].owner = this;
            }
            init_root();
            st = load_fat();
            if (!st) return st;
            dirty_ = false;
            mounted_ = true;
            return Status{Errc::ok};
        }

        Status format() noexcept {
            if (!dev_ || dev_->block_size != BlockSize || dev_->block_count < data_start + 1) return Status{Errc::inval};
            super_ = Super{};
            super_.map_blocks = map_blocks;
            super_.data_start = data_start;
            super_.next_hint = data_start;
            for (auto& e : entries_) e = Entry{};
            init_root();
            fat_.fill(fat_free);
            for (util::u32 i = 0; i < data_start && i < BlockCount; ++i) {
                fat_[i] = fat_reserved;
            }
            dirty_ = true;
            return flush_meta();
        }

        Status flush() noexcept {
            return flush_meta();
        }

        Status unmount(bool force) noexcept {
            if (!mounted_) return Status{Errc::ok};
            if (dirty_) {
                if (force) {
                    dirty_ = false;
                    mounted_ = false;
                    return Status{Errc::ok};
                }
                auto st = flush_meta();
                if (!st) return st;
            }
            mounted_ = false;
            return Status{Errc::ok};
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
            util::usize cur_idx = root_index;
            while (true) {
                auto [head, rest] = split_first(pv);
                if (head.size == 0) return Status{Errc::inval};
                const bool last = (rest.size == 0);
                auto* e = find_child(cur_idx, head);
                const bool existed = e != nullptr;
                if (!e) {
                    if (last && want_create) {
                        e = create_entry(cur_idx, head, false);
                        if (!e) return Status{Errc::nomem};
                    } else {
                        return Status{Errc::noent};
                    }
                }
                if (last) {
                    if (want_create && want_excl && existed) return Status{Errc::exist};
                    if (e->is_dir) return Status{Errc::inval};
                    if (want_trunc) {
                        auto st = truncate(path, 0);
                        if (!st) return st;
                    }
                    out.node = Node{};
                    out.node.type = NodeType::file;
                    out.node.ops = &node_ops;
                    out.node.data = e;
                    out.node.size = e->size;
                    out.node.offset = 0;
                    return Status{Errc::ok};
                }
                if (!e->is_dir) return Status{Errc::inval};
                cur_idx = static_cast<util::usize>(e - entries_.data());
                pv = rest;
            }
        }

        Status unlink(std::string_view path) noexcept {
            util::usize parent = root_index;
            PathView leaf{};
            auto st = resolve_parent(path, parent, leaf);
            if (!st) return st;
            auto* e = find_child(parent, leaf);
            if (!e || !e->used) return Status{Errc::noent};
            if (e->is_dir) {
                const util::usize idx = static_cast<util::usize>(e - entries_.data());
                for (const auto& child : entries_) {
                    if (child.used && child.parent == idx) return Status{Errc::busy};
                }
            }
            if (!e->is_dir) {
                free_chain(e->block);
            }
            *e = Entry{};
            e->owner = this;
            dirty_ = true;
            return Status{Errc::ok};
        }

        Status truncate(std::string_view path, util::u64 size) noexcept {
            util::usize parent = root_index;
            PathView leaf{};
            auto st = resolve_parent(path, parent, leaf);
            if (!st) return st;
            auto* e = find_child(parent, leaf);
            if (!e || !e->used) return Status{Errc::noent};
            if (e->is_dir) return Status{Errc::inval};
            const util::usize new_size = static_cast<util::usize>(size);
            const util::usize new_blocks = (new_size + BlockSize - 1) / BlockSize;
            if (new_blocks == 0) {
                free_chain(e->block);
                e->block = 0;
            } else {
                if (!ensure_blocks(e, new_blocks)) return Status{Errc::nomem};
                trim_chain(e->block, new_blocks);
            }
            e->size = static_cast<util::u32>(new_size);
            dirty_ = true;
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
            auto* e = find_child(parent_from, leaf_from);
            if (!e || !e->used) return Status{Errc::noent};
            if (find_child(parent_to, leaf_to)) return Status{Errc::busy};
            const util::usize n = (leaf_to.size < Entry::name_cap - 1) ? leaf_to.size : (Entry::name_cap - 1);
            std::memset(e->name_buf.data(), 0, Entry::name_cap);
            if (n > 0) {
                std::memcpy(e->name_buf.data(), leaf_to.data, n);
            }
            e->parent = static_cast<util::u16>(parent_to);
            dirty_ = true;
            return Status{Errc::ok};
        }

        static NodeOps node_ops;

    private:
        static constexpr util::usize root_index = 0;

        void init_root() noexcept {
            auto& root = entries_[root_index];
            root.used = 1;
            root.is_dir = 1;
            root.parent = root_index;
            root.name_buf.fill('\0');
            root.block = 0;
            root.size = 0;
            root.owner = this;
        }

        Entry* find_child(util::usize parent, PathView name) noexcept {
            std::string_view key{name.data, name.size};
            for (auto& e : entries_) {
                if (e.used && e.parent == parent && e.name() == key) return &e;
            }
            return nullptr;
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
                    if (!fe || !fe->is_dir) return Status{Errc::noent};
                    cur_idx = static_cast<util::usize>(fe - entries_.data());
                    dv = rest;
                }
            }
            parent_out = cur_idx;
            leaf_out = base;
            return Status{Errc::ok};
        }

        Entry* create_entry(util::usize parent, PathView name, bool dir) noexcept {
            for (auto& e : entries_) {
                if (!e.used) {
                    e = Entry{};
                    e.used = 1;
                    e.block = 0;
                    e.parent = static_cast<util::u16>(parent);
                    e.is_dir = dir ? 1 : 0;
                    e.owner = this;
                    const auto n = (name.size < Entry::name_cap - 1) ? name.size : (Entry::name_cap - 1);
                    std::memcpy(e.name_buf.data(), name.data, n);
                    e.name_buf[n] = '\0';
                    e.size = 0;
                    dirty_ = true;
                    return &e;
                }
            }
            return nullptr;
        }

        Status flush_meta() noexcept {
            if (!dirty_) return Status{Errc::ok};
            std::array<util::u8, BlockSize> buf{};
            std::memcpy(buf.data(), &super_, sizeof(Super));
            for (util::usize i = 0; i < MaxFiles; ++i) {
                DiskEntry de{};
                de.name_buf = entries_[i].name_buf;
                de.block = entries_[i].block;
                de.size = entries_[i].size;
                de.parent = entries_[i].parent;
                de.used = entries_[i].used;
                de.is_dir = entries_[i].is_dir;
                std::memcpy(buf.data() + sizeof(Super) + sizeof(DiskEntry) * i, &de, sizeof(DiskEntry));
            }
            auto st = dev_->write(dev_->ctx, 0, std::span<const util::u8>(buf.data(), buf.size()));
            if (!st) return st;
            st = flush_fat();
            if (st) dirty_ = false;
            return st;
        }

        Status flush_fat() noexcept {
            if (!dev_) return Status{Errc::noent};
            const util::u8* src = reinterpret_cast<const util::u8*>(fat_.data());
            util::usize remaining = map_bytes;
            util::u32 block = 1;
            while (remaining > 0) {
                const util::usize chunk = remaining > BlockSize ? BlockSize : remaining;
                std::array<util::u8, BlockSize> buf{};
                std::memcpy(buf.data(), src, chunk);
                auto st = dev_->write(dev_->ctx, block, std::span<const util::u8>(buf.data(), buf.size()));
                if (!st) return st;
                src += chunk;
                remaining -= chunk;
                ++block;
            }
            return Status{Errc::ok};
        }

        Status load_fat() noexcept {
            if (!dev_) return Status{Errc::noent};
            util::u8* dst = reinterpret_cast<util::u8*>(fat_.data());
            util::usize remaining = map_bytes;
            util::u32 block = 1;
            while (remaining > 0) {
                std::array<util::u8, BlockSize> buf{};
                auto st = dev_->read(dev_->ctx, block, std::span<util::u8>(buf.data(), buf.size()));
                if (!st) return st;
                const util::usize chunk = remaining > BlockSize ? BlockSize : remaining;
                std::memcpy(dst, buf.data(), chunk);
                dst += chunk;
                remaining -= chunk;
                ++block;
            }
            return Status{Errc::ok};
        }

        util::u32 alloc_block() noexcept {
            if (!dev_) return 0;
            util::u32 start = super_.next_hint;
            if (start < data_start || start >= BlockCount) start = data_start;
            for (util::u32 i = start; i < BlockCount; ++i) {
                if (fat_[i] == fat_free) {
                    fat_[i] = fat_end;
                    super_.next_hint = i + 1;
                    return i;
                }
            }
            for (util::u32 i = data_start; i < start; ++i) {
                if (fat_[i] == fat_free) {
                    fat_[i] = fat_end;
                    super_.next_hint = i + 1;
                    return i;
                }
            }
            return 0;
        }

        util::usize chain_length(util::u32 head) const noexcept {
            if (head == 0 || head >= BlockCount) return 0;
            util::usize count = 0;
            util::u32 cur = head;
            while (cur != 0 && cur < BlockCount && fat_[cur] != fat_free && fat_[cur] != fat_reserved) {
                ++count;
                if (fat_[cur] == fat_end) break;
                cur = fat_[cur];
            }
            return count;
        }

        util::u32 find_block(util::u32 head, util::usize index) const noexcept {
            if (head == 0 || head >= BlockCount) return invalid_block;
            util::u32 cur = head;
            for (util::usize i = 0; i < index; ++i) {
                if (cur == 0 || cur >= BlockCount) return invalid_block;
                const auto next = fat_[cur];
                if (next == fat_end) return invalid_block;
                if (next == fat_free || next == fat_reserved) return invalid_block;
                cur = next;
            }
            return cur;
        }

        bool ensure_blocks(Entry* e, util::usize blocks_needed) noexcept {
            if (!e) return false;
            util::usize have = chain_length(e->block);
            if (have >= blocks_needed) return true;
            util::u32 last = 0;
            if (have == 0) {
                const auto first = alloc_block();
                if (first == 0) return false;
                e->block = first;
                last = first;
                have = 1;
            } else {
                last = find_block(e->block, have - 1);
                if (last == invalid_block) return false;
            }
            while (have < blocks_needed) {
                const auto next = alloc_block();
                if (next == 0) return false;
                fat_[last] = next;
                fat_[next] = fat_end;
                last = next;
                ++have;
            }
            return true;
        }

        void free_chain(util::u32 head) noexcept {
            util::u32 cur = head;
            while (cur != 0 && cur < BlockCount && fat_[cur] != fat_free && fat_[cur] != fat_reserved) {
                const auto next = fat_[cur];
                fat_[cur] = fat_free;
                if (next == fat_end) break;
                cur = next;
            }
        }

        void trim_chain(util::u32 head, util::usize keep) noexcept {
            if (keep == 0) {
                free_chain(head);
                return;
            }
            util::u32 last = find_block(head, keep - 1);
            if (last == invalid_block) return;
            const auto next = fat_[last];
            fat_[last] = fat_end;
            if (next != fat_end && next != fat_free && next != fat_reserved) {
                free_chain(next);
            }
        }

        Status read_entry(Entry* e, Node& n, std::span<util::u8> buf) noexcept {
            if (!e || !e->used || !dev_) return Status{Errc::noent};
            if (e->is_dir) return Status{Errc::inval};
            const util::usize off = static_cast<util::usize>(n.offset < 0 ? 0 : n.offset);
            if (off >= e->size) {
                n.offset = static_cast<util::i64>(off);
                return Status{Errc::ok};
            }
            util::usize to_copy = buf.size();
            if (off + to_copy > e->size) to_copy = e->size - off;
            util::usize remaining = to_copy;
            util::usize out_pos = 0;
            util::u32 block_idx = find_block(e->block, off / BlockSize);
            util::usize block_off = off % BlockSize;
            std::array<util::u8, BlockSize> block{};
            while (remaining > 0) {
                if (block_idx == invalid_block) break;
                auto st = dev_->read(dev_->ctx, block_idx, std::span<util::u8>(block.data(), block.size()));
                if (!st) return st;
                const util::usize chunk = (block_off + remaining > BlockSize) ? (BlockSize - block_off) : remaining;
                std::memcpy(buf.data() + out_pos, block.data() + block_off, chunk);
                out_pos += chunk;
                remaining -= chunk;
                if (fat_[block_idx] == fat_end) break;
                block_idx = fat_[block_idx];
                block_off = 0;
            }
            n.offset = static_cast<util::i64>(off + to_copy);
            return Status{Errc::ok};
        }

        Status write_entry(Entry* e, Node& n, std::span<const util::u8> buf) noexcept {
            if (!e || !e->used || !dev_) return Status{Errc::noent};
            if (e->is_dir) return Status{Errc::inval};
            const util::usize off = static_cast<util::usize>(n.offset < 0 ? 0 : n.offset);
            const util::usize end = off + buf.size();
            const util::usize blocks_needed = (end + BlockSize - 1) / BlockSize;
            if (!ensure_blocks(e, blocks_needed)) return Status{Errc::nomem};
            std::array<util::u8, BlockSize> block{};
            util::usize remaining = buf.size();
            util::usize in_pos = 0;
            util::u32 block_idx = find_block(e->block, off / BlockSize);
            util::usize block_off = off % BlockSize;
            while (remaining > 0) {
                if (block_idx == invalid_block) return Status{Errc::io};
                (void)dev_->read(dev_->ctx, block_idx, std::span<util::u8>(block.data(), block.size()));
                const util::usize chunk = (block_off + remaining > BlockSize) ? (BlockSize - block_off) : remaining;
                std::memcpy(block.data() + block_off, buf.data() + in_pos, chunk);
                auto st = dev_->write(dev_->ctx, block_idx, std::span<const util::u8>(block.data(), block.size()));
                if (!st) return st;
                in_pos += chunk;
                remaining -= chunk;
                if (remaining > 0) {
                    if (fat_[block_idx] == fat_end) return Status{Errc::io};
                    block_idx = fat_[block_idx];
                }
                block_off = 0;
            }
            if (end > e->size) e->size = static_cast<util::u32>(end);
            n.size = e->size;
            n.offset = static_cast<util::i64>(end);
            dirty_ = true;
            return Status{Errc::ok};
        }

        BlockDevice* dev_{nullptr};
        Super super_{};
        std::array<Entry, MaxFiles> entries_{};
        std::array<util::u16, BlockCount> fat_{};
        bool dirty_{false};
        bool mounted_{false};
    };

    template <util::usize BlockSize, util::usize MaxFiles, util::usize BlockCount>
    NodeOps BlockFs<BlockSize, MaxFiles, BlockCount>::node_ops{
        .read = [](Node& n, std::span<util::u8> buf) noexcept {
            auto* e = static_cast<Entry*>(n.data);
            if (!e || !e->owner) return Status{Errc::noent};
            return e->owner->read_entry(e, n, buf);
        },
        .write = [](Node& n, std::span<const util::u8> buf) noexcept {
            auto* e = static_cast<Entry*>(n.data);
            if (!e || !e->owner) return Status{Errc::noent};
            return e->owner->write_entry(e, n, buf);
        },
        .seek = [](Node& n, util::i64 off) noexcept {
            n.offset = off;
            return Status{Errc::ok};
        },
        .flush = [](Node&) noexcept { return Status{Errc::ok}; },
        .close = [](Node&) noexcept { return Status{Errc::ok}; }
    };
}
