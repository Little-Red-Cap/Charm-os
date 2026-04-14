# POSIX / ELF Stage Summary

## Current Baseline
- QEMU mainline smoke is green: `posix smoke + busybox phase2 smoke`
- ELF execution is on the regular path: `spawn -> load_image -> start_image`
- ELF explicit exit ABI v0 is live: `_exit(code)` works through `ExecContext` + `setjmp/longjmp`
- programs smoke is split into `posix.test_harness`, `exec`, `fdpath`, and `shell` modules
- `ExecContext` is now housed in `posix.exec_context`; `setjmp` remains at the `start_image()` call site so `longjmp` still lands on a live frame
- ELF hostcall dispatch now lives in `posix.elf_hostcall`; image catalog and source helpers are split into `posix.program_catalog` and `posix.exec_source`
- shared spawn/process types now live in `posix.proc_types`, and `posix.proc` re-exports them for existing callers
- ELF buffer/file/candidate loading now delegates through `posix.exec_loader`, leaving `posix.proc` closer to an orchestrator
- `load_image()` source selection now delegates through `posix.image_resolver`, so `posix.proc` is closer to a pure orchestrator
- child fd-table setup and file-actions application now delegate through `posix.spawn_fds`, so `posix.proc` keeps less spawn-specific wiring
- `stat_probe` is back on the mainline smoke and now validates the `fstat(-1) -> EBADF` path together with `file/pipe/term` minimum mode+size reporting
- shell smoke now resolves `/bin/*` through `PATH` in actual spawned flows, instead of bypassing it with exact-name registration
- busybox-style applet entry shapes are now smoke-covered: both `/bin/sh` via `argv[0]` and `busybox sh -c ...` via `argv[1]`
- FS Basics v1 is now on the mainline: `mkdir`, `unlink`, `rename`, `opendir/readdir`, and BusyBox-style `ls`
- freestanding C userland now keeps its minimal process/stdio surface in `charm_posix_user_crt.h`, while FS/FD/path/dir-facing contracts expand through the split header `Modules/io/posix/charm_posix_user_fs.h`
- BusyBox Phase 1 smoke now covers a minimal real flow: `mkdir -> ls / -> mv -> ls /work -> rm -> ls /work`
- redirect matrix v1 is now on the mainline shell smoke: `<`, `2>`, `2>&1`, and `>>`
- process-control slice now includes `kill v0`, `minimal ps`, shell/busybox `kill` applet coverage, and real-ELF `sleep/kill` hostcall coverage: `getpid`, `sleep`, the minimum `kill(SIGTERM/SIGINT/SIGKILL)` contract across proc/api/shell/busybox, and a minimum `ps(pid/state/name)` view are smoke-covered on the current same-address-space model
- spawned newlib smoke now also covers pure-fd `dup()` / `dup2()` / `fcntl(F_DUPFD)` plus minimal fd-flag control through `fcntl(F_GETFD/F_SETFD/F_GETFL/F_SETFL)`, `FD_CLOEXEC`, and `O_NONBLOCK`; a spawned cloexec regression now also proves non-inheritable fds are pruned across `spawn`
- spawned newlib `fcntl` smoke now covers pipe-backed descriptors, term-backed descriptors, and active fd-path aliases such as `/dev/stdin` / `/dev/stdout` / `/dev/stderr` / `/dev/tty` for `O_NONBLOCK`; it also proves that the stdio aliases keep following the current binding shape (for example, a remapped pipe still reports `S_IFIFO` and `isatty == 0`)

## Stable ABI Contracts

### ELF execution / process
- `explicit exit > return` is the active exit resolution rule
- `waitpid()` consumes the unified final exit result, not the internal exit mechanism
- `kill()` currently supports `SIGTERM` / `SIGKILL` / `SIGINT`; a killed process now reports `WaitKind::signaled` with the signal number as wait code
- ELF hostcalls use `ExecContext`; they no longer depend on the old global service/pid slot pair
- path-bearing real-ELF hostcalls must resolve against the spawned process cwd before touching `file_service_`; the raw exec-loader path remains a separate already-resolved input lane

