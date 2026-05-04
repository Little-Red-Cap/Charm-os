# System Compiler Front Page Entry Opener v0

`system_compiler.front_page_entry_opener/v0` is the consumer-side facade after
`system_compiler.front_page_entry_landing/v0` and, optionally,
`system_compiler.front_page_entry_landing_compare/v0`.

The landing object answers:

- which tab should open first
- which query hint belongs to that tab
- which summary / report / check target belongs to the selected entry

The opener object answers one more practical question:

- what deterministic explain open action should a tool perform now

It also records:

- a small `opened_projection` that proves what a consumer can already read from
  the selected front-page target without inventing a deeper query engine
- whether that open action can already be expressed as a safe
  `inspect_system_compiler_artifact_report.ps1` invocation

## Current shape

Current `system_compiler.front_page_entry_opener` includes:

- schema
  - `schemas/system_compiler.front_page_entry_opener.v0.schema.json`
  - `schemas/system_compiler.front_page_entry_opener_compare.v0.schema.json`
- exporter
  - `scripts/export_system_compiler_front_page_entry_opener.py`
  - `scripts/export_system_compiler_front_page_entry_opener_workspace.ps1`
- compare
  - `scripts/compare_system_compiler_front_page_entry_opener.py`
  - `scripts/compare_system_compiler_front_page_entry_opener_workspace.ps1`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opener.py`
  - `scripts/validate_system_compiler_front_page_entry_opener_compare.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opener_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opener_open_event_witness_compare_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opener_workspace_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opener_workspace_compare_smoke.ps1`

## Current outputs

By default the exporter leaves behind:

- `front-page.entry-opener.summary.json`
- `front-page.entry-opener.report.md`
- `front-page.entry-opener.check.txt`

The PowerShell workspace facade wraps the same exporter and validator, resolves
either direct summary paths or workspace roots, and writes the opener artifacts
under:

- `out/system-compiler-front-page-entry-opener-workspace/opener/`

This facade is intentionally thin. It does not reinterpret landing policy; it
only gives downstream workspace tools one stable entry point for:

- a landing workspace root or explicit landing summary
- an optional landing-compare workspace root or explicit landing-compare summary
- the validated opener summary/report/check output root

## What the opener records

The current summary records:

- the source landing summary path
- an optional source landing-compare summary path
- a compact `source_landing` projection
- the landing-provided `opening_reason`
- a compact `compare_context` projection when compare input exists
- one `open_action`
- one `opened_projection`
- one `inspector_invocation`
- follow-up questions for the next consumer seam

`open_action` is the stable consumer contract.

It contains:

- selected tab id / title / route / surface / role
- query kind
- query scope
- selection rule
- compare expectation
- opening reason
- target summary schema / kind / path
- target report markdown path
- target check text path
- follow-up query kinds
- blockers, if the landing cannot produce a deterministic open action

`opened_projection` is the first real consumer proof.

It lets the opener say:

- this target is a `biography`, `world_compare`, `world_shelf_review`,
  `biography_index`, `biography_index_compare`, `witness_bundle`,
  `runtime_evidence_bundle`, or `open_event_witness_compare`
- here is the smallest stable overview a consumer can already render now
- here are the most relevant supporting / evidence / compare paths that stay
  nearest to that opening

That means the opener is no longer only an "action plan" object.

It is also a very thin "opening result preview" object.

## Inspector invocation boundary

The opener does not execute the inspector.

It also does not invent new inspector flags.

The only query-to-flag mapping currently allowed is:

- `default_overview`
  - no query-specific switch
- `bringup_evidence`
  - `-BringupEvidence`
- `resource_summary`
  - `-ResourceSummary`
- `cap_list`
  - `-CapList`

Every ready inspector invocation also requests `-AsJson`.

If the landing target is not an actual
`system_compiler.artifact_report/v0` report, the opener keeps the open action
ready but marks `inspector_invocation.ready=false`.

That distinction is intentional.

Many current front-page entries point at higher-level summaries such as:

- `system_compiler.biography/v0`
- `system_compiler.world_compare/v0`
- `system_compiler.witness_bundle/v0`

Those are valid explain targets, but they are not safe `-Report` inputs for
`inspect_system_compiler_artifact_report.ps1`.

So the opener records:

- the deterministic target summary/report/check to open
- the smallest stable `opened_projection` it can already derive from that target
- why a direct inspector invocation is blocked

instead of hand-rolling an unsafe inspector argument strategy.

## Compare context

When a landing compare summary is provided, the opener verifies that the
compare references the source landing as either:

- `baseline_landing`
- `candidate_landing`

Then it preserves the opening-relevant compare fields:

- landing verdict
- whether the primary query changed
- whether landing regression changed
- whether query regression changed
- affected tab ids
- short narratives

This lets downstream tools open the candidate or baseline deterministically
while still showing the drift context that made this opening interesting.

## Opener compare

`system_compiler.front_page_entry_opener_compare/v0` compares two opener
facades as opening judgments.

It is intentionally not a raw directory diff.

The compare focuses on consumer-visible opener semantics:

- open action status, selected tab, query, target, and opening reason
- compare context availability and landing verdict
- opened projection status, kind, headline, summary lines, and evidence paths
- inspector readiness and blockers
- follow-up question drift

The current verdicts are:

- `standing`
  - the opener judgment is unchanged
- `improved`
  - the candidate keeps a ready action and gains useful opener context, such as
    landing compare context
- `drifted`
  - the candidate still opens but loses context or changes the judgment surface
- `collapsed`
  - the candidate no longer produces an `ok` ready opener action

The workspace wrapper resolves either direct opener summary paths or opener
workspace roots and writes:

- `front-page.entry-opener.compare.summary.json`
- `front-page.entry-opener.compare.report.md`
- `front-page.entry-opener.compare.check.txt`

## Opened projection boundary

The opener still does not try to become the full explain engine.

The new `opened_projection` stays intentionally modest.

Today it only recognizes targets that already have stable, consumer-facing
summary shapes in the repository:

- `system_compiler.biography/v0`
- `system_compiler.world_compare/v0`
- `system_compiler.world_shelf_review/v0`
- `system_compiler.biography_index/v0`
- `system_compiler.biography_index_compare/v0`
- `system_compiler.witness_bundle/v0`
- `minimal_kernel.runtime_evidence_bundle.summary/v1`
- `system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0`

For those targets it records:

- one `projection_kind`
- one headline
- short summary lines
- nearby question lines
- nearby supporting / evidence / compare paths

For `system_compiler.world_shelf_review/v0`, those summary lines include a
single `drift_digest ...` line. This is only a consumer preview of the review
object's own `drift_digest`; the opener does not re-run shelf compare logic or
reinterpret lower `biography_index_compare` semantics.

For
`system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0`,
the projection kind is `open_event_witness_compare_overview`.

It exposes:

- the witness verdict and changed-field count in the headline
- baseline and candidate witness ids, status, and source open-event ids
- a compact change-count line
- up to three `witness_drift ...` narratives
- baseline and candidate OpenEventWitness summaries as evidence paths

This lets an opener explain "why this witness compare is interesting" without
opening the full witness compare report first.

The opener also prepends one `opening_reason ...` summary line from the source
landing.

That reason is pass-through state. The opener does not re-score capabilities,
reselect tabs, or reinterpret `drift_digest`; it only makes the already selected
opening reason visible beside the preview.

If the target exists but the opener does not know how to project it yet,
`opened_projection.status` becomes `unavailable` and the blocker explains which
summary schema still needs a projection adapter.

## Manual example

```powershell
python ./scripts/export_system_compiler_front_page_entry_opener.py `
  --landing cmake-build-system-compiler-front-page-entry-landing-smoke/root-world-compare/front-page.entry-landing.summary.json `
  --output-root cmake-build-system-compiler-front-page-entry-opener-smoke/root-world-compare
