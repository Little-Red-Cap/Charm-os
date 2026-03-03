# io.reactor Contract (Hard Rules)

This document defines the non-negotiable contract for io.reactor.

## 1) notify() is ISR-safe and enqueue-only

- notify() must not run protocol callbacks.
- It only enqueues events and triggers the waker hook.

## 2) drain() runs in task context

- drain() dispatches callbacks and must be driven by a kernel task/EDA.
- drain() may call multiple callbacks; callbacks must not block.

## 3) Callback rules (budgeted)

- Each callback must use a fixed budget (bytes/iterations).
- No busy-spin, no sleep, no internal timeouts.
- After budget is consumed, return and wait for the next event.

## 4) subscribe/unsubscribe context

- subscribe/unsubscribe are task-context only.
- ISR must not call subscribe/unsubscribe.
- Callback may unsubscribe itself only if the implementation guarantees safety.

## 5) Waker hook

- Reactor provides a waker hook to integrate with the kernel scheduler.
- notify() must trigger waker when it enqueues or merges events.
