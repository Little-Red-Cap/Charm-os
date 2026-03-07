module;

#include <cstddef>
#include <cstring>
#include <string_view>

export module player.hqzy.fs_utils;

import fs_core;
import fs_errno;
import fs_stream;
import fs_vfs;
import player.hqzy.app_state;
import util.core;

namespace {
    bool is_dot_entry(std::string_view name) noexcept {
        return name == "." || name == "..";
    }

    void copy_name(char* dst, std::size_t cap, std::string_view src) noexcept {
        if (!dst || cap == 0) return;
        const char* ptr = src.data();
        if (!ptr) {
            dst[0] = '\0';
            return;
        }
        const std::size_t src_len = src.size() > 0 ? src.size() : std::strlen(ptr);
        const std::size_t n = (src_len < (cap - 1)) ? src_len : (cap - 1);
        for (std::size_t i = 0; i < n; ++i) dst[i] = ptr[i];
        dst[n] = '\0';
    }

    bool join_path(char* dst, std::size_t cap,
                   std::string_view dir,
                   std::string_view name) noexcept {
        if (!dst || cap == 0) return false;
        if (name.empty()) return false;
        if (dir.empty()) dir = "/";
        const bool root = (dir == "/");
        const std::size_t need = dir.size() + (root ? 0 : 1) + name.size();
        if (need >= cap) return false;
        std::size_t pos = 0;
        for (char c : dir) dst[pos++] = c;
        if (!root) dst[pos++] = '/';
        for (char c : name) dst[pos++] = c;
        dst[pos] = '\0';
        return true;
    }

    struct ListCtx {
        player::hqzy::AppState* state{nullptr};
        unsigned char count{0};
    };

    fs::Status on_list(void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
        auto* self = static_cast<ListCtx*>(ctx);
        if (!self || !self->state) return fs::Status{fs::Errc::inval};
        if (is_dot_entry(entry.name)) return fs::Status{fs::Errc::ok};
        if (self->count >= (sizeof(self->state->entries) / sizeof(self->state->entries[0]))) {
            return fs::Status{fs::Errc::ok};
        }
        auto& e = self->state->entries[self->count++];
        copy_name(e.name, sizeof(e.name), entry.name);
        e.is_dir = (entry.type == fs::NodeType::dir);
        return fs::Status{fs::Errc::ok};
    }
}

export namespace player::hqzy {
    fs::Status scan_dir(AppState& state, const char* dir) noexcept {
        state.entry_count = 0;
        state.entry_selected = 0;
        state.list_ready = false;
        state.list_error = false;
        if (!dir || !*dir) {
            state.list_error = true;
            return fs::Status{fs::Errc::inval};
        }
        copy_name(state.list_dir, sizeof(state.list_dir), dir);
        ListCtx ctx{&state, 0};
        auto st = fs::vfs_list(std::string_view{dir}, &ctx, &on_list);
        state.entry_count = ctx.count;
        state.list_ready = static_cast<bool>(st);
        state.list_error = !static_cast<bool>(st);
        return st;
    }

    fs::Status open_selected(AppState& state) noexcept {
        if (!state.list_ready) return fs::Status{fs::Errc::busy};
        if (state.entry_selected >= state.entry_count) {
            return fs::Status{fs::Errc::noent};
        }
        const auto& e = state.entries[state.entry_selected];
        char next[sizeof(state.play_path)]{};
        if (!join_path(next, sizeof(next), state.list_dir, e.name)) {
            state.list_error = true;
            return fs::Status{fs::Errc::nametoolong};
        }
        if (e.is_dir) {
            return scan_dir(state, next);
        }
        copy_name(state.play_path, sizeof(state.play_path), next);
        state.track.title = state.entries[state.entry_selected].name;
        state.playing = true;
        state.paused = false;
        state.play_request = true;
        state.stop_request = false;
        state.page = Page::now_playing;
        return fs::Status{fs::Errc::ok};
    }
}
