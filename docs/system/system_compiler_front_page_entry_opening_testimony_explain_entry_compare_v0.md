# System Compiler Front Page Entry Opening Testimony Explain Entry Compare v0

`system_compiler.front_page_entry_opening_testimony_explain_entry_compare/v0`
compares two opening testimony explain-entry summaries.

It consumes only:

- `system_compiler.front_page_entry_opening_testimony_explain_entry/v0`

It does not consume open-event witnesses, opening testimony landings, runtime
session summaries, world compare summaries, witness bundles, or raw kernel
runtime evidence.

## Role

The opening testimony chain can now close this route:

```text
open_event_witness
  -> opening_testimony_landing
  -> front_page_route
  -> opening_testimony_explain_entry
  -> opening_testimony_explain_entry_compare
  -> front_page_route
  -> front_page_route_compare
```

This compare answers:

```text
Do two opening testimony routes still select the same default explain surface?
```

It is intentionally a decision-surface compare. It does not rerun route
selection and does not reinterpret the lower testimony.

For runtime-session, this compare becomes the top of the testimony ladder:

```text
open_event_witness
  -> opening_testimony_landing
  -> front_page_route
  -> opening_testimony_explain_entry
  -> opening_testimony_explain_entry_compare
```

Its job is still narrow. It compares only already-selected explain-entry
surfaces and accepts `collapsed` only when the testimony/route/explain inputs
stop supporting a ready default selection.

## Compare Policy

The compare checks:

- source route ref
- explain-entry decision status and selection kind
- selected surface id, schema, and path
- supporting surfaces
- typed next questions

The verdict is:

- `standing` when the candidate remains ready and the explain-entry decision
  surface is unchanged.
- `improved` when the baseline was blocked and the candidate recovered to
  ready.
- `drifted` when the candidate is ready but the source route, decision,
  selected surface, supporting surfaces, or typed questions changed.
- `collapsed` when the candidate is blocked.

## Front Page Surface

The compare summary exposes:

- `baseline_opening_testimony_explain_entry`
- `candidate_opening_testimony_explain_entry`

through `front_page.supporting_surfaces`, so the generic route traversal can
continue without any new route schema branch.

## Current Shape

Current `system_compiler.front_page_entry_opening_testimony_explain_entry_compare`
includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_testimony_explain_entry_compare.v0.schema.json`
- comparer
  - `scripts/compare_system_compiler_front_page_entry_opening_testimony_explain_entry.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_testimony_explain_entry_compare.py`
- smokes
  - `scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_compare_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_compare_route_smoke.ps1`

The comparer leaves behind:

- `front-page.entry-opening-testimony.explain-entry.compare.summary.json`
- `front-page.entry-opening-testimony.explain-entry.compare.report.md`
- `front-page.entry-opening-testimony.explain-entry.compare.check.txt`

The runtime-session-targeted compare smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-explain-entry-compare-smoke
```

## Manual Example

Run the compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_compare_smoke.ps1 -Clean
```

Run the route closure smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_compare_route_smoke.ps1 -Clean
```

Export directly:

```powershell
python ./scripts/compare_system_compiler_front_page_entry_opening_testimony_explain_entry.py `
  --baseline cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-smoke/clean-route-explain-entry/front-page.entry-opening-testimony.explain-entry.summary.json `
  --candidate cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-smoke/drift-route-explain-entry/front-page.entry-opening-testimony.explain-entry.summary.json `
  --output-root cmake-build-opening-testimony-explain-entry-compare
```

Then validate:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_testimony_explain_entry_compare.py `
  --summary cmake-build-opening-testimony-explain-entry-compare/front-page.entry-opening-testimony.explain-entry.compare.summary.json
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-COMPARE-SMOKE] case=self-standing verdict=standing
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-COMPARE-SMOKE] case=clean-to-landing-compare verdict=drifted
```

## Boundary

This is not a new explain UI and not a testimony validator. It only compares the
default explain-entry selection already emitted by the existing explain-entry
projection.
