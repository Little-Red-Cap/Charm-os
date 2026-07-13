## Summary

- What changed
- Why this change is needed

## SSU Submit Mapping (Required for execution-path changes)

If this PR introduces or changes any execution path (task wakeup, pump, loop step, deferred work, submit API, ISR defer path), fill all items below.

- Submit kind: `event-submit` / `io-ready-submit` / `demand-submit` / `none`
- Why this submit kind is correct:
- Execution domain: `task_only` / `isr_safe` / `mixed`
- Blocking behavior: `non_blocking` / `may_block`
- Budget behavior: `single_step` / `budgeted`
- Resubmit path (if any):
- Recovery path if fallback/temporary bypass exists:

## RunLoop/Phase Audit (if applicable)

For any added/modified `RunLoop::add_step(...)`:

- `submit_projection` explicitly set: yes / no
- Projection value: `event-submit` / `io-ready-submit` / `demand-submit`

## Risk & Validation

- Main risk:
- Build/verify commands run:
- Target(s) verified:

## Notes

- Anything intentionally deferred

## SSU Submit Gate (Required when submit paths change)

Run:

- `scripts/ssu_submit_gate.ps1`
