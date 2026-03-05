module;

#include <cstddef>
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
        const std::size_t n = (src.size() < (cap - 1)) ? src.size() : (cap - 1);
        for (std::size_t i = 0; i < n; ++i) dst[i] = src[i];
        dst[n] = '\0';
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
}
