module;

#include <cstdarg>
#include <cstdint>
#include <cstdio>

export module player.runtime.hqzy_cm7.foundation;

import util.core;
import util.error;
import player.runtime.hqzy_cm7.board_platform;
import player.runtime.hqzy_cm7.boot_log;

export namespace player::foundation {
    struct Identity {
        const char* product{nullptr};
        const char* platform{nullptr};
        const char* board{nullptr};
        const char* scenario{nullptr};
    };

    struct Runtime {
        player::app_test_hqzy::board_platform::Context board{};
        Identity identity{};
    };

    inline void print(const Runtime&, const char* msg) noexcept {
        player::app_test_hqzy::boot_log::print(msg);
    }

    inline void printf(const Runtime&, const char* fmt, ...) noexcept {
        if (!fmt) return;
        char buf[160]{};
        va_list args;
        va_start(args, fmt);
        const int n = std::vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        if (n > 0) {
            player::app_test_hqzy::boot_log::print(buf);
        }
    }

    inline util::u64 now_ms(const Runtime&) noexcept {
        return player::app_test_hqzy::board_platform::now_ms(nullptr);
    }

    inline util::Result<Runtime> init(const Identity& identity) noexcept {
        auto board = player::app_test_hqzy::board_platform::init();
        if (!board) {
            return util::unexpected(board.error());
        }
        Runtime rt{};
        rt.board = *board;
        rt.identity = identity;
        player::app_test_hqzy::boot_log::printf(
            "foundation: product=%s platform=%s board=%s scenario=%s\n",
            identity.product ? identity.product : "?",
            identity.platform ? identity.platform : "?",
            identity.board ? identity.board : "?",
            identity.scenario ? identity.scenario : "?");
        return rt;
    }
}