### fd / errno contracts validated by smoke
- `open("/missing.txt") -> -1 && errno == ENOENT`
- `isatty(file/pipe) -> 0 && errno == 0`
- `isatty(non-tty) -> 0 && errno == 0`
- `fstat(-1, ...) -> -1 && errno == EBADF`
- `read(EOF) -> 0 && errno == 0`
- `read(-1, ...) -> -1 && errno == EBADF`
- `write(-1, ...) -> -1 && errno == EBADF`
- `close(-1) -> -1 && errno == EBADF`
- `pipe()` v0 currently behaves as an eager nonblocking primitive: empty reads with a live writer return `-1 && errno == EAGAIN`, full writes with a live reader return `-1 && errno == EAGAIN`, writer shutdown still yields `read() -> 0`, and per-endpoint `O_NONBLOCK` remains observable through `fcntl(F_GETFL/F_SETFL)` and shared across dup-family aliases on that endpoint
- term-backed stdio now also exposes minimal status-flag state through `fcntl(F_GETFL/F_SETFL)`; `stdin` / `stdout` / `stderr` keep separate open-file-description state, while `/dev/tty` and `/dev/stderr` aliases inherit and share the selected source descriptor's `O_NONBLOCK` bit
- `/dev/console` and `/dev/tty` currently share the same live-terminal selection rule: read opens prefer the active read-side terminal source, write opens prefer the active write-side terminal source, and both aliases share that source descriptor's observable status flags
- when no live terminal fd remains in the table, `open("/dev/console", ...)`, `open("/dev/tty", ...)`, `stat("/dev/console", ...)`, and `stat("/dev/tty", ...)` fail with `ENOENT`
- `dup(valid-fd) -> new-fd`, `dup2(old,new) -> new`, `fcntl(fd, F_DUPFD, min) -> lowest-available>=min`, `fcntl(fd, F_GETFD) -> {0|FD_CLOEXEC}`, `fcntl(fd, F_SETFD, flag) -> 0`, `fcntl(fd, F_GETFL) -> access-mode | status-flags`, `fcntl(fd, F_SETFL, flags) -> 0`, `fcntl(fd, F_SETFD/F_SETFL, invalid) -> -1 && errno == EINVAL`, `dup(-1) -> -1 && errno == EBADF`, `dup2(-1,new) -> -1 && errno == EBADF`, `fcntl(-1, F_DUPFD/F_GETFD/F_SETFD/F_GETFL/F_SETFL, ...) -> -1 && errno == EBADF`, `fcntl(fd, F_DUPFD, -1) -> -1 && errno == EINVAL`, `dup/full-table` style exhaustion returns `EMFILE`, dup-family-created descriptors always clear `FD_CLOEXEC` on the new fd, and duplicated descriptors share open-file status flags such as `O_APPEND` / `O_NONBLOCK` through the backend ctx
- `open("/dir", O_WRONLY) -> -1 && errno == EISDIR`
- `open("/file/child", O_RDONLY) -> -1 && errno == ENOTDIR`
- `open("/dev/stdin")` / `open("/dev/stdout")` / `open("/dev/stderr")` now alias the active stdio fd set, while `open("/dev/console", ...)` and `open("/dev/tty", ...)` alias a live terminal fd; `stat/fstat` on term-style descriptors stabilizes at `S_IFCHR`
- `mkdir("/work") -> 0`, repeated `mkdir("/work") -> -1 && errno == EEXIST`, and `mkdir("/file") -> -1 && errno == EEXIST`
- `unlink("/missing") -> -1 && errno == ENOENT`
- `mkdir("/file/sub")`, `unlink("/file/sub")`, `rmdir("/file/sub")`, and `stat("/file/sub", ...)` now consistently fail with `ENOTDIR`
- newlib path-facing probes now also confirm `access("/file/sub", F_OK) -> -1 && errno == ENOTDIR` on the same bad parent-path shape
- newlib `access()` now also keeps `errno` unchanged on success and rejects invalid mode bits with `EINVAL`
- `rename("/missing", "/target") -> -1 && errno == ENOENT`, `rename("/file/sub", "/target") -> -1 && errno == ENOTDIR`, and `rename("/source", "/file/sub") -> -1 && errno == ENOTDIR`
- `opendir("/path")` + `readdir()` now expose stable entry name/type/size basics for smoke coverage; `opendir(file)` fails with `ENOTDIR`, `opendir()` with the dir-handle pool exhausted fails with `EMFILE`, `opendir()` also fails with `ENOMEM` when the fixed dir-entry snapshot buffer overflows and with `ENAMETOOLONG` when a listed entry exceeds the exported dirent name buffer, `readdir()` on end-of-directory returns `nullptr` without clobbering `errno`, and `readdir()` / `closedir()` on a null, invalid, or already-closed directory handle fail with `EINVAL`
- `chdir("/missing") -> -1 && errno == ENOENT`, `chdir("/file") -> -1 && errno == ENOTDIR`, and `chdir("/file/sub") -> -1 && errno == ENOTDIR`
- newlib `remove()` now also has a pinned minimum path contract: `remove("/missing") -> ENOENT`, `remove("/file/sub") -> ENOTDIR`, and `remove("/nonempty-dir") -> ENOTEMPTY`

