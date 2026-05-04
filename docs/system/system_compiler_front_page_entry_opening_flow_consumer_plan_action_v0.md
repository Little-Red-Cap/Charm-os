# System Compiler Front Page Entry Opening Flow Consumer Plan Action v0

`system_compiler.front_page_entry_opening_flow_consumer_plan_action/v0` is the
single-action facade after
`system_compiler.front_page_entry_opening_flow_consumer_plan/v0`.

The plan says:

- which opener actions exist
- which action is default
- which compare-aware action should stay near it
- which fallback actions are eagerly prepared

The action facade answers one smaller consumer question:

- which one action should an explain consumer open now

It is not a new selector, planner, renderer, or inspector wrapper.

It only picks one already-planned action and records a stable opening witness
for downstream explain tools.

## Current shape

Current `system_compiler.front_page_entry_opening_flow_consumer_plan_action`
includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_flow_consumer_plan_action.v0.schema.json`
- exporter
  - `scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py`
- workspace exporter
  - `scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace.ps1`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_action_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace_smoke.ps1`

## Current outputs

The exporter leaves behind:

- `front-page.entry-opening-flow.consumer.plan-action.summary.json`
- `front-page.entry-opening-flow.consumer.plan-action.report.md`
- `front-page.entry-opening-flow.consumer.plan-action.check.txt`

The default output root is:

```powershell
out/system-compiler-front-page-entry-opening-flow-consumer-plan-action
```

The smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-smoke
```

The workspace exporter writes under:

```powershell
out/system-compiler-plan-action-ws
```

The workspace smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke
```

## What the action facade records

The current summary records:

- `selection_request`
  - requested action id, action kind, or entry name
  - effective selector used by the exporter
  - matched action count
- `source_plan`
  - source plan result and ready status
  - planned action count
  - default and compare action ids/names
- `selected_action`
  - the original plan action copied without recomputing policy
- `open_action`
  - the normalized action a consumer should execute now
- `opener_surface`
  - the selected opener summary/report/check surface
- `execution_receipt`
  - consumer operation, selected rank, source rank, selector, and inspector readiness

The consumer operation is intentionally fixed:

```text
open-opener-summary
```

That keeps this layer narrow.

Later explain tools can open the selected opener witness, then decide whether
to render the opener projection, route deeper, or wait for a richer UI shell.

## Selection policy

The exporter accepts exactly one selector:

- no selector
  - selects the plan default action
- `--action-id`
  - selects the matching action id
- `--action-kind`
  - selects the first matching action kind
- `--entry-name`
  - selects the matching entry name

This facade does not invent a new priority order.

It either follows the plan default or follows the explicit consumer request.

## Manual example

Run the action smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_action_smoke.ps1 -Clean
```

Run the workspace action smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace_smoke.ps1 -Clean
```

Or export through the workspace wrapper from a prepared front-page workspace:

```powershell
./scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace.ps1 `
  -FrontPageWorkspaceRoot cmake-build-codex-system-compiler-front-page-smoke `
  -OutputRoot cmake-build-plan-action-ws-smoke `
  -Clean
```

Or reuse an already materialized plan workspace:

```powershell
./scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace.ps1 `
  -PlanWorkspaceRoot cmake-build-plan-action-ws-smoke/plan-ws `
  -OutputRoot cmake-build-plan-action-ws-hot-smoke `
  -ActionKind compare-neighbor
```

Or export a default action directly:

```powershell
python ./scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py `
  --plan cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-workspace-smoke/plan/front-page.entry-opening-flow.consumer.plan.summary.json `
  --output-root cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-smoke/default
```

Or export the compare-neighbor action:

```powershell
python ./scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py `
  --plan cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-workspace-smoke/plan/front-page.entry-opening-flow.consumer.plan.summary.json `
  --action-kind compare-neighbor `
  --output-root cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-smoke/compare-neighbor
```

Then validate:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py `
  --summary cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-smoke/default/front-page.entry-opening-flow.consumer.plan-action.summary.json
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-SMOKE] case=default selector=default_action action=open-default kind=default
[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-SMOKE] case=compare-neighbor selector=action_kind:compare-neighbor action=open-compare-neighbor kind=compare-neighbor
```

## Why this matters

The opening-flow consumer chain now has three increasingly concrete layers:

- selector
  - decide which explain entries matter first
- plan
  - turn that order into a small execution bundle
- action
  - choose one deterministic opener witness to execute now

That gives a future explain surface a tiny contract:

- consume one action summary
- open its `opener_surface.summary_path`
- show its `open_action` and `execution_receipt`
- do not reconstruct selector or plan policy from raw JSON

This is one more step toward a system that can explain how it should be opened,
not just what it contains.
