module;

#include <array>
#include <span>

export module posix.proc_types;

import util.core;

export namespace posix {
    struct ProcessId { int value{-1}; };

    enum class PathMode : util::u8 {
        exact,
        search_path,
    };

    struct FileAction {
        enum class Kind { open, close, dup2 };
        Kind kind{};
        int fd{-1};
        int newfd{-1};
        const char* path{nullptr};
        int flags{0};
        int mode{0};
    };

    template <util::usize MaxActions>
    struct FileActions {
        std::array<FileAction, MaxActions> actions{};
        util::usize count{0};

        bool add_open(int fd, const char* path, int flags, int mode = 0) noexcept {
            if (count >= MaxActions) return false;
            actions[count++] = FileAction{FileAction::Kind::open, fd, -1, path, flags, mode};
            return true;
        }
        bool add_close(int fd) noexcept {
            if (count >= MaxActions) return false;
            actions[count++] = FileAction{FileAction::Kind::close, fd, -1, nullptr, 0, 0};
            return true;
        }
        bool add_dup2(int fd, int newfd) noexcept {
            if (count >= MaxActions) return false;
            actions[count++] = FileAction{FileAction::Kind::dup2, fd, newfd, nullptr, 0, 0};
            return true;
        }
    };

    struct SpawnConfig {
        const char* path{nullptr};
        std::span<const char* const> argv{};
        std::span<const char* const> envp{};
        const char* cwd{nullptr};
        const FileActions<16>* file_actions{nullptr};
        int stdio_in{-1};
        int stdio_out{-1};
        int stdio_err{-1};
        PathMode path_mode{PathMode::exact};
    };

    struct SpawnResult { ProcessId pid{}; };

    inline constexpr int SIGINT = 2;
    inline constexpr int SIGKILL = 9;
    inline constexpr int SIGTERM = 15;

    enum class WaitKind : util::u8 {
        exited,
        signaled,
        stopped,
        continued
    };

    struct WaitStatus {
        ProcessId pid{};
        WaitKind kind{WaitKind::exited};
        int code{0};
    };
}
