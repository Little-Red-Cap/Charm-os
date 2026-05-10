# System Compiler Front Page Entry Opening Testimony Landing v0

`system_compiler.front_page_entry_opening_testimony_landing/v0` is a thin
explain-entry landing projected from one standard open-event witness.

It consumes only:

- `system_compiler.front_page_entry_opening_flow_open_event_witness/v0`

It does not consume runtime session summaries, world compare summaries, or raw
kernel runtime evidence.

## Role

The opening-flow chain is now intentionally layered:

```text
open_event
  = concrete OpeningJudgment carrier
open_event_witness
  = testimony projection of that judgment
opening_testimony_landing
  = explain-entry landing for that testimony
```

This object answers one question:

```text
Which explain entry should this opening judgment testimony open as?
```

It does not replace runtime-session bridge work.

The runtime-session bridge may eventually produce an open event and then an
open-event witness. This landing only consumes the resulting witness testimony.

For runtime-session specifically, this landing now anchors a second upward
corridor beside the already-committed opener/workspace path:

```text
runtime-session consumer
  -> runtime-session opening bridge
  -> open_event
  -> open_event_witness
  -> opening_testimony_landing
```

That corridor shares the same `open_event_witness` handoff object as the
opener/workspace corridor. Neither corridor is allowed to reopen raw runtime
session evidence once the witness boundary has been crossed.

## Relationship to OpeningJudgmentCorridor

Within `OpeningJudgmentCorridor`, `opening_testimony_landing` is the terminal
object of `Testimony projection` and the first lawful root of the upper
reading corridor.

It must not:

- re-prove session semantics
- reopen raw runtime/session/world-compare evidence
- replace route traversal or explain-entry selection

## Boundary

`opening_testimony_landing/v0` must not:

- re-prove session semantics
- parse runtime/session/world-compare evidence
- infer default focus from raw lower-layer artifacts
- expand open-event compare semantics
- replace the runtime-session bridge or wrapper

It only projects witness-declared facts into an explain-entry landing.

## Current Shape

Current `system_compiler.front_page_entry_opening_testimony_landing` includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_testimony_landing.v0.schema.json`
- exporter
  - `scripts/export_system_compiler_front_page_entry_opening_testimony_landing.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_testimony_landing.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opening_testimony_landing_smoke.ps1`
- compare
  - `scripts/compare_system_compiler_front_page_entry_opening_testimony_landing.py`
  - `scripts/validate_system_compiler_front_page_entry_opening_testimony_landing_compare.py`
  - `scripts/system_compiler_front_page_entry_opening_testimony_landing_compare_smoke.ps1`
- route closure
  - `scripts/system_compiler_front_page_entry_opening_testimony_landing_route_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_testimony_landing_route_compare_smoke.ps1`

The exporter leaves behind:

- `front-page.entry-opening-testimony.landing.summary.json`
- `front-page.entry-opening-testimony.landing.report.md`
- `front-page.entry-opening-testimony.landing.check.txt`

The default output root is:

```powershell
out/system-compiler-front-page-entry-opening-testimony-landing
```

The smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-opening-testimony-landing-smoke
```

The runtime-session-targeted smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-smoke
```

## Ready Rule

The landing is `ready` when the source witness has enough testimony to open:

- source schema and kind are the standard open-event witness
- `result=ok`
- `judgment.witness_status=ok`
- `witness_entry.source_path` is present and resolves
- `explanation.text_lines` is non-empty

Otherwise the landing is `blocked`.

Blocked violations only describe missing witness or landing inputs. They do
not interpret runtime session, world compare, or kernel evidence semantics.

## Output Meaning

The summary records:

- `source_witness_ref`
  - the source witness summary, report, and check
- `opening_identity`
  - open event id, status, reason, source judgment status, and source judgment
    grade
- `landing_decision`
  - `selected_entry_id=open-event-witness`
  - `selected_tab_id=opening_testimony`
  - `selected_role=opening_testimony`
- `testimony_preview`
  - source judgment summary
  - witness explanation text lines
  - witness observations
- `artifact_targets`
  - evidence refs already declared by the witness
  - artifact refs already declared by the witness entry
- `next_questions`
  - typed explain hops for the next reader action

## Typed Next Questions

The first version always emits:

- `inspect_open_event`
  - inspect the source open event carried by the testimony
- `inspect_evidence_refs`
  - inspect the evidence refs already declared by the witness
- `compare_open_event_witness`
  - compare this testimony against another open-event witness

These are explain-entry hints, not compare semantics.

## Manual Example

Run the smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_landing_smoke.ps1 -Clean
```

Run the landing compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_landing_compare_smoke.ps1 -Clean
```

Run the route closure smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_landing_route_smoke.ps1 -Clean
```

Run the route compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_landing_route_compare_smoke.ps1 -Clean
```

Export directly from an open-event witness:

```powershell
python ./scripts/export_system_compiler_front_page_entry_opening_testimony_landing.py `
  --witness cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-smoke/default-no-compare-witness/front-page.entry-opening-flow.open-event.witness.summary.json `
  --output-root cmake-build-opening-testimony-landing
```

Then validate:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_testimony_landing.py `
  --summary cmake-build-opening-testimony-landing/front-page.entry-opening-testimony.landing.summary.json
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-SMOKE] case=clean-witness-landing status=ready source_judgment=accepted
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-SMOKE] case=drift-witness-landing status=ready source_judgment=accepted_with_drift
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-SMOKE] case=blocked-empty-explanation-landing status=blocked source_judgment=accepted
```

## Why This Matters

The front-page/opening-flow line now has a downstream explain seam:

```text
opening judgment
  -> testimony projection
  -> explain-entry landing
```

That keeps lower-layer session witnesses and upper-layer opening behavior
connected without mixing their responsibilities.
