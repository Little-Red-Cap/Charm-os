# System Compiler Front Page Entry Opening Flow Consumer Plan Compare v0

`system_compiler.front_page_entry_opening_flow_consumer_plan_compare/v0`
compares two
`system_compiler.front_page_entry_opening_flow_consumer_plan/v0` summaries.

It answers one practical question:

- did the explain-open action plan preserve the same consumer execution shape

The compare object does not rerun selector, opener, landing, route, or
workspace export tools.

It consumes two existing consumer plan summaries and leaves a deterministic
compare witness that later explain tooling can read directly.

## Current shape

Current
`system_compiler.front_page_entry_opening_flow_consumer_plan_compare`
includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_flow_consumer_plan_compare.v0.schema.json`
- exporter
  - `scripts/compare_system_compiler_front_page_entry_opening_flow_consumer_plan.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_consumer_plan_compare.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_compare_smoke.ps1`

## Current outputs

By default the compare exporter leaves behind:

- `front-page.entry-opening-flow.consumer.plan.compare.summary.json`
- `front-page.entry-opening-flow.consumer.plan.compare.report.md`
- `front-page.entry-opening-flow.consumer.plan.compare.check.txt`

The dedicated smoke writes under:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-compare-smoke
```

## What the compare records

The current summary records:

- baseline and candidate consumer plan summary paths
- baseline and candidate plan provenance
- plan-level result, execution status, action count, next action count,
  omitted count, default action, and compare action changes
- ordered action id additions, removals, and order drift
- per-action changes for rank, operation, target, projection, compare context,
  opener path, and inspector readiness
- regression surface narratives
- a verdict:
  - `standing`
  - `improved`
  - `drifted`
  - `collapsed`

## Current smoke evidence

The smoke currently proves two paths:

- `self-standing`
  - compares the consumer plan summary with itself
  - expected verdict: `standing`
- `removed-compare-action`
  - removes `open-compare-neighbor` from a synthetic candidate
  - expected verdict: `drifted`

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-COMPARE-SMOKE] case=self-standing verdict=standing changed=0 added=0 removed=0 default_changed=False compare_changed=False
[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-COMPARE-SMOKE] case=removed-compare-action verdict=drifted changed=3 added=0 removed=1 default_changed=False compare_changed=True
```

## Boundary

This object intentionally stays at consumer-plan compare level.

It does not:

- rebuild the consumer selector or consumer plan
- rerun opener, landing, capability, route, biography, witness, or world
  compare tools
- execute inspector commands
- decide UI state
- invent a second selection policy

It only judges whether two already materialized consumer execution plans still
ask an explain consumer to perform the same first actions.

Target and opener paths are normalized relative to the nearest selector
workspace root when possible. That keeps scratch output roots from looking like
action-plan drift while preserving original paths in provenance fields.

## Manual example

Generate the base consumer plan witness:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_smoke.ps1 -Clean
```

Run the compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_compare_smoke.ps1 -Clean
```

Or compare two explicit consumer plan summaries:

```powershell
python ./scripts/compare_system_compiler_front_page_entry_opening_flow_consumer_plan.py `
  --baseline cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-smoke/front-page.entry-opening-flow.consumer.plan.summary.json `
  --candidate cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-smoke/front-page.entry-opening-flow.consumer.plan.summary.json `
  --output-root cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-compare-smoke/self-standing
```

Validate the compare object:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_flow_consumer_plan_compare.py `
  --summary cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-compare-smoke/self-standing/front-page.entry-opening-flow.consumer.plan.compare.summary.json
```

## Why this matters

`front_page_entry_opening_flow_consumer_plan/v0` made the opening handoff
executable.

`front_page_entry_opening_flow_consumer_plan_compare/v0` makes that executable
handoff interrogable across time.

Instead of asking later explain tooling to diff plan JSON, it can consume one
object that says:

- did the default opener action change
- did the compare-neighbor action move or disappear
- did fallback action order drift
- did target, operation, projection, compare context, or inspector readiness
  change
- which consumer action changed first

That keeps the front-page explain line close to Charm's larger rule: every
surface that can be executed should also be able to testify about how it
changed.
