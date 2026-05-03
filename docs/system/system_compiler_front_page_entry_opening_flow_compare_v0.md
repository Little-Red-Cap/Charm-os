# System Compiler Front Page Entry Opening Flow Compare v0

`system_compiler.front_page_entry_opening_flow_compare/v0` compares two
`system_compiler.front_page_entry_opening_flow/v0` summaries.

It answers one question:

- did the consumer-side opening chain preserve the same opener case surface

The compare object does not rerun the lower smoke chain.

It consumes two existing opening-flow summaries and leaves a deterministic
compare witness.

## Current shape

Current `system_compiler.front_page_entry_opening_flow_compare` includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_flow_compare.v0.schema.json`
- exporter
  - `scripts/compare_system_compiler_front_page_entry_opening_flow.py`
- workspace compare wrapper
  - `scripts/compare_system_compiler_front_page_entry_opening_flow_workspace.ps1`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_compare.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opening_flow_compare_smoke.ps1`

## Current outputs

By default the compare exporter leaves behind:

- `front-page.entry-opening-flow.compare.summary.json`
- `front-page.entry-opening-flow.compare.report.md`
- `front-page.entry-opening-flow.compare.check.txt`

The dedicated smoke writes under:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-compare-smoke
```

The workspace compare wrapper writes under:

```powershell
out/system-compiler-front-page-entry-opening-flow-workspace-compare
```

## What the compare records

The current summary records:

- baseline and candidate opening-flow summary paths
- baseline and candidate flow provenance
- flow-level count deltas
- flow step id changes
- opener case additions and removals
- opener case changes for selected tab, query, target, projection, compare
  context, and inspector readiness
- regression surface narratives
- a verdict:
  - `standing`
  - `improved`
  - `drifted`
  - `collapsed`

## Current smoke evidence

The smoke currently proves two paths:

- `self-standing`
  - compares the flow summary with itself
  - expected verdict: `standing`
- `removed-compare-opener`
  - removes `root-witness-to-root-world-compare` from a synthetic candidate
  - expected verdict: `drifted`

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-FLOW-COMPARE-SMOKE] case=self-standing verdict=standing changed=0 added=0 removed=0
[FRONT-PAGE-ENTRY-OPENING-FLOW-COMPARE-SMOKE] case=removed-compare-opener verdict=drifted changed=0 added=0 removed=1
```

## Boundary

This object intentionally stays at compare level.

It does not:

- run the opening-flow smoke
- rebuild opener summaries
- execute inspector commands
- decide which explain surface should render next

It only judges whether two opening-flow witnesses expose the same consumer
opening surface.

For workspace-exported flows, target summary paths are compared relative to
each flow output root. That keeps `baseline_opening_flow/` and
`candidate_opening_flow/` scratch directories from looking like consumer-facing
drift while still preserving the original absolute paths in provenance fields.

## Manual example

Generate the base opening-flow witness:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_smoke.ps1 -Clean
```

Run the compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_compare_smoke.ps1 -Clean
```

Or compare two explicit summaries:

```powershell
python ./scripts/compare_system_compiler_front_page_entry_opening_flow.py `
  --baseline cmake-build-system-compiler-front-page-entry-opening-flow-smoke/front-page.entry-opening-flow.summary.json `
  --candidate cmake-build-system-compiler-front-page-entry-opening-flow-smoke/front-page.entry-opening-flow.summary.json `
  --output-root cmake-build-system-compiler-front-page-entry-opening-flow-compare-smoke/self-standing
```

Or compare two prepared front-page workspaces by exporting both opening-flow
witnesses first and then comparing them:

```powershell
./scripts/compare_system_compiler_front_page_entry_opening_flow_workspace.ps1 `
  -BaselineFrontPageWorkspaceRoot cmake-build-codex-system-compiler-front-page-smoke `
  -CandidateFrontPageWorkspaceRoot cmake-build-codex-system-compiler-front-page-smoke `
  -OutputRoot cmake-build-system-compiler-front-page-entry-opening-flow-workspace-compare-smoke `
  -Clean
```

Validate the compare object:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_flow_compare.py `
  --summary cmake-build-system-compiler-front-page-entry-opening-flow-compare-smoke/self-standing/front-page.entry-opening-flow.compare.summary.json
```

## Why this matters

`front_page_entry_opening_flow/v0` made the opening chain self-describing.

`front_page_entry_opening_flow_compare/v0` makes it interrogable across time.

Instead of asking a later tool to diff smoke folders, it can consume one object
that says:

- did opener coverage change
- did a projection disappear
- did compare context get lost
- did the inspector readiness boundary drift
- which opener case changed first

That keeps the front-page explain line closer to a real witness world: every
opening surface can now be opened, summarized, and compared.