```

With compare context:

```powershell
python ./scripts/export_system_compiler_front_page_entry_opener.py `
  --landing cmake-build-system-compiler-front-page-entry-landing-smoke/root-world-compare/front-page.entry-landing.summary.json `
  --landing-compare cmake-build-system-compiler-front-page-entry-landing-compare-smoke/root-witness-to-root-world-compare/front-page.entry-landing.compare.summary.json `
  --output-root cmake-build-system-compiler-front-page-entry-opener-smoke/root-witness-to-root-world-compare
```

Then validate the exported opener object:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opener.py `
  --summary cmake-build-system-compiler-front-page-entry-opener-smoke/root-witness-to-root-world-compare/front-page.entry-opener.summary.json
```

Or run the dedicated smoke:

```powershell
./scripts/system_compiler_front_page_entry_opener_smoke.ps1 -Clean
```

To prove only the OpenEventWitnessCompare projection adapter:

```powershell
./scripts/system_compiler_front_page_entry_opener_open_event_witness_compare_smoke.ps1 -Clean
```

To export the same opener object through the reusable workspace facade:

```powershell
./scripts/export_system_compiler_front_page_entry_opener_workspace.ps1 `
  -LandingWorkspaceRoot cmake-build-system-compiler-front-page-entry-landing-smoke/root-world-compare `
  -LandingCompareWorkspaceRoot cmake-build-system-compiler-front-page-entry-landing-compare-smoke/root-witness-to-root-world-compare `
  -OutputRoot out/system-compiler-front-page-entry-opener-workspace `
  -Clean
```

