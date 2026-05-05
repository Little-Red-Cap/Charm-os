# System Compiler Front Page Entry Opening Testimony Explain Entry v0

`system_compiler.front_page_entry_opening_testimony_explain_entry/v0`
projects one front-page route or route compare into a default explain-entry
decision.

It consumes only:

- `system_compiler.front_page_route/v0`
- `system_compiler.front_page_route_compare/v0`

It does not consume open-event witnesses directly, runtime session summaries,
world compare summaries, or raw kernel runtime evidence.

For runtime-session, this means the upper explain-entry decision is welded onto
the already-closed opening judgment chain without adding a second compare brain
or rereading raw session evidence:

```text
runtime-session consumer
  -> runtime-session opening bridge
  -> open_event
  -> open_event_witness
  -> opening_testimony_landing
  -> front_page_route
  -> opening_testimony_explain_entry
```

## Role

The opening testimony chain is now:

```text
open_event_witness
  -> opening_testimony_landing
  -> front_page_route
  -> opening_testimony_explain_entry
```

For route compare inputs:

```text
opening_testimony_landing
  -> front_page_route
  -> front_page_route_compare
  -> opening_testimony_explain_entry
```

This object answers:

```text
Which explain surface should this opening testimony route open by default?
```

## Selection Policy

For a route whose root is `opening_testimony_landing/v0`, the selected surface is
`source_open_event_witness`.

For a route whose root is `opening_testimony_landing_compare/v0`, the selected
surface is `candidate_opening_testimony_landing`, and the baseline landing is
kept as supporting context.

For a route whose root is `opening_testimony_explain_entry_compare/v0`, the
selected surface is `candidate_opening_testimony_explain_entry`, and the
baseline explain entry is kept as supporting context.

For route compare summaries:

- `standing` selects the candidate route root.
- `improved` and `drifted` select the candidate changed or newly added surface.
- `collapsed` is blocked.

The exporter only reads route and route-compare surfaces. It does not reopen the
selected witness, landing, or runtime/session evidence.

## Current Shape

Current `system_compiler.front_page_entry_opening_testimony_explain_entry`
includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_testimony_explain_entry.v0.schema.json`
- exporter
  - `scripts/export_system_compiler_front_page_entry_opening_testimony_explain_entry.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_testimony_explain_entry.py`
- smokes
  - `scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_route_compare_smoke.ps1`

The exporter leaves behind:

- `front-page.entry-opening-testimony.explain-entry.summary.json`
- `front-page.entry-opening-testimony.explain-entry.report.md`
- `front-page.entry-opening-testimony.explain-entry.check.txt`

The runtime-session-targeted smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-explain-entry-smoke
```

## Ready Rule

The explain entry is `ready` when:

- source schema and kind are a supported route or route compare
- the source result is `ok`
- the selected explain surface exists
- the selected explain surface has a resolvable summary path

Otherwise it is `blocked`. Violations describe only route/explain-entry
selection failures.

## Manual Example

Run the route smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_smoke.ps1 -Clean
```

Run the route-compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_route_compare_smoke.ps1 -Clean
```

Export directly from a route:

```powershell
python ./scripts/export_system_compiler_front_page_entry_opening_testimony_explain_entry.py `
  --source-summary cmake-build-system-compiler-front-page-entry-opening-testimony-landing-route-smoke/clean-landing-route/front-page.route.summary.json `
  --output-root cmake-build-opening-testimony-explain-entry
```

Then validate:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_testimony_explain_entry.py `
  --summary cmake-build-opening-testimony-explain-entry/front-page.entry-opening-testimony.explain-entry.summary.json
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-SMOKE] case=clean-route-explain-entry status=ready selection=route_landing_default selected=source_open_event_witness
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-ROUTE-COMPARE-SMOKE] case=drifted-route-compare-explain-entry status=ready selection=route_compare_candidate_change selected=candidate_opening_testimony_landing
```

## Boundary

This is a default explain-entry decision seam. It is not a witness bundle, a
runtime-session compare, or a UI integration. It exists so later front-page
tooling can consume a stable opening testimony route decision without rebuilding
route traversal policy.
