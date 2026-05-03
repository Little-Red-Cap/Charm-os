# System Compiler Front Page Entry Opening Flow v0

`system_compiler.front_page_entry_opening_flow/v0` is the smoke-level consumer
artifact after:

- `system_compiler.front_page_route/v0`
- `system_compiler.front_page_entry_capability/v0`
- `system_compiler.front_page_entry_landing/v0`
- `system_compiler.front_page_entry_landing_compare/v0`
- `system_compiler.front_page_entry_opener/v0`

The earlier objects answer:

- what a consumer can reach
- which explain landings exist
- which tab should open first
- how two landing plans drift
- what deterministic open action and immediate preview a tool should use

`front_page_entry_opening_flow` answers one more practical question:

- did the whole consumer-side opening chain still close end to end
- how many opener cases actually landed
- how many projections stayed available
- how many openings still preserve compare context
- which flow steps produced the current opening evidence set

This is still intentionally modest.

It is not yet a rich interactive explain shell.

It is a smoke-level evidence object that says the current front-page opening
language still stands as one continuous consumer path.

## Current shape

Current `system_compiler.front_page_entry_opening_flow` includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_flow.v0.schema.json`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_flow.py`
- smoke / exporter
  - `scripts/system_compiler_front_page_entry_opening_flow_smoke.ps1`
- workspace wrapper
  - `scripts/export_system_compiler_front_page_entry_opening_flow_workspace.ps1`

## Current outputs

The smoke currently leaves behind:

- `front-page.entry-opening-flow.summary.json`
- `front-page.entry-opening-flow.report.md`
- `front-page.entry-opening-flow.check.txt`

The default output root is:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-smoke
```

When the flow is exported from an already prepared multi-case front-page
workspace, the default wrapper output root is:

```powershell
out/system-compiler-front-page-entry-opening-flow-workspace
```

## What the flow records

The current summary records:

- one `flow_status` block with:
  - expected opener count
  - actual opener count
  - available projection count
  - compare-context count
  - inspector-ready count
  - blocked-inspector count
  - completed-step count
- ordered `flow_steps` for:
  - entry capability
  - entry landing
  - entry landing compare
  - entry opener
- one `opener_cases` list that projects each opener summary into:
  - selected tab / role
  - query kind / scope
  - target summary schema / kind / path
  - projection status / kind
  - compare-context availability and verdict
  - inspector readiness and blockers
- a `front_page.supporting_surfaces` list that points back to every exported
  opener case summary / report / check artifact

That means this object is not replacing the lower artifacts.

It is summarizing whether the whole opening seam still holds together.

## Why it exists

The earlier `front_page_*` artifacts already made the opening policy much
thinner.

But without one higher-level evidence object, a consumer or CI step would still
have to reassemble by hand:

- which smoke roots were used
- which opening steps completed
- how many opener cases actually landed
- whether all opener projections stayed renderable
- whether compare-aware openings still survived the chain

`front_page_entry_opening_flow` turns that into one smaller artifact that can
be asked:

- does the opening chain stand
- where does it break first
- which opener case drifted
- is the failure in routing, landing, compare, or opener projection

## Current boundary

This object intentionally stays at smoke-level evidence.

It does not:

- replace the lower `front_page_entry_opener` summaries
- invent richer explain-entry rendering policy
- claim inspector execution is safe when `inspector_ready=false`
- flatten target-summary-specific semantics into one fake universal schema

Instead it does the narrower thing:

- preserve the step outputs
- preserve the case-level opener evidence
- preserve enough counters and links for CI or later consumers to reason about
  the opening chain

## Manual example

Run the full consumer-side opening chain and emit the flow artifact:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_smoke.ps1 -Clean
```

Or start from a front-page workspace that already contains the canonical
multi-case witness worlds, route traces, and shelf review evidence:

```powershell
./scripts/export_system_compiler_front_page_entry_opening_flow_workspace.ps1 `
  -FrontPageWorkspaceRoot cmake-build-codex-system-compiler-front-page-smoke `
  -OutputRoot cmake-build-system-compiler-front-page-entry-opening-flow-workspace-smoke `
  -Clean
```

Then validate the exported flow object:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_flow.py `
  --summary cmake-build-system-compiler-front-page-entry-opening-flow-smoke/front-page.entry-opening-flow.summary.json
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] openers=10 projections=10 compare_context=2 inspector_ready=0
```

## Why this matters

This object is useful because it creates a stable handoff seam between:

- lower opening-policy artifacts
- later explain-surface consumers
- route-aware smoke / CI evidence

Instead of asking later tools to inspect:

- route smoke outputs
- capability smoke outputs
- landing smoke outputs
- compare smoke outputs
- opener smoke outputs

one by one, a consumer can start from one artifact that already says:

- these are the completed opening steps
- these are the current opener cases
- these openings still have renderable projections
- these openings still preserve compare drift
- these openings are still blocked from direct inspector execution

That keeps later work closer to "consume the evidence artifact" and further
from "rebuild the smoke narrative from scattered folders".
