# System Compiler Front Page Entry Opening Flow Open Event Witness v0

`system_compiler.front_page_entry_opening_flow_open_event_witness/v0` is the
first compact witness projected from an explainable opening judgment.

It consumes:

- `system_compiler.front_page_entry_opening_flow_open_event/v0`

It does not replace the open-event record.

The open-event record is the full judgment.

The open-event witness is the portable testimony for that judgment.

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
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_open_event_witness_compare.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_compare_smoke.ps1`

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

## What the witness records

The current summary records:

- `open_event_identity`
  - source event id, status, result, reason, and source artifact
- `judgment`
  - witness id and status
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
- `explanation`
  - the hard explain text projected from the source open-event explanation view

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

## Manual example

Run the smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_smoke.ps1 -Clean
```

Run the witness compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_compare_smoke.ps1 -Clean
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
