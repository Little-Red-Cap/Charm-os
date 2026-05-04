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
- exporter
  - `scripts/export_system_compiler_front_page_entry_opener.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opener.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opener_smoke.ps1`

## Current outputs

By default the exporter leaves behind:

- `front-page.entry-opener.summary.json`
- `front-page.entry-opener.report.md`
- `front-page.entry-opener.check.txt`

## What the opener records

The current summary records:

- the source landing summary path
- an optional source landing-compare summary path
- a compact `source_landing` projection
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
- target summary schema / kind / path
- target report markdown path
- target check text path
- follow-up query kinds
- blockers, if the landing cannot produce a deterministic open action

`opened_projection` is the first real consumer proof.

It lets the opener say:

- this target is a `biography`, `world_compare`, `world_shelf_review`,
  `biography_index`, `biography_index_compare`, `witness_bundle`, or
  `runtime_evidence_bundle`
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
- show this summary/report/check target
- render this small `opened_projection` immediately if available
- only execute this inspector invocation if `ready=true`
- otherwise explain the blocker instead of guessing

That keeps the explain surface closer to "consume the artifact witness" and
further from "reconstruct policy from script folklore".
