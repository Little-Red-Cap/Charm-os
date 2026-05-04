# System Compiler Front Page Entry Opening Flow Open Event Witness Compare v0

`system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0`
compares two compact OpenEventWitness objects.

It consumes:

- `system_compiler.front_page_entry_opening_flow_open_event_witness/v0`

It does not compare full OpenEventRecord payloads.

The open-event compare judges the full opening judgment.

The open-event witness compare judges whether two portable testimonies still
prove the same judgment.

## Current shape

Current
`system_compiler.front_page_entry_opening_flow_open_event_witness_compare`
includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_flow_open_event_witness_compare.v0.schema.json`
- compare
  - `scripts/compare_system_compiler_front_page_entry_opening_flow_open_event_witness.py`
  - `scripts/compare_system_compiler_front_page_entry_opening_flow_open_event_witness_workspace.ps1`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_open_event_witness_compare.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_compare_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_workspace_compare_smoke.ps1`

## Current outputs

The compare exporter leaves behind:

- `front-page.entry-opening-flow.open-event.witness.compare.summary.json`
- `front-page.entry-opening-flow.open-event.witness.compare.report.md`
- `front-page.entry-opening-flow.open-event.witness.compare.check.txt`

The default output root is:

```powershell
out/system-compiler-front-page-entry-opening-flow-open-event-witness-compare
```

The smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-compare-smoke
```

The workspace compare smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-workspace-compare-smoke
```

## What the compare records

The current summary records:

- `witness_status`
  - baseline and candidate witness identities, source events, status, selected consumer, and selected action
- `identity_changes`
  - source event id, status, reason, and source artifact drift
- `judgment_changes`
  - witness status, selected consumer/action, compare context, workspace facade, and ref count drift
- `witness_entry_changes`
  - compact witness entry, subject facets, observations, and artifact refs drift
- `evidence_ref_changes`
  - evidence roles and normalized evidence refs drift
- `explanation_changes`
  - hard explain text drift
- `witness_regression_surface`
  - semantic flags and narratives explaining the verdict

## Verdicts

The witness compare emits:

- `standing`
  - both testimonies preserve the same witness semantics
- `improved`
  - candidate recovers from a failing testimony
- `drifted`
  - candidate still stands, but testimony semantics changed
- `collapsed`
  - candidate no longer stands as valid testimony

## Manual example

Run the smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_compare_smoke.ps1 -Clean
```

Run the workspace compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_workspace_compare_smoke.ps1 -Clean
```

Compare two witness summaries directly:

```powershell
python ./scripts/compare_system_compiler_front_page_entry_opening_flow_open_event_witness.py `
  --baseline cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-smoke/default-no-compare-witness/front-page.entry-opening-flow.open-event.witness.summary.json `
  --candidate cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-smoke/default-with-drift-compare-witness/front-page.entry-opening-flow.open-event.witness.summary.json `
  --output-root cmake-build-open-event-witness-compare
```

Then validate:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_flow_open_event_witness_compare.py `
  --summary cmake-build-open-event-witness-compare/front-page.entry-opening-flow.open-event.witness.compare.summary.json
```

Compare through the workspace wrapper from witness summaries:

```powershell
./scripts/compare_system_compiler_front_page_entry_opening_flow_open_event_witness_workspace.ps1 `
  -BaselineOpenEventWitnessSummaryPath cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-smoke/default-no-compare-witness/front-page.entry-opening-flow.open-event.witness.summary.json `
  -CandidateOpenEventWitnessSummaryPath cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-smoke/default-with-drift-compare-witness/front-page.entry-opening-flow.open-event.witness.summary.json `
  -OutputRoot cmake-build-open-event-witness-workspace-compare `
  -Clean
```

The same wrapper can also start from open-event summaries and export the
intermediate witnesses before comparing:

```powershell
./scripts/compare_system_compiler_front_page_entry_opening_flow_open_event_witness_workspace.ps1 `
  -BaselineOpenEventSummaryPath cmake-build-system-compiler-front-page-entry-opening-flow-open-event-smoke/default-no-compare/front-page.entry-opening-flow.open-event.summary.json `
  -CandidateOpenEventSummaryPath cmake-build-system-compiler-front-page-entry-opening-flow-open-event-smoke/default-with-drift-compare/front-page.entry-opening-flow.open-event.summary.json `
  -OutputRoot cmake-build-open-event-witness-from-events-workspace-compare `
  -Clean
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-COMPARE-SMOKE] case=open-event-witness-self-standing verdict=standing changed=0
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-COMPARE-SMOKE] case=open-event-witness-default-to-drift-context verdict=drifted
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE-SMOKE] case=workspace-witness-summary-self-standing verdict=standing changed=0
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE-SMOKE] case=workspace-open-event-summary-to-drift-witness verdict=drifted changed=17
```

## Why this matters

The opening-flow chain now has one more audit layer:

- open event
  - records the full explainable opening judgment
- open event witness
  - distills that judgment into portable testimony
- open event witness compare
  - judges whether two testimonies still prove the same opening judgment

This is the smallest useful counterfactual for witness work.

It lets later tools ask whether a testimony drifted before they inspect the full
OpenEventRecord or assemble a larger witness bundle.
