# System Compiler Front Page Entry Opening Flow Consumer v0

`system_compiler.front_page_entry_opening_flow_consumer/v0` is the thin
handoff object after `system_compiler.front_page_entry_opening_flow/v0`.

The opening flow proves that the consumer-side chain still closes:

- route
- capability
- landing
- landing compare
- opener

The consumer object answers the next practical question:

- which opening should a higher explain tool render first
- which compare-aware opening should stay nearby
- which openings are already renderable from existing projections
- why direct artifact-report inspector execution is still blocked

It deliberately stays below a full explain surface.

It does not invent a renderer, query shell, or new route language. It consumes
one opening-flow summary and exports a stable entry list that another tool can
open.

## Current shape

Current `system_compiler.front_page_entry_opening_flow_consumer` includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_flow_consumer.v0.schema.json`
- exporter
  - `scripts/export_system_compiler_front_page_entry_opening_flow_consumer.py`
- workspace wrapper
  - `scripts/export_system_compiler_front_page_entry_opening_flow_consumer_workspace.ps1`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_consumer.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opening_flow_consumer_smoke.ps1`

## Current outputs

The exporter leaves behind:

- `front-page.entry-opening-flow.consumer.summary.json`
- `front-page.entry-opening-flow.consumer.report.md`
- `front-page.entry-opening-flow.consumer.check.txt`

The default output root is:

```powershell
out/system-compiler-front-page-entry-opening-flow-consumer
```

The smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-consumer-smoke
```

The workspace wrapper output root is:

```powershell
out/system-compiler-front-page-entry-opening-flow-consumer-workspace
```

## What the consumer records

The current summary records:

- `source_flow`
  - copied counters from the source opening-flow summary
- `consumer_status`
  - total opening count
  - renderable opening count
  - ready open-action count
  - compare-aware opening count
  - inspector-ready and blocked-inspector counts
  - default opening name
  - compare opening name
- `default_opening`
  - the first stable opening a consumer should show as the normal preview
- `compare_opening`
  - the first stable compare-aware opening a consumer should keep nearby
- `readiness_surface`
  - projection-kind counts
  - target-summary-schema counts
  - blocked inspector reason counts
  - renderable / blocked opening names
- `opening_handoff_entries`
  - the ordered case-level handoff list derived from source `opener_cases`

## Current policy

The consumer keeps policy intentionally small:

- an opening is renderable when its open action is ready, projection is
  available, target summary exists, and opener summary exists
- `root-witness` is preferred as the default opening when renderable
- compare-aware openings are selected from cases that preserved compare context
- inspector readiness remains separate from renderability

That last point is important.

A biography, witness bundle, world compare, shelf review, or runtime evidence
summary can be renderable through an opener projection while still being
blocked from direct `inspect_system_compiler_artifact_report.ps1` execution.

## Manual example

Run the consumer smoke from an existing opening-flow workspace:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_consumer_smoke.ps1 `
  -InputRoot cmake-build-system-compiler-front-page-entry-opening-flow-workspace-smoke `
  -OutputRoot cmake-build-system-compiler-front-page-entry-opening-flow-consumer-smoke `
  -Clean
```

Or export directly:

```powershell
python ./scripts/export_system_compiler_front_page_entry_opening_flow_consumer.py `
  --flow cmake-build-system-compiler-front-page-entry-opening-flow-workspace-smoke/front-page.entry-opening-flow.summary.json `
  --output-root cmake-build-system-compiler-front-page-entry-opening-flow-consumer-smoke
```

Or start from a prepared front-page workspace and export both the opening-flow
witness and consumer handoff in one step:

```powershell
./scripts/export_system_compiler_front_page_entry_opening_flow_consumer_workspace.ps1 `
  -FrontPageWorkspaceRoot cmake-build-codex-system-compiler-front-page-smoke `
  -OutputRoot cmake-build-system-compiler-front-page-entry-opening-flow-consumer-workspace-smoke `
  -Clean
```

Then validate:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_flow_consumer.py `
  --summary cmake-build-system-compiler-front-page-entry-opening-flow-consumer-smoke/front-page.entry-opening-flow.consumer.summary.json
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SMOKE] openings=10 renderable=10 compare_aware=2 default=root-witness
```

## Why this matters

`front_page_entry_opening_flow` says that the opening chain still stands.

`front_page_entry_opening_flow_consumer` says how a later consumer should begin
using that chain.

This gives higher explain tooling a stable handoff artifact without forcing it
to recalculate opener readiness, projection availability, compare context, or
inspector blockers from scattered lower-level summaries.
