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

## Stable ABI Contracts

### ELF execution / process
- `explicit exit > return` is the active exit resolution rule
- `waitpid()` consumes the unified final exit result, not the internal exit mechanism
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

## Mainline Capabilities Already Holding
- file-backed ELF can be loaded and spawned from a path
- stdio/fd/pipe/wait form a working minimal userland spine
- busybox phase2 smoke remains usable on top of the current spine
- child fd cleanup on exit is in place and no longer destabilizes pipe EOF behavior
- real-ELF smoke now isolates env/stderr subcases per harness, avoiding shared-fd cross contamination
- `spawnp`-style PATH lookup is now exercised from `posix.api` and program-shell smoke, not only proc smoke

## Isolated / Deferred Issues
- no current isolated smoke blocker; remaining work is focused on expanding semantics rather than restoring the mainline

## Recommended Next Cuts
- structural cleanup plan: `docs/system/posix_cleanup_refactor_plan.md`
1. Extend `stat_probe` from size/error basics to `mode` / file-type fields without re-introducing helper-side layout coupling
2. Extend the current `spawnp`/shell PATH coverage toward more Linux-like command resolution (`argv[0]`, shell fallback, BusyBox entry shapes)
3. Decide whether v0 should keep converging path-type errors beyond `open()` into `mkdir` / `truncate` / `rename`
4. Continue small, high-signal ELF samples only when they either harden an ABI contract or directly unblock Linux userland behavior
