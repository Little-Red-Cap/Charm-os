# Minimal POSIX Spawn Design (Charm)

This document defines the minimal, modern C++ interface for POSIX-compatible
process spawn, pipe, and redirection in Charm. It targets BusyBox phases 2/3
and avoids legacy POSIX API leakage into core modules.

## Scope and Principles

- Spawn is closer to `posix_spawn`, not `system`.
- No implicit `PATH` search unless explicitly requested.
- No `fork` baseline; `vfork` is optional and deferred.
- Fixed-capacity storage; failures are explicit and diagnosable.
- POSIX compatibility lives in a wrapper layer, not in core APIs.

## Core Interfaces (Draft)

```cpp
namespace posix {
    struct ProcessId { int value{-1}; };

    struct PipeEnds {
        int read_fd{-1};
        int write_fd{-1};
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

    template <std::size_t MaxActions>
    struct FileActions {
        std::array<FileAction, MaxActions> actions{};
        std::size_t count{0};

        bool add_open(int fd, const char* path, int flags, int mode = 0) noexcept;
        bool add_close(int fd) noexcept;
        bool add_dup2(int fd, int newfd) noexcept;
    };

    enum class PathMode {
        exact,
        search_path
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

    struct SpawnResult {
        ProcessId pid{};
    };

    enum class WaitKind {
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

    util::Result<PipeEnds> pipe_create() noexcept;
    util::Result<SpawnResult> spawn(const SpawnConfig& cfg) noexcept;
    util::Result<WaitStatus> waitpid(ProcessId pid, int options = 0) noexcept;
}
```

## Semantics and Ordering

- `path` is the executable path, not an arbitrary command string.
- `PathMode::search_path` enables PATH lookup using `argv[0]` or `path`.
- `cwd` applies once per spawn; no file-action `chdir` in v1.
- Relative executable paths and `FileAction::open.path` are interpreted against `cwd`; when `cwd` is unset, they inherit the parent's current working directory.
- Phase 2 uses `FileActions::add_close` to emulate close-on-exec; `FD_CLOEXEC` is deferred.
- Apply order:
  1) resolve executable
  2) create child process/task object
  3) inherit fd table
  4) apply `cwd`
  5) apply `stdio_in/out/err`
  6) apply `file_actions` (overrides stdio if conflict)
  7) install env
  8) start image

## File Action Constraints (v1)

Focus on stable redirection flags:

- `O_RDONLY`
- `O_WRONLY | O_CREAT | O_TRUNC`
- `O_WRONLY | O_CREAT | O_APPEND`

Other combinations are allowed but not guaranteed for BusyBox phase 2.

## Pipe and FD Inheritance

- All fds are inheritable by default.
- `FileActions::add_close` is the only v1 mechanism to prune fds.
- `FD_CLOEXEC` is deferred.
- Child fd table is closed on process exit to release pipe/file refs promptly.

Pipe semantics:

- All writers closed -> read returns EOF.
- All readers closed -> write returns `EPIPE` (or equivalent error).

## Capacity Guidance

- `FileActions<16>` is the recommended default.
- `FileActions<8>` is the minimum footprint option.

FD table failures must be explicit:

- `EMFILE`: per-process fd table full.
- `ENFILE`: global fd pool exhausted.
- `ENOSPC` (or domain Errc): pipe/proc pool exhausted.

## BusyBox Phase Mapping

Phase 2 (Shell, redirect, pipe):

- `open/close/read/write`
- `dup/dup2`
- `pipe`
- `spawn`
- `waitpid`
- `chdir/getcwd`
- `PATH` search
- `isatty`
- `fstat/stat`
- `errno`

Phase 3 (Process control):

- `kill` (SIGTERM/SIGKILL/SIGINT minimum)
- `sleep/usleep/nanosleep` via system clock
- `getpid`
- minimal `ps` (pid/state/name)

## Capability Nodes

- `posix.fd_table`: fd lifecycle, dup/close, inheritance
- `posix.pipe`: pipe endpoints and backpressure
- `posix.proc`: spawn/waitpid/process state
- `posix.env`: envp + PATH lookup
- `posix.term`: stdio binding + isatty

## Compatibility Layer Boundary

Core APIs return structured types (e.g. `WaitStatus`).
POSIX wrapper converts to traditional `int status` and errno.
