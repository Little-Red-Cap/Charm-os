# System Compiler Front Page Entry Capability v0

`system_compiler.front_page_entry_capability/v0` is a small consumer-side
helper built on top of `system_compiler.front_page_route/v0`.

`front_page_route` says:

- what the consumer walk can reach
- where the route expands
- where it revisits
- where it closes cycles
- what extra `route_provenance` roots exist

`front_page_entry_capability` says something narrower and more practical:

- if a tool starts from one route summary
- which explain landings are already available
- which landing should be treated as the recommended default
- what capability tier this entry has already reached
- what important landings are still missing

This is meant to sit next to the explain-entry line, not replace it.

It gives upper tools a stable, machine-readable capability guardrail before
they start rendering more opinionated explain surfaces.

## Current shape

Current `system_compiler.front_page_entry_capability` includes:

- schema
  - `schemas/system_compiler.front_page_entry_capability.v0.schema.json`
- exporter
  - `scripts/export_system_compiler_front_page_entry_capability.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_capability.py`
- smoke
  - `scripts/system_compiler_front_page_entry_capability_smoke.ps1`

## Current outputs

By default the exporter leaves behind:

- `front-page.entry-capability.summary.json`
- `front-page.entry-capability.report.md`
- `front-page.entry-capability.check.txt`

## What the capability map records

The current summary records:

- the input route summary and its root surface
- one recommended entry mode:
  - `review`
  - `compare`
  - `biography`
  - `evidence`
  - `route`
- one current capability tier:
  - `review_ready`
  - `compare_ready`
  - `biography_ready`
  - `evidence_only`
  - `route_only`
- capability presence and counts for:
  - `delivery_biography`
  - `counterfactual_verdict`
  - `grouped_review`
  - `supporting_evidence`
  - `supporting_testimony`
  - `shelf_compare`
  - `candidate_shelf`
  - `baseline_shelf`
  - `route_provenance`
- one preferred route entry for each available capability
- condensed provenance hints for route-aware consumers

That makes it useful for tools that want to decide:

- whether a compare tab should appear
- whether a grouped review tab is actually backed by route evidence
- which biography landing should open first
- whether the route already exposes deeper front-page roots

## Current selection rules

Preferred entries are currently chosen by a simple consumer-friendly rule:

- smaller depth first
- non-cycle before cycle
- non-revisit before revisit
- expanded nodes preferred over leaf repeats

This is intentionally modest.

It is not trying to be the final explain-surface policy engine.

It is only trying to say:

> If a tool needs one landing for this capability right now, this is the most
> reasonable declared route entry to start from.

## Manual example

```powershell
python ./scripts/export_system_compiler_front_page_entry_capability.py `
  --summary cmake-build-system-compiler-front-page-route-smoke/root-world-compare/front-page.route.summary.json `
  --output-root cmake-build-system-compiler-front-page-entry-capability-smoke/root-world-compare
```

Then validate the exported capability object:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_capability.py `
  --summary cmake-build-system-compiler-front-page-entry-capability-smoke/root-world-compare/front-page.entry-capability.summary.json
```

Or run the dedicated smoke:

```powershell
./scripts/system_compiler_front_page_entry_capability_smoke.ps1 -Clean
```

## Why this matters

This object is useful because it moves one more decision out of tool-local
guesswork.

Before this, a tool could walk a route, but still had to infer by itself:

- is this basically a biography entry
- is this already compare-capable
- is review really present
- which path should I open first

Now that judgment is part of the artifact language itself.

That means your upcoming explain-entry consumers can stay thinner, more stable,
and less coupled to route internals.
