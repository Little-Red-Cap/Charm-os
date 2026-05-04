# System Compiler Front Page Entry Opening Flow Consumer Plan v0

`system_compiler.front_page_entry_opening_flow_consumer_plan/v0` is the
execution-plan object after
`system_compiler.front_page_entry_opening_flow_consumer_selector/v0`.

The selector says which renderable explain entries should be opened first.

The consumer plan answers the next practical question:

- which opener action should run as the default entry
- which compare-aware opener should stay nearest to it
- which fallback opener actions should be eagerly prepared next
- which stable opener summaries back those actions

It is not a renderer, UI shell, or new route language.

It only turns a validated selector artifact into a deterministic action list
that a later explain consumer can execute.

## Current shape

Current `system_compiler.front_page_entry_opening_flow_consumer_plan` includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_flow_consumer_plan.v0.schema.json`
- exporter
  - `scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan.py`
- workspace exporter
  - `scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan_workspace.ps1`
- action facade
  - `scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py`
  - `scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace.ps1`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_consumer_plan.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_action_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_sample_smoke.ps1`

## Current outputs

The exporter leaves behind:

- `front-page.entry-opening-flow.consumer.plan.summary.json`
- `front-page.entry-opening-flow.consumer.plan.report.md`
- `front-page.entry-opening-flow.consumer.plan.check.txt`

The default output root is:

```powershell
out/system-compiler-front-page-entry-opening-flow-consumer-plan
```

The smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-smoke
```

The workspace exporter writes under:

```powershell
out/system-compiler-plan-ws
```

The action facade smoke writes under:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-smoke
```

The action facade workspace smoke writes under:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke
```

## What the consumer plan records

The current summary records:

- `source_selector`
  - copied status from the consumer selector
- `planner_status`
  - execution plan status
  - planned action count
  - default action entry name
  - compare action entry name
  - next action count
  - omitted entry count
- `execution_plan.default_action`
  - the first opener action a consumer should execute
- `execution_plan.compare_action`
  - the compare-aware opener action when it is distinct from default
- `execution_plan.next_actions`
  - eagerly prepared fallback actions
- `execution_plan.action_entries`
  - all planned actions in execution order
- `planning_surface`
  - planned action ids
  - planned and omitted entry names
  - projection-kind and target-schema counters
  - compare-context and inspector-ready counters

Each action keeps:

- action id / kind / rank
- source selector rank
- entry name and display group
- selected tab, role, query kind, and query scope
- target summary schema / kind / path
- projection kind and compare-context flag
- opening reason and projection headline
- opener summary / report / check paths
- inspector readiness and blockers

## Current policy

The plan deliberately follows the selector instead of recomputing selection
policy:

- `open-default` follows the selector default entry
- `open-compare-neighbor` follows the selector compare entry when it is
  distinct from default
- `open-next-*` follows selector order for remaining fallback entries
- fallback eager planning is capped at three `next` actions
- omitted entries remain recoverable from the source selector artifact

For a runtime-session-only selector, the plan should produce one
`open-default` action with `entry_name=runtime-session-sample`,
`selected_tab_id=runtime_session`, and
`projection_kind=kernel_runtime_session_overview`.

That keeps this layer small.

The plan is a consumer-side action bundle, not a second selector and not a UI
state machine.

The next consumer seam is
`system_compiler.front_page_entry_opening_flow_consumer_plan_action/v0`.

That object selects one already-planned action and records the exact opener
summary a later explain surface should open now.

## Manual example

Run the consumer plan smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_smoke.ps1 `
  -SelectorWorkspaceRoot cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-workspace-smoke `
  -OutputRoot cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-smoke `
  -Clean
```

Or export directly from a prepared front-page workspace by letting the wrapper
produce the selector witness first:

```powershell
./scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan_workspace.ps1 `
  -FrontPageWorkspaceRoot cmake-build-codex-system-compiler-front-page-smoke `
  -OutputRoot cmake-build-plan-ws-smoke `
  -Clean
```

Or export directly:

```powershell
python ./scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan.py `
  --selector cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-workspace-smoke/selector/front-page.entry-opening-flow.consumer.selector.summary.json `
  --output-root cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-smoke
```

Then validate:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_flow_consumer_plan.py `
  --summary cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-smoke/front-page.entry-opening-flow.consumer.plan.summary.json
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-SMOKE] actions=5 default=root-witness compare=root-witness-to-root-world-compare next=3 omitted=5 reason=delivery_biography
```

Expected narrow runtime-session shape:

```text
[FRONT-PAGE-ENTRY-RUNTIME-SESSION-PLAN-ACTION-SAMPLE-SMOKE] action_id=open-default entry=runtime-session-sample projection=kernel_runtime_session_overview
```

Run the single-action facade smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_action_smoke.ps1 -Clean
```

Run the single-action workspace facade smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace_smoke.ps1 -Clean
```

Run the narrow runtime-session plan/action sample:

```powershell
./scripts/system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_sample_smoke.ps1 -Clean
```

## Why this matters

`front_page_entry_opening_flow_consumer_selector` makes the opening handoff
selectable.

`front_page_entry_opening_flow_consumer_plan` makes it executable.

This gives later explain tooling a tiny first-read artifact:

- open this default opener
- keep this compare opener near it
- prepare these next opener actions
- defer the rest without losing provenance

The action facade then lets a consumer open exactly one of those opener
witnesses without reconstructing plan selection policy.

That preserves the artifact chain while moving the system one step closer to a
real consumer entry point.
