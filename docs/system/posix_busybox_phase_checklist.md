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

Current Charm slice:
- done: `ls`, `mkdir`, `rm`, `mv`
- deferred: `cp`
- scope guard: keep `truncate`, `O_APPEND`, `lseek`, and wider path/stat matrices out of this first cut

Acceptance:
- basic path handling
- `stat/fstat` behavior
- correct exit codes
- stderr on errors

Current acceptance on mainline smoke:
- `busybox mkdir work`
- `busybox ls /`
- `busybox mv /work/a.txt /work/b.txt`
- `busybox rm /work/b.txt`
- `busybox ls /work`

## Phase 2: Shell + Redirect + Pipe

Targets:
- sh, test, [
- redirect: `>`, `>>`, `<`, `2>`, `2>&1`
- pipe: `|`

Current Charm slice:
- done: `>`, `>>`, `<`, `2>`, `2>&1`, `|`
- scope guard: append is kept at the minimum shell/userland contract level; wider `lseek`/truncate semantics stay deferred

Acceptance:
- pipeline works across multiple commands
- redirection overrides stdout/stderr
- `PATH` lookup resolves commands
- `isatty` influences shell formatting (prompt)

Current acceptance on mainline smoke:
- `sh -c 'echo hi > out.txt'`
- `sh -c 'echo ho >> out.txt'`
- `sh -c 'echo hi | cat'`
- `sh -c 'cat < input.txt'`
- `sh -c 'stderr_demo 2> err.txt'`
- `sh -c 'stderr_demo > both.txt 2>&1'`
- `sh -c 'stderr_demo >> both.txt 2>&1'`

## Phase 3: Process Control

Targets:
- ps
- kill
- sleep
- xargs
- find

Current Charm slice:
- done: `getpid` base contract, minimum `sleep`, `kill v0`, minimal `ps`
- next: widen Phase 3 userland coverage without expanding into full process groups/signal model
- scope guard: keep this phase focused on minimum userland-observable behavior, not a full Linux signal/process model

Acceptance:
- `waitpid` returns correct status
- `kill` supports SIGTERM/SIGKILL/SIGINT
- `sleep` respects clock/timer
- `ps` prints pid/state/name minimally

Current acceptance on mainline smoke:
- API-level bound-process `getpid()`
- real ELF `getpid()` output equals spawned pid
- API-level `sleep(0/1)`
- proc smoke `kill(SIGTERM)` on enter yields `waitpid(signaled)` and prevents target execution
- API smoke `kill(SIGTERM)` yields encoded wait status `SIGTERM`
- `sh -c 'ps'` prints the current minimal `pid/state/name` view
- `sh -c 'sleep 2'`
