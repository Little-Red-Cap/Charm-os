module;

#include <string_view>

#ifdef errno
#undef errno
#endif

export module posix.user_crt;

export import posix.errno;
export import posix.user_runtime;

import posix.exec_context;
import util.core;

export namespace posix::user {
    inline char** environ() noexcept {
        return posix::user::envp();
    }

    inline const char* getenv_cstr(std::string_view key) noexcept {
        if (key.empty()) {
            return nullptr;
        }
        auto** entries = posix::user::envp();
        if (!entries) {
            return nullptr;
        }
        for (util::usize index = 0; entries[index] != nullptr; ++index) {
            const char* entry = entries[index];
            std::string_view text{entry};
            const auto pos = text.find('=');
            if (pos == std::string_view::npos) {
                continue;
            }
            if (text.substr(0, pos) == key) {
                return entry + pos + 1;
            }
        }
        return nullptr;
    }

    inline int* errno_location() noexcept {
        if (auto* context = posix::active_exec_context()) {
            return &context->errno_value;
        }
        return &posix::errno_ref();
    }

    [[noreturn]] inline void _exit(int code) noexcept {
        if (auto* context = posix::active_exec_context()) {
            posix::request_exec_exit(*context, code);
        }
        for (;;) {}
    }

    [[noreturn]] inline void exit(int code) noexcept {
        _exit(code);
    }

    [[noreturn]] inline void abort() noexcept {
        _exit(134);
    }
}
