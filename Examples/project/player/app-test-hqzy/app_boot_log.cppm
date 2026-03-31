module;

#include <cstdarg>
#include <cstdint>
#include <cstdio>

export module player.app_test_hqzy.boot_log;

import util.core;
import util.error;
import player.app_test_hqzy.platform_bootstrap;

export namespace player::app_test_hqzy::boot_log {
    inline void print(const char* msg) noexcept {
        platform_bootstrap::write_uart(msg);
    }

    inline void printf(const char* fmt, ...) noexcept {
        if (!fmt) return;
        char buf[128]{};
        va_list args;
        va_start(args, fmt);
        const int n = std::vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        if (n > 0) {
            platform_bootstrap::write_uart(buf);
        }
    }

    inline void print_err(const char* tag, util::Errc err) noexcept {
        char buf[80]{};
        const int n = std::snprintf(
            buf, sizeof(buf), "%s err=%d\n", tag, static_cast<int>(err));
        if (n > 0) {
            platform_bootstrap::write_uart(buf);
        }
    }
}
