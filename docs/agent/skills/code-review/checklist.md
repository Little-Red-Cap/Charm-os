# Code Review Checklist

> status: `supporting`
>
> scope: review prompts; topic contracts and source remain authoritative

Use this after reading the diff, affected source/CMake, real consumers and
tests. A prompt is applicable only when the reviewed path owns that behavior.
Do not turn project policy, MCU restrictions or one module's error type into a
repository-wide rule.

## Evidence And Scope

- [ ] Is the claimed behavior visible in source and target wiring, not only the
      commit message or README?
- [ ] Are the real consumer, owner, execution context and lifetime identified?
- [ ] Are positive, negative and failure paths covered at the evidence domain
      required by the claim?
- [ ] Are Host, QEMU, build-only and real-board results distinguished?
- [ ] Does the change touch public behavior, and if so is the canonical contract
      updated instead of adding a parallel explanation?

## Correctness And Failure

- [ ] Are input validation, partial progress, retry/cancel/timeout and terminal
      states explicit?
- [ ] Are backend errors preserved or deliberately translated at a named
      boundary?
- [ ] Does failure leave ownership and mutable state valid for the next allowed
      operation?
- [ ] Are fixed capacities, alignment, overflow and buffer lifetime checked?
- [ ] Are nullable pointers, callbacks, handles and stale tokens rejected or
      given explicit semantics?

## Ownership And Dependencies

- [ ] Does Core remain free of project, board, HAL, UI and product facts?
- [ ] Does the consumer use the narrowest existing entry rather than expanding
      a facade for convenience?
- [ ] Are board/vendor details isolated behind the actual service/backend
      boundary?
- [ ] If `init.graph` is used, are provides/requires, phase, runlevel, capacity
      and first-error behavior correct?
- [ ] Is a cross-boundary dependency justified by a real consumer rather than a
      historical directory model?

## Execution And Resources

- [ ] For ISR, realtime, kernel or DMA paths, are blocking, allocation,
      formatting, cache and concurrency constraints explicit?
- [ ] For non-blocking IO, do zero progress, partial transfer and `would_block`
      match the relevant channel/protocol contract?
- [ ] Is timeout driven by the owning clock/reactor/scheduler rather than a
      hidden busy loop?
- [ ] Are task/ISR/reactor handoffs explicit and observable?
- [ ] Does third-party code remain at a controlled adapter/build boundary?

## Signal, State And Wiring

- [ ] Is `emit()` limited to synchronous same-domain notification?
- [ ] Is persistent truth stored in state rather than reconstructed from an
      event stream?
- [ ] Do cross-context actions use an explicit post/queue/ingress path?
- [ ] Is long-lived connection ownership visible and shorter than its target
      lifetime?
- [ ] Can callbacks mutate connection topology safely under the actual signal
      implementation?

## Temporary Exceptions

- [ ] Is the reason concrete and the affected scope minimal?
- [ ] Is the workaround isolated from stable interfaces?
- [ ] Are failure behavior, evidence limitation and removal condition recorded?

## Review Output

Report findings first, ordered by severity. Each finding needs a file/line,
observable impact and repair direction. If no finding remains, state unrun tests
and residual risk; do not add empty sections or style-only filler.

Rules: [`charm-architecture`](../../rules/charm-architecture.md),
[`embedded-modern-cpp`](../../rules/embedded-modern-cpp.md), and the
[`signal/state contract`](../../../architecture/signal_state_contract_v0.md).
