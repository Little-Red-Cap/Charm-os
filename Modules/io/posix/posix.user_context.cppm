module;

#include <array>
#include <cstddef>
#include <string_view>

export module posix.user_context;

import util.core;

namespace posix::user::detail {
    inline constexpr util::usize kStartupContextStackDepth = 32;
}

export namespace posix::user {
    struct StartupContext {
        int argc{0};
        char** argv{nullptr};
        char** envp{nullptr};
    };

    void bind_startup_context(int argc, char** argv, char** envp) noexcept;
    void unbind_startup_context() noexcept;
    const StartupContext* active_startup_context() noexcept;
    bool has_startup_context() noexcept;

    int argc() noexcept;
    char** argv() noexcept;
    char** envp() noexcept;
    const char* argv0() noexcept;
    const char* arg(util::usize index) noexcept;
    const char* env_entry(util::usize index) noexcept;
    std::string_view getenv(std::string_view key) noexcept;
}

namespace posix::user::detail {
#if defined(__arm__) || defined(__thumb__)
    inline StartupContext g_active_startup_context{};
    inline bool g_has_active_startup_context{false};
    inline std::array<StartupContext, kStartupContextStackDepth> g_startup_context_stack{};
    inline util::usize g_startup_context_depth{0};
#else
    inline thread_local StartupContext g_active_startup_context{};
    inline thread_local bool g_has_active_startup_context{false};
    inline thread_local std::array<StartupContext, kStartupContextStackDepth> g_startup_context_stack{};
    inline thread_local util::usize g_startup_context_depth{0};
#endif
}

export namespace posix::user {
    inline void bind_startup_context(int argc_value, char** argv_value, char** envp_value) noexcept {
        if (detail::g_has_active_startup_context &&
            detail::g_startup_context_depth < detail::g_startup_context_stack.size()) {
            detail::g_startup_context_stack[detail::g_startup_context_depth++] =
                detail::g_active_startup_context;
        }
        detail::g_active_startup_context = StartupContext{argc_value, argv_value, envp_value};
        detail::g_has_active_startup_context = true;
    }

    inline void unbind_startup_context() noexcept {
        if (detail::g_startup_context_depth > 0) {
            detail::g_active_startup_context =
                detail::g_startup_context_stack[--detail::g_startup_context_depth];
            detail::g_has_active_startup_context = true;
            return;
        }
        detail::g_active_startup_context = {};
        detail::g_has_active_startup_context = false;
    }

    inline const StartupContext* active_startup_context() noexcept {
        return detail::g_has_active_startup_context ? &detail::g_active_startup_context : nullptr;
    }

    inline bool has_startup_context() noexcept {
        return active_startup_context() != nullptr;
    }

    inline int argc() noexcept {
        const auto* context = active_startup_context();
        return context ? context->argc : 0;
    }

    inline char** argv() noexcept {
        const auto* context = active_startup_context();
        return context ? context->argv : nullptr;
    }

    inline char** envp() noexcept {
        const auto* context = active_startup_context();
        return context ? context->envp : nullptr;
    }

    inline const char* argv0() noexcept {
        return arg(0);
    }

    inline const char* arg(util::usize index) noexcept {
        const auto* context = active_startup_context();
        if (!context || index >= static_cast<util::usize>(context->argc) || !context->argv) {
            return nullptr;
        }
        return context->argv[index];
    }

    inline const char* env_entry(util::usize index) noexcept {
        auto** entries = envp();
        if (!entries) {
            return nullptr;
        }
        for (util::usize i = 0;; ++i) {
            if (!entries[i]) {
                return nullptr;
            }
            if (i == index) {
                return entries[i];
            }
        }
    }

    inline std::string_view getenv(std::string_view key) noexcept {
        if (key.empty()) {
            return {};
        }
        auto** entries = envp();
        if (!entries) {
            return {};
        }
        for (util::usize i = 0; entries[i] != nullptr; ++i) {
            std::string_view entry{entries[i]};
            const auto pos = entry.find('=');
            if (pos == std::string_view::npos) {
                continue;
            }
            if (entry.substr(0, pos) == key) {
                return entry.substr(pos + 1);
            }
        }
        return {};
    }
}
