# System Compiler Front Page Entry Landing Compare v0

`system_compiler.front_page_entry_landing_compare/v0` is the next
consumer-side object after `system_compiler.front_page_entry_landing/v0`.

`front_page_entry_landing` answers:

- which tab should open first
- which tabs follow as secondary landings
- which provenance roots can be expanded next

`front_page_entry_landing_compare` answers one more question:

- if we compare two already-exported landing plans
- did the default explain opening change
- did the candidate gain or lose direct landing capability
- did provenance roots appear or disappear
- should the candidate landing be treated as standing, improved, drifted, or collapsed

It still stays on the consumer side.

It does not reopen producer internals and it does not diff arbitrary raw JSON.

## Current shape

Current `system_compiler.front_page_entry_landing_compare` includes:

- schema
  - `schemas/system_compiler.front_page_entry_landing_compare.v0.schema.json`
- exporter
  - `scripts/compare_system_compiler_front_page_entry_landing.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_landing_compare.py`
- smoke
  - `scripts/system_compiler_front_page_entry_landing_compare_smoke.ps1`

## Current outputs

By default the comparer leaves behind:

- `front-page.entry-landing.compare.summary.json`
- `front-page.entry-landing.compare.report.md`
- `front-page.entry-landing.compare.check.txt`

## What the compare records

The compare summary currently records:

- baseline and candidate landing provenance
- default landing mode / tier / primary-tab drift
- direct explain mode changes
- tab additions, removals, and alias drift
- provenance root additions and removals
- a `landing_regression_surface` that highlights the smallest consumer-facing
  opening breakage surface

This matters because the landing layer is where "which page opens first" stops
being an implementation detail and becomes a consumer contract.

## Current verdicts

Current `landing_verdict` mirrors the broader compare language:

- `standing`
  - no landing drift was detected
- `improved`
  - the candidate gained direct capability, richer tabs, or richer provenance
    without regressions
- `drifted`
  - the candidate lost tabs or direct modes, downgraded entry tier, or changed
    the opening plan without staying equivalent
- `collapsed`
  - the candidate landing summary itself no longer stands as `result=ok`

## Manual example

```powershell
python ./scripts/compare_system_compiler_front_page_entry_landing.py `
  --baseline cmake-build-system-compiler-front-page-entry-landing-smoke/root-witness/front-page.entry-landing.summary.json `
  --candidate cmake-build-system-compiler-front-page-entry-landing-smoke/root-world-compare/front-page.entry-landing.summary.json `
  --output-root cmake-build-system-compiler-front-page-entry-landing-compare-smoke/root-witness-to-root-world-compare
```

Then validate the exported compare object:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_landing_compare.py `
  --summary cmake-build-system-compiler-front-page-entry-landing-compare-smoke/root-witness-to-root-world-compare/front-page.entry-landing.compare.summary.json
```

Or run the dedicated smoke:

```powershell
./scripts/system_compiler_front_page_entry_landing_compare_smoke.ps1 -Clean
```

## Why this matters

This object gives later explain-entry tools a thinner compare seam.

Instead of re-deriving:

- whether the default open tab changed
- whether a direct compare or review landing disappeared
- whether provenance roots got richer

a consumer can read one compare object that already says:

- this landing stands / improved / drifted / collapsed
- this primary tab changed
- these tabs or direct modes were added or removed
- these provenance roots are new or missing

That keeps later tools closer to "consume the artifact plan" and further from
"rebuild landing drift policy in code".