## Mainline Capabilities Already Holding
- file-backed ELF can be loaded and spawned from a path
- stdio/fd/pipe/wait form a working minimal userland spine
- fd duplication now has a stable minimum contract through `dup()`/`dup2()`/`fcntl()` across API smoke, bound-runtime bridge smoke, and spawned newlib smoke; descriptor close-on-exec now maps to `FdEntry.inheritable == false`, while `O_APPEND` / `O_NONBLOCK` now ride the shared open-file-description side via backend ctx hooks
- pipe endpoints now also have a documented v0 nonblocking contract: readiness is still eager rather than truly blocking, but user-visible `EAGAIN` / EOF behavior and `O_NONBLOCK` introspection are stable enough for the current syscall bridge and smoke surface
- term aliases now also have a stable minimum status-flag contract, and `/dev/tty` source selection now keys off `O_ACCMODE` so write-intended aliases prefer live write-capable terminal descriptors instead of accidentally selecting `stdin`
- `/dev/console` and `/dev/tty` now also have a pinned no-live-term boundary: once stdio has been fully rebound away from terminal-backed descriptors, both aliases fail with `ENOENT` for both `open()` and `stat()` instead of silently aliasing a non-terminal fd
- spawned newlib smoke now also covers direct `pipe()` use through the bridge, including empty-read `EAGAIN`, capacity-driven write-side `EAGAIN`, dup-shared `O_NONBLOCK`, and writer-close EOF on the read end
- busybox phase2 smoke remains usable on top of the current spine
- busybox phase1 minimal FS slice is now usable on top of the current spine
- child fd cleanup on exit is in place and no longer destabilizes pipe EOF behavior
- real-ELF smoke now isolates env/stderr subcases per harness, avoiding shared-fd cross contamination
- `spawnp`-style PATH lookup is now exercised from `posix.api` and program-shell smoke, not only proc smoke
- `spawnp` now also has an API-level `argv[0]` search-path regression, which is closer to Linux userland launcher shapes
- shell smoke now validates `cat < file`, `stderr_demo 2> err.txt`, `stderr_demo > both.txt 2>&1`, `echo ho >> out.txt`, and `stderr_demo >> both.txt 2>&1`
- API smoke now validates the minimum append contract directly with `O_APPEND`
- API smoke now validates bound-process `getpid()`, and real ELF smoke validates `getpid()` against the spawned pid value
- API smoke now validates `sleep(0/1)`, shell smoke validates `/bin/sleep` through `sh -c 'sleep 2'` with test-clock advancement, and real ELF smoke validates `elfmem:sleep 2`
- real ELF smoke now also validates `elfmem:kill_self` across `SIGTERM` / `SIGINT` / `SIGKILL`: user code enters, emits `before-kill`, then `waitpid()` reports the selected signal
- real ELF file-sample smoke now validates `cat_file` across both file-backed input and pipe-backed stdin, while keeping `isatty(stdout)=1`, `isatty(file/pipe)=0`, and `fstat()` on the active input fd stable
- real ELF file-sample smoke now also validates `write_file` across `open(O_WRONLY|O_CREAT|O_TRUNC) -> write -> close -> reopen-readback`, including host-side visibility of the final file content
- real ELF file-sample smoke now also validates that `write_file` resolves relative output paths against `SpawnConfig.cwd`, so userland file writes can target the current working directory rather than always falling back to `/`
- real ELF file-sample smoke now also validates `append_file` across `open(O_WRONLY|O_CREAT|O_APPEND) -> append -> close -> reopen-readback`, including preservation of the seeded prefix and host-side visibility of the final file content
- real ELF fd-probe smoke now pins a minimum fd-kind matrix across `stdin(term)`, redirected `stdout(pipe)`, and an opened file descriptor via `fstat()` + `isatty()`
- proc/api smoke now validate the minimum `kill v0` contract across `SIGTERM` / `SIGINT` / `SIGKILL`: kill-on-enter prevents target execution, `waitpid()` reports `signaled`, and API wait status encodes the delivered signal in the low bits
- shell smoke now validates `sh -c 'ps'`, and the current minimal view exposes `pid/state/name` for the live shell + child process set
- shell smoke now also validates `sh -c 'kill <pid>'`, `sh -c 'kill -INT <pid>'`, and `sh -c 'kill -KILL <pid>'` through `/bin/kill`, with the target remaining unentered and `waitpid()` still reporting the selected signal
- busybox direct-dispatch smoke now also validates `busybox ps`, `busybox sleep 2`, and `busybox kill {TERM|INT|KILL} <pid>`, so process applets are covered outside the shell wrapper path