To prove the workspace facade without depending on a pre-existing front-page
smoke directory:

```powershell
./scripts/system_compiler_front_page_entry_opener_workspace_smoke.ps1 -Clean
```

That smoke builds synthetic but schema-valid landing fixtures, uses the real
landing compare exporter to create an `improved` compare context, and then
checks both:

- a cold opener workspace with no compare context
- a hot opener workspace with candidate landing compare context

Compare two opener workspaces:

```powershell
./scripts/compare_system_compiler_front_page_entry_opener_workspace.ps1 `
  -BaselineOpenerWorkspaceRoot cmake-build-system-compiler-front-page-entry-opener-workspace-smoke/cold-runtime-evidence `
  -CandidateOpenerWorkspaceRoot cmake-build-system-compiler-front-page-entry-opener-workspace-smoke/hot-runtime-evidence-with-landing-compare `
  -OutputRoot cmake-build-opener-workspace-compare `
  -Clean
```

Run the opener workspace compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opener_workspace_compare_smoke.ps1 -Clean
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE-SMOKE] case=workspace-self-standing verdict=standing changed=0 compare_changed=False projection_changed=False improved=False regressed=False
[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE-SMOKE] case=workspace-cold-to-hot-compare-context verdict=improved changed=10 compare_changed=True projection_changed=True improved=True regressed=False
[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE-SMOKE] case=workspace-hot-to-cold-lost-compare-context verdict=drifted changed=10 compare_changed=True projection_changed=True improved=False regressed=True
```

To run the full entry-opening flow from capability through landing, landing
compare, and opener:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_smoke.ps1 -Clean
```

## Why this matters

This object is useful because it removes the last bit of hand-rolled opening
policy from later explain tools.

Instead of consuming:

- landing tabs
- query hints
- optional compare drift
- target-summary-specific preview rules
- inspector script parameter rules
- target summary shape guesses

a tool can consume one smaller object that already says:

- open this tab
- use this query
- explain why this opening won
- show this summary/report/check target
- render this small `opened_projection` immediately if available
- only execute this inspector invocation if `ready=true`
- otherwise explain the blocker instead of guessing

That keeps the explain surface closer to "consume the artifact witness" and
further from "reconstruct policy from script folklore".
