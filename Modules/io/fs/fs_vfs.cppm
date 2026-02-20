module;

#include <cstddef>
#include <cstdint>
#include <array>
#include <string_view>

export module fs_vfs;

import fs_core;
import fs_errno;
import fs_stream;
import fs_path;
import util.core;

export namespace fs {
    inline constexpr std::size_t max_mounts = 8;

    struct MountPoint {
        std::string_view prefix{};
        Mount* mount{nullptr};
    };

    inline std::array<MountPoint, max_mounts> g_mounts{};
    inline std::size_t g_mount_count{0};

    inline void clear_mounts() noexcept {
        g_mount_count = 0;
        for (auto& m : g_mounts) {
            m.prefix = {};
            m.mount = nullptr;
        }
    }

    inline Status add_mount(std::string_view prefix, Mount* m) noexcept {
        if (!m) return Status{Err::inval};
        if (g_mount_count >= max_mounts) return Status{Err::busy};
        auto norm = normalize(prefix);
        g_mounts[g_mount_count++] = MountPoint{std::string_view{norm.data, norm.size}, m};
        return Status{Err::ok};
    }

    inline Status remove_mount(std::string_view prefix) noexcept {
        auto norm = normalize(prefix);
        std::string_view pre{norm.data, norm.size};
        for (std::size_t i = 0; i < g_mount_count; ++i) {
            if (g_mounts[i].prefix == pre) {
                for (std::size_t j = i + 1; j < g_mount_count; ++j) {
                    g_mounts[j - 1] = g_mounts[j];
                }
                g_mounts[g_mount_count - 1] = MountPoint{};
                --g_mount_count;
                return Status{Err::ok};
            }
        }
        return Status{Err::noent};
    }

    inline Status vfs_flush(std::string_view prefix) noexcept {
        auto norm = normalize(prefix);
        std::string_view pre{norm.data, norm.size};
        for (std::size_t i = 0; i < g_mount_count; ++i) {
            if (g_mounts[i].prefix == pre) {
                auto* m = g_mounts[i].mount;
                if (!m || !m->ops || !m->ops->flush) return Status{Err::nosys};
                auto st = m->ops->flush(m);
                if (st) clear_dirty(m);
                return st;
            }
        }
        return Status{Err::noent};
    }

    inline bool vfs_is_dirty(std::string_view prefix) noexcept {
        auto norm = normalize(prefix);
        std::string_view pre{norm.data, norm.size};
        for (std::size_t i = 0; i < g_mount_count; ++i) {
            if (g_mounts[i].prefix == pre) {
                return is_dirty(g_mounts[i].mount);
            }
        }
        return false;
    }

    inline Status vfs_unmount(std::string_view prefix, bool force = false) noexcept {
        auto norm = normalize(prefix);
        std::string_view pre{norm.data, norm.size};
        for (std::size_t i = 0; i < g_mount_count; ++i) {
            if (g_mounts[i].prefix == pre) {
                auto* m = g_mounts[i].mount;
                if (m && m->ops && m->ops->unmount) {
                    auto st = m->ops->unmount(m, force);
                    if (!st) return st;
                }
                for (std::size_t j = i + 1; j < g_mount_count; ++j) {
                    g_mounts[j - 1] = g_mounts[j];
                }
                g_mounts[g_mount_count - 1] = MountPoint{};
                --g_mount_count;
                return Status{Err::ok};
            }
        }
        return Status{Err::noent};
    }

    inline std::size_t mount_count() noexcept { return g_mount_count; }

    // 兼容旧接口：仅设置单一根挂载
    inline void set_mount(Mount* m) noexcept {
        clear_mounts();
        (void)add_mount("/", m);
    }

    inline Mount* find_mount(std::string_view path, std::string_view& out_prefix) noexcept {
        auto norm = normalize(path);
        std::string_view p{norm.data, norm.size};
        Mount* chosen = nullptr;
        std::string_view chosen_prefix{};
        for (std::size_t i = 0; i < g_mount_count; ++i) {
            const auto& mp = g_mounts[i];
            if (!mp.mount) continue;
            const auto& pre = mp.prefix;
            if (pre.empty()) {
                if (!chosen || chosen_prefix.size() == 0) {
                    chosen = mp.mount;
                    chosen_prefix = pre;
                }
            } else if (p.size() >= pre.size() && p.substr(0, pre.size()) == pre) {
                if (pre.size() > chosen_prefix.size()) {
                    chosen = mp.mount;
                    chosen_prefix = pre;
                }
            }
        }

        out_prefix = chosen_prefix;
        return chosen;
    }

