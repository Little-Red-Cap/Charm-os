# System Compiler Front Page Entry Opening Flow Consumer Selector v0

`system_compiler.front_page_entry_opening_flow_consumer_selector/v0` is the
reader after `system_compiler.front_page_entry_opening_flow_consumer/v0`.

The consumer handoff says which openings are renderable.

The selector answers the next practical question:

- which renderable opening should be shown first
- which compare-aware opening should stay beside it
- which fallback openings remain available
- what stable opener summaries back those choices

It is not a renderer or an interactive explain shell.

It only turns a validated consumer handoff into a deterministic open order.

## Current shape

Current `system_compiler.front_page_entry_opening_flow_consumer_selector`
includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_flow_consumer_selector.v0.schema.json`
- exporter
  - `scripts/export_system_compiler_front_page_entry_opening_flow_consumer_selector.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_consumer_selector.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opening_flow_consumer_selector_smoke.ps1`
- narrow runtime-session downstream smoke
  - `scripts/system_compiler_front_page_entry_runtime_session_opening_flow_consumer_selector_sample_smoke.ps1`
- workspace wrapper
  - `scripts/export_system_compiler_front_page_entry_opening_flow_consumer_selector_workspace.ps1`

## Current outputs

The exporter leaves behind:

- `front-page.entry-opening-flow.consumer.selector.summary.json`
- `front-page.entry-opening-flow.consumer.selector.report.md`
- `front-page.entry-opening-flow.consumer.selector.check.txt`

The default output root is:

```powershell
out/system-compiler-front-page-entry-opening-flow-consumer-selector
```

The smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-smoke
```

The workspace wrapper output root is:

```powershell
out/system-compiler-front-page-entry-opening-flow-consumer-selector-workspace
```

## What the selector records

The current summary records:

- `source_consumer`
  - copied status from the consumer handoff
- `selector_status`
  - open-plan status
  - selected entry count
  - default entry name
  - compare entry name
  - fallback entry count
- `open_plan.default_entry`
  - first normal explain opening
- `open_plan.compare_entry`
  - first compare-aware explain opening
- `open_plan.fallback_entries`
  - remaining renderable openings in priority order
- `open_plan.ordered_entries`
  - default, compare, and fallback entries in render order

Each selected entry keeps:

- target summary schema / kind / path
- projection kind
- opening reason
- projection headline, summary lines, and question lines
- query kind / scope
- compare context flag and landing verdict
- opener summary / report / check paths
- inspector readiness and blockers

## Current policy

The selector deliberately follows the existing consumer handoff instead of
recomputing opening policy:

- default entry comes from `consumer_status.default_opening_name`
- compare entry comes from `consumer_status.compare_opening_name`
- fallback entries preserve consumer handoff priority order
- entries remain renderable even when direct inspector execution is blocked

For a runtime-session-only handoff, the selector should keep
`runtime-session-sample` as the default entry and preserve
`projection_kind=kernel_runtime_session_overview` in the selected open plan.

That means a later explain surface can read this artifact without walking the
whole lower chain again.

## Manual example

Run the selector smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_consumer_selector_smoke.ps1 `
  -ConsumerWorkspaceRoot cmake-build-system-compiler-front-page-entry-opening-flow-consumer-workspace-smoke `
  -OutputRoot cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-smoke `
  -Clean
```

Or export directly:

```powershell
python ./scripts/export_system_compiler_front_page_entry_opening_flow_consumer_selector.py `
  --consumer cmake-build-system-compiler-front-page-entry-opening-flow-consumer-workspace-smoke/consumer/front-page.entry-opening-flow.consumer.summary.json `
  --output-root cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-smoke
```

Or start from a prepared front-page workspace and export the consumer workspace
and selector in one step:

```powershell
./scripts/export_system_compiler_front_page_entry_opening_flow_consumer_selector_workspace.ps1 `
  -FrontPageWorkspaceRoot cmake-build-codex-system-compiler-front-page-smoke `
  -OutputRoot cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-workspace-smoke `
  -Clean
```

Then validate:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_flow_consumer_selector.py `
  --summary cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-smoke/front-page.entry-opening-flow.consumer.selector.summary.json
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-SMOKE] selected=10 default=root-witness compare=root-witness-to-root-world-compare fallback=8 reason=delivery_biography projection_summary=3 projection_questions=2
```

Expected narrow runtime-session downstream shape:

```text
[FRONT-PAGE-ENTRY-RUNTIME-SESSION-CONSUMER-SELECTOR-SAMPLE-SMOKE] default=runtime-session-sample projection=kernel_runtime_session_overview
```

## Why this matters

`front_page_entry_opening_flow_consumer` makes the opening handoff consumable.

`front_page_entry_opening_flow_consumer_selector` makes it directly selectable.

This gives later explain tooling a small, deterministic first-read artifact:
open this default entry, keep this compare entry nearby, and preserve these
fallbacks without rebuilding the lower opening chain.
