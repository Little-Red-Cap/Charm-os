module;

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string_view>

export module player.stm32h7.app_boot_fs;

import fs_vfs;
import player.stm32h7.board_console;

export namespace player::stm32h7::app::boot_fs {
    inline void early_uart_print_sv(std::string_view s) noexcept {
        char buf[64]{};
        std::size_t pos = 0;
        for (std::size_t i = 0; i < s.size(); ++i) {
            buf[pos++] = s[i];
            if (pos + 1 >= sizeof(buf)) {
                buf[pos] = '\0';
                board::early_uart_print(buf);
                pos = 0;
            }
        }
        if (pos > 0) {
            buf[pos] = '\0';
            board::early_uart_print(buf);
        }
    }

    struct RootDumpCtx {
        const char* prefix{nullptr};
        std::size_t count{0};
    };

    inline fs::Status dump_entry_early(void* p,
                                       const fs::MountOps::ListEntry& entry) noexcept {
        auto* out = static_cast<RootDumpCtx*>(p);
        if (!out || !out->prefix) return fs::Status{fs::Errc::inval};
        board::early_uart_print("fs: ");
        board::early_uart_print(out->prefix);
        board::early_uart_print(" ");
        early_uart_print_sv(entry.name);
        board::early_uart_print(" type=");
        char buf[16]{};
        const int n = std::snprintf(buf, sizeof(buf), "%d", entry.type);
        if (n > 0) board::early_uart_print(buf);
        board::early_uart_print("\n");
        ++out->count;
        return fs::Status{};
    }

    inline void dump_dir_early(const char* path) noexcept {
        RootDumpCtx ctx{path, 0};
        const auto st = fs::vfs_list(path, &ctx, dump_entry_early);
        if (!st) {
            board::early_uart_print("fs: list failed ");
            board::early_uart_print(path);
            board::early_uart_print(" err=");
            char buf[16]{};
            const int n = std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(st.error()));
            if (n > 0) board::early_uart_print(buf);
            board::early_uart_print("\n");
            return;
        }
        board::early_uart_print("fs: ");
        board::early_uart_print(path);
        board::early_uart_print(" entries=");
        char buf[16]{};
        const int n = std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(ctx.count));
        if (n > 0) board::early_uart_print(buf);
        board::early_uart_print("\n");
    }
}
