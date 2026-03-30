# POSIX Compatibility Roadmap (BusyBox as Acceptance Sample)

This document defines a pragmatic POSIX-compatibility roadmap for Charm.
BusyBox is treated as an acceptance sample for real-world userland behavior,
not as the POSIX specification itself.

## Positioning

- POSIX defines interface boundaries and minimum semantics.
- musl libc defines practical libc expectations.
- BusyBox validates real-world behavior and integration gaps.
- The goal is "usable compatibility", not full Linux parity.

## Architectural Fit

- The POSIX shim lives in Runtime/IO, not in Domain.
- All IO must go through `io.channel`/`io.reactor` and `fs.vfs`.
- Initialization must be via `init.graph` nodes and capabilities.
- Errors use `util::Errc` and `util::Result<T>`.

## Capability Map (Proposed)

- `posix.fd_table` -> manages file descriptors and stdio
  - depends on `fs.vfs`, `io.registry`, `system.clock`
- `posix.vfs_adapter` -> VFS path and stat translation
  - depends on `fs.vfs`, `fs.path`, `fs.errno`
- `posix.pipe` -> pipe/dup/redirect support
  - depends on `io.channel`, `io.reactor`
- `posix.proc` -> spawn/exec/wait (non-fork baseline)
  - depends on `kernel.eda`, `system.clock`
- `posix.signal` -> minimal signal model for shell/wait
  - depends on `kernel.eda`
- `posix.devfs` -> `/dev/null`, `/dev/console`, `/tmp`
  - depends on `fs.vfs`, `io.registry`

## Phased Acceptance Plan

### Phase 0: Core Glue (No BusyBox yet)

Minimum API set:
- `open/close/read/write/lseek`
- `stat/fstat`, `opendir/readdir`, `errno`
- `stdin/stdout/stderr` mapping to `io.console0`

Acceptance:
- VFS demos run without POSIX shim regressions.
- `errno` values match `util::Errc` mapping.

### Phase 1: FS Applets (BusyBox subset)

Targets:
- `echo`, `cat`, `ls`, `mkdir`, `rm`, `cp`, `mv`

Needs:
- Path rules, permissions stubs, `stat` behavior
- Basic glob handling (if BusyBox expects it)

Acceptance:
- Applets run with expected exit codes.
- Directory listing and file operations behave consistently.

### Phase 2: Shell + Redirect/Pipe

Targets:
- `sh`, `test`, `[`
- pipes and redirects: `|`, `>`, `<`, `>>`

Needs:
- `pipe`, `dup/dup2`, `close-on-exec` semantics
- simple spawn/exec (no fork, no MMU requirement)

Acceptance:
- Shell scripts with pipelines and redirects run.
- Basic command chaining returns correct status.

### Phase 3: Process Control

Targets:
- `ps`, `kill`, `sleep`, `xargs`, `find`

Needs:
- `wait/waitpid`, minimal signals, task status
- `/proc` or stubbed `ps` data model

Acceptance:
- Sleep/kill/wait flows behave correctly.
- `ps` returns stable, useful output.

## BusyBox Usage Model

- Use BusyBox as an acceptance suite.
- Keep a minimal applet list per phase.
- Validate behavior using strace on Linux as a reference.
- Do not treat BusyBox as the semantic source of truth.

## Non-Goals

- Full Linux syscall compatibility.
- True `fork` semantics without MMU.
- Full signal model parity.

## Risks and Mitigations

- fork/exec gap: provide spawn-only path and document constraints.
- signal complexity: scope to shell and wait semantics first.
- `/dev` expectations: implement `/dev/null` and `/dev/console` early.

## Suggested Validation Artifacts

- A per-phase checklist of applets and expected outputs.
- A trace report comparing Linux strace vs Charm shim calls.
- A small regression suite for fd/pipe/dup and exit codes.

