# RTOS Long-Term Contracts

This document records the non-negotiable, long-term constraints for the Charm
RTOS. These rules are intended to keep the core deterministic, portable, and
auditable across MCU targets.

## 1) Core vs Port Boundary

Core responsibilities:
- Scheduling and state transitions.
- Synchronization semantics.
- Timeouts and wait paths.

Port responsibilities:
- Context switch entry/exit.
- IRQ/critical section control.
- Tick source and first task start.

Board drivers must not enter the core.

## 2) Lifecycle Model

Two phases are mandatory:
- Startup registration.
- Runtime activation.

Runtime creation is forbidden in production builds. A preallocated pool may be
activated at runtime. Dynamic creation is allowed only in PC/debug profiles.

## 3) ISR Semantics

ISR APIs must be explicitly named and restricted to:
- Increment/mark.
- Defer wakeups to task context.

Task-context APIs must reject ISR usage (debug assert).
ISR deferral must be centralized (scheduler-level poll path).

## 4) Observability Is Mandatory

The RTOS must always provide:
- Trace events (ring buffer).
- Scheduler statistics.

Observability can be disabled at runtime, but not removed from the core.

## 5) Cancellation & Cleanup

Blocking waits must support explicit cancellation:
- Cancelled waits return `WaitResult::cancelled`.
- Global cleanup is allowed only in task context and must cancel all waiters.

## 6) Priority Inversion

Detection must exist (trace + stats). Priority inheritance may be a later
implementation, but detection is mandatory.

## 7) Portability Roadmap

Roadmap order is fixed:
1. Cortex-M: stability and determinism first.
2. RISC-V: minimal port as the second target.
3. Cortex-A: only after port boundaries are proven in the above targets.
