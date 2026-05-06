# System Compiler Front Page Entry Opening Flow Open Event Witness v0

`system_compiler.front_page_entry_opening_flow_open_event_witness/v0` is the
first compact witness projected from an explainable opening judgment.

It consumes:

- `system_compiler.front_page_entry_opening_flow_open_event/v0`

It does not replace the open-event record.

The open-event record is the full judgment.

The open-event witness is the portable testimony for that judgment.

More precisely:

```text
open_event
  = concrete OpeningJudgment carrier
open_event_witness
  = compact testimony projection of that judgment
open_event_compare
  = semantic drift judge for two opening judgments
open_event_witness_compare
  = testimony drift judge for two compact testimonies
```

## Current shape

Current `system_compiler.front_page_entry_opening_flow_open_event_witness`
includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_flow_open_event_witness.v0.schema.json`
- exporter
  - `scripts/export_system_compiler_front_page_entry_opening_flow_open_event_witness.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_open_event_witness.py`
- compare
  - `scripts/compare_system_compiler_front_page_entry_opening_flow_open_event_witness.py`
  - `scripts/compare_system_compiler_front_page_entry_opening_flow_open_event_witness_workspace.ps1`
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_open_event_witness_compare.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_compare_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_workspace_compare_smoke.ps1`

## Current outputs

The exporter leaves behind:

- `front-page.entry-opening-flow.open-event.witness.summary.json`
- `front-page.entry-opening-flow.open-event.witness.report.md`
- `front-page.entry-opening-flow.open-event.witness.check.txt`

The default output root is:

```powershell
out/system-compiler-front-page-entry-opening-flow-open-event-witness
```

The smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-smoke
```

The compare smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-compare-smoke
```

The workspace compare smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-workspace-compare-smoke
```

## What the witness records

The current summary records:

- `open_event_identity`
  - source event id, status, result, reason, and source artifact
- `judgment`
  - witness id and status
  - source judgment status, grade, basis, and summary
  - selected consumer and action
  - candidate and rejected consumer counts
  - compare attachment and verdict
  - workspace facade status
  - evidence and artifact ref counts
- `witness_entry`
  - a compact witness-entry-shaped projection
  - `kind=open_event_witness`
  - `layer=opening_flow`
  - observations suitable for later bundle consumption
- `evidence_refs`
  - the source open-event witness refs, normalized as explicit evidence inputs
  - runtime-session wrappers may also project `consumer_summary_ref` and
    `selected_artifact_ref` as additive evidence refs
- `explanation`
  - the hard explain text projected from the source open-event explanation view
  - runtime-session wrappers may also project `opening_input_observations`
    so the witness can carry the selected opening route without reproving
    session semantics

## Witness status

The witness status is intentionally about whether the opening judgment can
stand as testimony:

- `ok`
  - source open-event is `accepted` or `accepted_with_drift`
  - no source violations are present
- `fail`
  - source open-event is `blocked`
  - source violations are present

This means `accepted_with_drift` is still a valid witness.

It is not a failed testimony.

It is an honest testimony that says compare context changed the opening
judgment.

## Source judgment projection

The witness keeps witness status separate from source judgment status.

Current source judgment fields inside `judgment` are:

- `source_judgment_status`
  - copied from the source open event's judgment status
- `source_judgment_grade`
  - `described` or `compared`
- `source_judgment_basis`
  - the evidence roles that support the source judgment carrier
- `source_judgment_summary`
  - the source judgment's short system-testimony sentence

This makes the witness a testimony projection rather than a second root
judgment.

## Runtime-session opening route projection

When the source open event comes from the runtime-session-specific wrapper:

- `witness_entry.witness_focus` becomes:
  - `front_page`
  - `opening_flow`
  - `runtime_session`
  - `session_witness`
  - `artifact_target`
- `accepted_with_drift` still produces `witness_status=ok`
- `opening_input_observations` preserve:
  - consumer summary ref
  - selected focus ref
  - selected explain hop ref
  - selected artifact ref
  - fallback artifact refs

This witness still does not re-prove runtime/session/world-compare meaning.

It only proves that the opening selection, route, and facade remained stable.

## Relationship to OpeningJudgmentCorridor

Within `OpeningJudgmentCorridor`, `open_event_witness` belongs to
`Testimony projection`.

It is the compact testimony carrier for one already-formed opening judgment.

It must not:

- recompute the lower selection policy
- reopen raw runtime/session/world-compare evidence
- replace landing, route, explain-entry, or handoff policy

## Manual example

Run the smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_smoke.ps1 -Clean
```

Run the witness compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_compare_smoke.ps1 -Clean
```

Run the witness workspace compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_workspace_compare_smoke.ps1 -Clean
```

Export directly from an open-event summary:

```powershell
python ./scripts/export_system_compiler_front_page_entry_opening_flow_open_event_witness.py `
  --open-event cmake-build-system-compiler-front-page-entry-opening-flow-open-event-smoke/default-no-compare/front-page.entry-opening-flow.open-event.summary.json `
  --output-root cmake-build-open-event-witness
```

Then validate:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_flow_open_event_witness.py `
  --summary cmake-build-open-event-witness/front-page.entry-opening-flow.open-event.witness.summary.json
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-SMOKE] case=default-no-compare-witness witness_status=ok event_status=accepted compare=False/not_attached
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-SMOKE] case=default-with-drift-compare-witness witness_status=ok event_status=accepted_with_drift compare=True/drifted
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-COMPARE-SMOKE] case=open-event-witness-self-standing verdict=standing changed=0
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-COMPARE-SMOKE] case=open-event-witness-default-to-drift-context verdict=drifted
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE-SMOKE] case=workspace-witness-summary-self-standing verdict=standing changed=0
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE-SMOKE] case=workspace-open-event-summary-to-drift-witness verdict=drifted changed=17
```

## Why this matters

The opening-flow chain now has a portable evidence unit:

- action
  - chooses one deterministic opener witness to execute now
- open event
  - records the full explainable opening judgment
- open event witness
  - distills that judgment into compact testimony
- open event witness compare
  - judges whether two testimonies still prove the same opening judgment
- open event compare
  - judges whether two opening judgments preserve the same semantics

This keeps `witness_bundle/v0` from growing too early while still giving later
bundle and constitution work a clean object to consume.
