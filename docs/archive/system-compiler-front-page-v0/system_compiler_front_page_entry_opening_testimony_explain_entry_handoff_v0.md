# System Compiler Front Page Entry Opening Testimony Explain Entry Handoff v0

`system_compiler.front_page_entry_opening_testimony_explain_entry_handoff/v0`
projects one opening testimony explain-entry into a deterministic downstream
open instruction.

It consumes only:

- `system_compiler.front_page_entry_opening_testimony_explain_entry/v0`

It does not consume routes, opening testimony landings, open-event witnesses,
runtime session summaries, world compare summaries, witness bundles, or raw
kernel runtime evidence.

## Role

The opening testimony chain can now expose this handoff:

```text
open_event_witness
  -> opening_testimony_landing
  -> front_page_route
  -> opening_testimony_explain_entry
  -> opening_testimony_explain_entry_handoff
```

For explain-entry compare routes:

```text
opening_testimony_explain_entry_compare
  -> front_page_route
  -> opening_testimony_explain_entry
  -> opening_testimony_explain_entry_handoff
```

This object answers:

```text
How should a downstream tool open the selected explain surface now?
```

## Handoff Policy

The handoff copies the explain-entry `selected_surface` into `open_target`.
It preserves `supporting_surfaces` as `supporting_targets` without reordering or
reinterpreting them.

The fixed handoff action is:

- `action_kind=open_explain_surface`
- `query_kind=default_explain_surface`
- `query_scope=selected_surface`
- `expected_consumer_operation=open-selected-summary`

The handoff is `ready` only when the source explain-entry is `ok`, its decision
is `ready`, and the selected surface has an existing summary path.

## Current Shape

Current `system_compiler.front_page_entry_opening_testimony_explain_entry_handoff`
includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_testimony_explain_entry_handoff.v0.schema.json`
- exporter
  - `scripts/export_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py`
- smokes
  - `scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_compare_route_handoff_smoke.ps1`

The exporter leaves behind:

- `front-page.entry-opening-testimony.explain-entry.handoff.summary.json`
- `front-page.entry-opening-testimony.explain-entry.handoff.report.md`
- `front-page.entry-opening-testimony.explain-entry.handoff.check.txt`

## Manual Example

Run the standard handoff smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_smoke.ps1 -Clean
```

Run the explain-entry-compare route handoff smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_compare_route_handoff_smoke.ps1 -Clean
```

Export directly:

```powershell
python ./scripts/export_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py `
  --source-summary cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-smoke/clean-route-explain-entry/front-page.entry-opening-testimony.explain-entry.summary.json `
  --output-root cmake-build-opening-testimony-explain-entry-handoff
```

Then validate:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py `
  --summary cmake-build-opening-testimony-explain-entry-handoff/front-page.entry-opening-testimony.explain-entry.handoff.summary.json
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-SMOKE] case=clean-route-handoff status=ready target=source_open_event_witness
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-COMPARE-ROUTE-HANDOFF-SMOKE] status=ready target=candidate_opening_testimony_explain_entry
```

## Boundary

This is not an explain UI, a handoff compare, or a replacement for
`system_compiler.front_page_entry_opener/v0`. The older opener remains tied to
front-page landing inputs. This handoff is a lower-risk downstream seam for
tools that already receive an opening testimony explain-entry decision and only
need a stable default open action.

## Relationship to OpeningJudgmentCorridor

Within `OpeningJudgmentCorridor`, this handoff is the current downstream
terminal action of the `Reading corridor`.

It receives one already-selected explain surface and turns it into a stable
open instruction.

It must not:

- reopen raw runtime/session/world-compare evidence
- replace explain-entry selection
- invent a second handoff-side routing policy