## Isolated / Deferred Issues
- no current isolated smoke blocker; remaining work is focused on expanding semantics rather than restoring the mainline

## Toolchain Notes
- shared singleton-style module state is safer when it lives in one module object rather than a class-scope `inline static`; `charm.system.clock` now keeps the active time-source binding in module storage so real-ELF hostcalls and test harness code observe the same bound clock under the current GCC `modules-ts` toolchain
- QEMU smoke RAMFS fixtures should stay proportional to the sample they carry; oversized per-test RAMFS instances can look like a logic hang on the emulated target even when the POSIX path itself is correct
- `Examples/kernel/posix/qemu/run_qemu_ci.ps1` defaults to the full smoke contract; when validating partial targets such as `posix-qemu-newlib-stdio.elf`, pass `-RequireBusyboxPhase2:$false` so the runner only gates on the POSIX smoke marker
- GCC `-fmodules-ts` 下的 `net` / `posix` 导入冲突解阻经验已记录到 `docs/system/posix_modules_ts_build_notes.md`

## Recommended Next Cuts
- structural cleanup plan: `docs/system/posix_cleanup_refactor_plan.md`
1. Keep FS Basics v1 narrow and stable: harden `mkdir` / `unlink` / `rename` / `opendir` / `readdir` errno and path contracts
2. Continue the Phase 3 process-control slice after `minimal ps`: widen user-visible validation with a few more BusyBox/real-ELF cases, but keep process groups, sessions, and the wider signal model out of scope
3. Revisit wider `truncate` / `lseek` / path-error matrices only when a concrete BusyBox-style tool is blocked by them
4. Continue small, high-signal ELF samples only when they either harden an ABI contract or directly unblock Linux userland behavior
