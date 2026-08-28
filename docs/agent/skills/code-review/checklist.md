# Code Review Checklist

> status: `supporting`
>
> scope: review prompts; topic contracts and source remain authoritative

Use this after reading the diff, source/CMake, consumers and tests. Apply only prompts owned by the reviewed path;
project policy, MCU restrictions and local error types are not repository-wide rules.

## Evidence And Scope

- [ ] Is the claimed behavior visible in source and target wiring, not only the
      commit message or README?
- [ ] Are consumer, owner, execution context, lifetime and required evidence domain identified?
- [ ] Are positive, negative and failure paths covered?
- [ ] Are Host, QEMU, build-only and real-board results distinguished?
- [ ] Does the change touch public behavior, and if so is the canonical contract
      updated instead of adding a parallel explanation?

## Correctness And Failure

- [ ] Are input validation, partial progress, retry/cancel/timeout and terminal
      states explicit?
- [ ] Are backend errors preserved or deliberately translated at a named
      boundary?
- [ ] Does failure leave ownership and mutable state valid for the next allowed operation?
- [ ] Are capacity, alignment, overflow, buffer lifetime and stale/null handles explicit?

## Ownership And Dependencies

- [ ] Does Core remain free of project, board, HAL, UI and product facts?
- [ ] Does the consumer link `Charm::core` and use the public include path instead of bypassing the target?
- [ ] Is each cross-boundary dependency justified by a real consumer rather than a historical directory model?

## Execution And Resources

- [ ] Does the public header remain free of allocation, exceptions, platform vocabulary and hidden state?
- [ ] Are build-only, Host-run and cross-environment claims kept separate?

## Temporary Exceptions

- [ ] Is the reason concrete, scope minimal and workaround isolated from stable interfaces?
- [ ] Are failure behavior, evidence limitation and removal condition recorded?

## Review Output

Report findings first by severity, with file/line, impact and repair direction. If none remain, state unrun tests and
residual risk; do not add empty sections or style-only filler.

Rules: [`charm-architecture`](../../rules/charm-architecture.md),
[`embedded-modern-cpp`](../../rules/embedded-modern-cpp.md).
