# BusyBox Compatibility Checklist (Phases 1-3)

This is a practical acceptance checklist for BusyBox-style behavior.
Use it alongside `docs/system/posix_compat_roadmap.md`.

## Phase 1: FS Basics

Applets:
- echo
- cat
- ls
- mkdir
- rm
- cp
- mv

Acceptance:
- basic path handling
- `stat/fstat` behavior
- correct exit codes
- stderr on errors

## Phase 2: Shell + Redirect + Pipe

Targets:
- sh, test, [
- redirect: `>`, `>>`, `<`, `2>`, `2>&1`
- pipe: `|`

Acceptance:
- pipeline works across multiple commands
- redirection overrides stdout/stderr
- `PATH` lookup resolves commands
- `isatty` influences shell formatting (prompt)

## Phase 3: Process Control

Targets:
- ps
- kill
- sleep
- xargs
- find

Acceptance:
- `waitpid` returns correct status
- `kill` supports SIGTERM/SIGKILL/SIGINT
- `sleep` respects clock/timer
- `ps` prints pid/state/name minimally