    inline Status vfs_open(std::string_view path, File& f, OpenFlags flags) noexcept {
        std::string_view prefix{};
        auto* chosen = find_mount(path, prefix);
        if (!chosen || !chosen->ops || !chosen->ops->open) return Status{Err::nosys};

        auto norm = normalize(path);
        std::string_view p{norm.data, norm.size};
        std::string_view rest = p.substr(prefix.size());
        auto rest_norm = normalize(rest);
        std::string_view rest_view{rest_norm.data, rest_norm.size};
        auto st = chosen->ops->open(chosen, rest_view, f, flags);
        if (st) {
            f.mount = chosen;
        }
        return st;
    }

    inline Status vfs_open(std::string_view path, File& f) noexcept {
        return vfs_open(path, f, OpenFlags::read);
    }

    inline Status vfs_close(File& f) noexcept {
        return close(f);
    }

    inline Status vfs_read(File& f, std::span<util::u8> buf) noexcept {
        return read(f, buf);
    }

    inline Status vfs_write(File& f, std::span<const util::u8> buf) noexcept {
        return write(f, buf);
    }

    inline Status vfs_seek(File& f, util::i64 off) noexcept {
        return seek(f, off);
    }

    inline Status vfs_flush(File& f) noexcept {
        return flush(f);
    }

    inline Status vfs_unlink(std::string_view path) noexcept {
        std::string_view prefix{};
        auto* chosen = find_mount(path, prefix);
        if (!chosen || !chosen->ops || !chosen->ops->unlink) return Status{Err::nosys};
        auto norm = normalize(path);
        std::string_view p{norm.data, norm.size};
        std::string_view rest = p.substr(prefix.size());
        auto rest_norm = normalize(rest);
        std::string_view rest_view{rest_norm.data, rest_norm.size};
        auto st = chosen->ops->unlink(chosen, rest_view);
        if (st) mark_dirty(chosen);
        return st;
    }

    inline Status vfs_truncate(std::string_view path, util::u64 size) noexcept {
        std::string_view prefix{};
        auto* chosen = find_mount(path, prefix);
        if (!chosen || !chosen->ops || !chosen->ops->truncate) return Status{Err::nosys};
        auto norm = normalize(path);
        std::string_view p{norm.data, norm.size};
        std::string_view rest = p.substr(prefix.size());
        auto rest_norm = normalize(rest);
        std::string_view rest_view{rest_norm.data, rest_norm.size};
        auto st = chosen->ops->truncate(chosen, rest_view, size);
        if (st) mark_dirty(chosen);
        return st;
    }

    inline Status vfs_mkdir(std::string_view path) noexcept {
        std::string_view prefix{};
        auto* chosen = find_mount(path, prefix);
        if (!chosen || !chosen->ops || !chosen->ops->mkdir) return Status{Err::nosys};
        auto norm = normalize(path);
        std::string_view p{norm.data, norm.size};
        std::string_view rest = p.substr(prefix.size());
        auto rest_norm = normalize(rest);
        std::string_view rest_view{rest_norm.data, rest_norm.size};
        auto st = chosen->ops->mkdir(chosen, rest_view);
        if (st) mark_dirty(chosen);
        return st;
    }

    inline Status vfs_rename(std::string_view from, std::string_view to) noexcept {
        std::string_view prefix_from{};
        std::string_view prefix_to{};
        auto* src = find_mount(from, prefix_from);
        auto* dst = find_mount(to, prefix_to);
        if (!src || !dst) return Status{Err::noent};
        if (src != dst) return Status{Err::notsup};
        if (!src->ops || !src->ops->rename) return Status{Err::nosys};
        auto norm_from = normalize(from);
        std::string_view pfrom{norm_from.data, norm_from.size};
        std::string_view rest_from = pfrom.substr(prefix_from.size());
        auto rest_from_norm = normalize(rest_from);
        std::string_view rest_from_view{rest_from_norm.data, rest_from_norm.size};
        auto norm_to = normalize(to);
        std::string_view pto{norm_to.data, norm_to.size};
        std::string_view rest_to = pto.substr(prefix_to.size());
        auto rest_to_norm = normalize(rest_to);
        std::string_view rest_to_view{rest_to_norm.data, rest_to_norm.size};
        auto st = src->ops->rename(src, rest_from_view, rest_to_view);
        if (st) mark_dirty(src);
        return st;
    }

    inline Status vfs_list(std::string_view path, void* ctx, MountOps::ListFn fn) noexcept {
        if (!fn) return Status{Err::inval};
        std::string_view prefix{};
        auto* chosen = find_mount(path, prefix);
        if (!chosen || !chosen->ops || !chosen->ops->list) return Status{Err::nosys};
        auto norm = normalize(path);
        std::string_view p{norm.data, norm.size};
        std::string_view rest = p.substr(prefix.size());
        auto rest_norm = normalize(rest);
        std::string_view rest_view{rest_norm.data, rest_norm.size};
        return chosen->ops->list(chosen, rest_view, ctx, fn);
    }
}
