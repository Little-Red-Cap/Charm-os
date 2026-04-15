# POSIX Compatibility Roadmap (Maintenance Mode)

This document now serves as a maintenance-mode roadmap for Charm POSIX.

`POSIX v0` is considered closed. That means the subsystem is no longer on an
open-ended feature-expansion track. Future work should be driven by real
blockers, real userland samples, and explicit contract gaps.

Related documents:

- `docs/system/posix_v0_closure_checklist.md`
- `docs/system/posix_stage_summary.md`
- `docs/system/posix_support_overview.md`
- `docs/system/posix_busybox_phase_checklist.md`

## Current Position

- POSIX defines interface boundaries and minimum semantics.
- musl/newlib define practical libc-facing expectations.
- BusyBox remains an acceptance sample for real-world userland behavior.
- The goal stays "usable compatibility", not full Linux parity.

## What Is Already Closed

The following baseline is now treated as established:

- file/dir/fd/stdio minimum userland spine
- shell redirect / pipe minimum behavior
- same-address-space spawn/wait execution model
- minimal process-control slice: `getpid`, `sleep`, `kill`, `waitpid`, `ps`
- public C surface + newlib bridge minimum stability
- QEMU mainline smoke and dedicated newlib stdio smoke

This means the previous phase-oriented roadmap has effectively reached:

- Phase 0: closed
- Phase 1: closed
- Phase 2: closed
- Phase 3 minimum: closed

The detailed historical acceptance slices still live in
`docs/system/posix_busybox_phase_checklist.md` and
`docs/system/posix_stage_summary.md`.

## Architecture Still Holds

- The POSIX shim lives in Runtime/IO, not in Domain.
- All IO should still flow through runtime abstractions such as VFS, fd tables,
  channels, and the runtime execution model.
- Initialization should remain capability-driven.
- Errors should continue to translate through `util::Errc` and the POSIX bridge.

The closure of `v0` does not relax these boundaries; it makes them more
important, because future work must stay incremental and disciplined.

## Post-v0 Working Model

Future work should follow this loop:

1. Identify a real blocker.
2. Reduce it to the smallest missing runtime/ABI contract.
3. Patch only the owning module boundary.
4. Add the smallest validating smoke/sample.
5. Sync the contract into docs.

If a proposed change cannot point to a real blocker, it should normally not be
on the active roadmap.

## Active Roadmap Categories

### 1. Regression Defense

Use this when:

- an existing smoke fails
- a build/toolchain change breaks POSIX validation
- the public headers, runtime bridge, and actual behavior drift apart

Typical work:

- contract fixes
- errno/return-value alignment
- smoke maintenance
- documentation sync

### 2. Real Sample Unblocking

Use this when:

- a BusyBox applet is genuinely blocked
- a real ELF sample exposes a missing capability
- a minimal C/newlib userland sample cannot run because of a POSIX-side gap

Typical work:

- add a focused reproducer
- implement the smallest missing contract
- promote the result into a stable minimal regression

### 3. Environment Extensions

Use this only when a real sample requires it and the value is reusable.

Candidates include:

- more `devfs` nodes
- minimal `/proc` views
- wider `stat` field coverage
- `select/poll`
- `termios`
- broader socket-facing compatibility

These are not default expansion items anymore.

## BusyBox Usage Model

- Keep using BusyBox as an acceptance suite.
- Prefer a minimal applet set tied to real blockers.
- Do not treat BusyBox as the semantic source of truth.
- Do not widen the applet list just to make the matrix look larger.

## Non-Goals

- Full Linux syscall compatibility.
- True `fork` semantics without MMU.
- Full signal model parity.
- Open-ended "cover more API because it exists on Linux" work.

## Review Filter For Future POSIX Work

Before new POSIX work becomes active, it should answer:

- Which real program/sample is blocked?
- Which exact contract is missing or drifting?
- Which module owns the fix?
- What is the smallest validation path?
- Which document needs to move with the change?

If these answers are weak, the work probably belongs in the parking lot, not on
the active roadmap.

## Suggested Validation Artifacts

- a minimal reproducer per newly-unblocked capability
- a focused smoke for the owned contract
- QEMU coverage when the behavior is user-visible at runtime
- a matching doc update in the POSIX subsystem docs
