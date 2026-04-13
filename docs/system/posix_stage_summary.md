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
- `stat_probe` is back on the mainline smoke and now validates the `fstat(-1) -> EBADF` path together with file-size reporting
- shell smoke now resolves `/bin/*` through `PATH` in actual spawned flows, instead of bypassing it with exact-name registration
- busybox-style applet entry shapes are now smoke-covered: both `/bin/sh` via `argv[0]` and `busybox sh -c ...` via `argv[1]`
- FS Basics v1 is now on the mainline: `mkdir`, `unlink`, `rename`, `opendir/readdir`, and BusyBox-style `ls`
- BusyBox Phase 1 smoke now covers a minimal real flow: `mkdir -> ls / -> mv -> ls /work -> rm -> ls /work`
- redirect matrix v1 is now on the mainline shell smoke: `<`, `2>`, `2>&1`, and `>>`
- process-control slice now includes `kill v0`, `minimal ps`, shell/busybox `kill` applet coverage, and real-ELF `sleep/kill` hostcall coverage: `getpid`, `sleep`, `kill(SIGTERM/SIGKILL/SIGINT)`, and a minimum `ps(pid/state/name)` view are smoke-covered on the current same-address-space model

## Stable ABI Contracts

### ELF execution / process
- `explicit exit > return` is the active exit resolution rule
- `waitpid()` consumes the unified final exit result, not the internal exit mechanism
- `kill()` currently supports `SIGTERM` / `SIGKILL` / `SIGINT`; a killed process now reports `WaitKind::signaled` with the signal number as wait code
- ELF hostcalls use `ExecContext`; they no longer depend on the old global service/pid slot pair

### fd / errno contracts validated by smoke
- `open("/missing.txt") -> -1 && errno == ENOENT`
- `isatty(file/pipe) -> 0 && errno == 0`
- `isatty(non-tty) -> 0 && errno == 0`
- `fstat(-1, ...) -> -1 && errno == EBADF`
- `read(EOF) -> 0 && errno == 0`
- `read(-1, ...) -> -1 && errno == EBADF`
- `write(-1, ...) -> -1 && errno == EBADF`
- `close(-1) -> -1 && errno == EBADF`
- `open("/dir", O_WRONLY) -> -1 && errno == EISDIR`
- `open("/file/child", O_RDONLY) -> -1 && errno == ENOTDIR`
- `open("/dev/console", ...)` now aliases a live terminal fd, and `stat/fstat` on term-style descriptors stabilizes at `S_IFCHR`
- `mkdir("/work") -> 0`, duplicate create returns `EEXIST`
- `unlink("/missing") -> -1 && errno == ENOENT`
- `opendir("/path")` + `readdir()` now expose stable entry name/type/size basics for smoke coverage

## Mainline Capabilities Already Holding
- file-backed ELF can be loaded and spawned from a path
- stdio/fd/pipe/wait form a working minimal userland spine
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
- real ELF smoke now also validates `elfmem:kill_self`: user code enters, emits `before-kill`, then `waitpid()` reports `signaled(SIGTERM)`
- proc/api smoke now validate the minimum `kill v0` contract: kill-on-enter prevents target execution, `waitpid()` reports `signaled`, and API wait status encodes `SIGTERM` in the low bits
- shell smoke now validates `sh -c 'ps'`, and the current minimal view exposes `pid/state/name` for the live shell + child process set
- shell smoke now also validates `sh -c 'kill <pid>'` through `/bin/kill`, with the target remaining unentered and `waitpid()` still reporting `signaled(SIGTERM)`
- busybox direct-dispatch smoke now also validates `busybox ps`, `busybox sleep 2`, and `busybox kill <pid>`, so process applets are covered outside the shell wrapper path

## Isolated / Deferred Issues
- no current isolated smoke blocker; remaining work is focused on expanding semantics rather than restoring the mainline

## Recommended Next Cuts
- structural cleanup plan: `docs/system/posix_cleanup_refactor_plan.md`
1. Keep FS Basics v1 narrow and stable: harden `mkdir` / `unlink` / `rename` / `opendir` / `readdir` errno and path contracts
2. Continue the Phase 3 process-control slice after `minimal ps`: widen user-visible validation with a few more BusyBox/real-ELF cases, but keep process groups, sessions, and the wider signal model out of scope
3. Revisit wider `truncate` / `lseek` / path-error matrices only when a concrete BusyBox-style tool is blocked by them
4. Continue small, high-signal ELF samples only when they either harden an ABI contract or directly unblock Linux userland behavior
