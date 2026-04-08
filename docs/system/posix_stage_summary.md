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
- `stat_probe` is temporarily isolated so it does not block the mainline smoke

## Stable ABI Contracts

### ELF execution / process
- `explicit exit > return` is the active exit resolution rule
- `waitpid()` consumes the unified final exit result, not the internal exit mechanism
- ELF hostcalls use `ExecContext`; they no longer depend on the old global service/pid slot pair

### fd / errno contracts validated by smoke
- `open("/missing.txt") -> -1 && errno == ENOENT`
- `isatty(non-tty) -> 0 && errno == 0`
- `fstat(-1, ...) -> -1 && errno == 5`
- `read(EOF) -> 0 && errno == 0`
- `read(-1, ...) -> -1 && errno == 95`
- `write(-1, ...) -> -1 && errno == 95`
- `open("/dir", O_WRONLY) -> -1 && errno == EISDIR`
- `open("/file/child", O_RDONLY) -> -1 && errno == ENOTDIR`

## Mainline Capabilities Already Holding
- file-backed ELF can be loaded and spawned from a path
- stdio/fd/pipe/wait form a working minimal userland spine
- busybox phase2 smoke remains usable on top of the current spine
- child fd cleanup on exit is in place and no longer destabilizes pipe EOF behavior

## Isolated / Deferred Issues
- `stat_probe`: currently behaves like a function-body / layout-level anomaly in the test host path; isolated from mainline smoke
- `close(-1)`: currently returns `-1`, but errno has not been stabilized to the intended v0 contract value yet

## Recommended Next Cuts
- structural cleanup plan: `docs/system/posix_cleanup_refactor_plan.md`
1. Resume `stat_probe` on a separate line, keeping helper-based or split-function experiments away from the mainline smoke
2. Decide whether v0 should keep converging path-type errors beyond `open()` into `mkdir` / `truncate` / `rename`
3. Continue small, high-signal ELF samples only when they either harden an ABI contract or directly unblock Linux userland behavior
