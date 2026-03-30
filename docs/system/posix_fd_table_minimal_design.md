# Minimal POSIX FD Table Design (Charm)

This document defines the minimal, fixed-capacity fd table and fd entry model
to support BusyBox phase 2/3 (shell, redirects, pipelines, process control).

## Goals

- Fixed-capacity fd table per process.
- Uniform fd entry interface for file, pipe, term, and special nodes.
- No dynamic allocation in the core path.
- Explicit error reporting for capacity and invalid usage.

## Core Types (Draft)

```cpp
namespace posix {
    enum class FdKind : util::u8 {
        file,
        pipe,
        term,
        dev,
        proc,
    };

    enum class FdFlags : util::u16 {
        none = 0,
        non_block = 1 << 0,
        read_only = 1 << 1,
        write_only = 1 << 2,
        read_write = 1 << 3,
    };

    struct FdOps {
        util::Result<util::usize> (*read)(void* ctx, io::MutByteView) noexcept;
        util::Result<util::usize> (*write)(void* ctx, io::ByteView) noexcept;
        util::Result<void> (*close)(void* ctx) noexcept;
        util::Result<void> (*stat)(void* ctx, PosixStat& out) noexcept;
    };

    struct FdEntry {
        int id{-1};
        FdKind kind{FdKind::file};
        FdFlags flags{FdFlags::none};
        const FdOps* ops{nullptr};
        void* ctx{nullptr};         // owned by subsystem (vfs/pipe/term)
        bool inheritable{true};
    };

    template <std::size_t MaxFds>
    class FdTable {
    public:
        util::Result<int> attach(FdEntry entry, int desired = -1) noexcept;
        util::Result<void> close(int fd) noexcept;
        util::Result<void> dup2(int from, int to) noexcept;
        util::Result<FdEntry*> get(int fd) noexcept;
        util::Result<FdEntry> clone_entry(int fd) noexcept; // for spawn
        void clear() noexcept;
    private:
        std::array<FdEntry, MaxFds> slots_{};
        std::bitset<MaxFds> used_{};
    };
}
```

## Behavior

- `attach` allocates an fd slot; if `desired` is occupied, returns `EMFILE`.
- `dup2` preserves semantics: if `to` exists, close then duplicate.
- `close` calls entry ops->close and releases slot.
- `clone_entry` copies references without transferring ownership.
- All entries are inheritable by default.

## Integration Points

- `posix.fd_table` depends on:
  - `fs.vfs` for file-backed entries
  - `posix.pipe` for pipe endpoints
  - `posix.term` for tty/stdio
- `posix.proc` uses `clone_entry` to inherit parent fds.

## Error Model

- invalid fd -> `util::Errc::noent`
- table full -> map to `EMFILE`
- global fd pool full -> map to `ENFILE`
- write on closed pipe -> map to `EPIPE`

## Notes

- Use `FdFlags` only for read/write policy and non-blocking hints.
- Backpressure and readiness should remain in the pipe/term subsystem.
- No `FD_CLOEXEC` in v1; use explicit `FileActions::add_close`.

