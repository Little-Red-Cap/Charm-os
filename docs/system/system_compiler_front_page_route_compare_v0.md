# System Compiler Front Page Route Compare v0

`system_compiler.front_page_route_compare/v0` is the next consumer-side object
after `system_compiler.front_page_route/v0`.

`front_page_route` answers:

- starting from one root summary
- which declared `front_page` surfaces become reachable
- where the route expands
- where it revisits
- where it closes a real cycle

`front_page_route_compare` answers a different question:

- if we compare two already-exported route summaries
- how did the consumer walk change
- which level-1 surfaces appeared or disappeared
- whether key surfaces stayed reachable
- whether the candidate route got richer, drifted, or collapsed

It still stays on the consumer side.

It does not reopen producer internals and it does not diff arbitrary raw JSON.

## Current shape

Current `system_compiler.front_page_route_compare` includes:

- schema
  - `schemas/system_compiler.front_page_route_compare.v0.schema.json`
- exporter
  - `scripts/compare_system_compiler_front_page_route.py`
- validator
  - `scripts/validate_system_compiler_front_page_route_compare.py`
- smoke
  - `scripts/system_compiler_front_page_route_compare_smoke.ps1`

## Current outputs

By default the comparer leaves behind:

- `front-page.route.compare.summary.json`
- `front-page.route.compare.report.md`
- `front-page.route.compare.check.txt`

## What the compare records

The compare summary currently records:

- baseline and candidate route provenance
- root route status on both sides
- level-1 surface drift
- reachable surface / role drift
- schema-count and role-count drift
- cycle / revisit / expanded-anchor drift
- route-entry changes anchored by semantic route path, not raw route id
- a `route_regression_surface` that highlights the smallest consumer-facing
  breakage surface

That anchor choice matters.

The compare object intentionally does not key entries by the generated
`route_id`, because `route_id` is traversal-order local.

Instead it compares entries by a semantic route anchor built from:

- parent route anchor
- `surface_id`
- `role`
- declared summary schema

This lets the compare follow the same route concept even when the absolute
artifact paths or sibling positions differ between two worlds.

## Current verdicts

Current `route_verdict` mirrors the broader compare language:

- `standing`
  - no route drift was detected
- `improved`
  - the candidate route gained reachability or richness without regressions
- `drifted`
  - the candidate route changed in a non-standing way, or lost reachable route
    surface
- `collapsed`
  - the candidate route summary itself no longer stands as `result=ok`

## Manual example

```powershell
python ./scripts/compare_system_compiler_front_page_route.py `
  --baseline cmake-build-system-compiler-front-page-route-smoke/root-witness/front-page.route.summary.json `
  --candidate cmake-build-system-compiler-front-page-route-smoke/witness-ci-shelf/front-page.route.summary.json `
  --output-root cmake-build-system-compiler-front-page-route-compare-smoke/root-witness-to-witness-ci-shelf
```

Then validate the exported compare object:

```powershell
python ./scripts/validate_system_compiler_front_page_route_compare.py `
  --summary cmake-build-system-compiler-front-page-route-compare-smoke/root-witness-to-witness-ci-shelf/front-page.route.compare.summary.json
```

Or run the dedicated smoke:

```powershell
./scripts/system_compiler_front_page_route_compare_smoke.ps1 -Clean
```

## Why this matters

This object is small, but it raises the explain surface one more step.

We can now ask not only:

- what route exists

but also:

- how that route changed
- what a richer default front page would expose
- what consumer-facing path disappeared when a world drifted

That gives the system compiler line a route-aware compare surface without
forcing later tools to bypass the artifact layer.
