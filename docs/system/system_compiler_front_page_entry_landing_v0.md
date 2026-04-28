# System Compiler Front Page Entry Landing v0

`system_compiler.front_page_entry_landing/v0` is a thin consumer-side object
that sits one layer above `system_compiler.front_page_entry_capability/v0`.

The capability object says:

- which explain landings exist
- which landing mode is recommended
- which route entry is preferred for each capability

The landing object says one more practical thing:

- if a tool wants to open an explain entry now
- which tab should open first
- which tabs should appear next
- which provenance roots can be expanded as secondary entry worlds

This still avoids producer internals.

It is only packaging consumer-side decisions into a smaller open-plan object.

## Current shape

Current `system_compiler.front_page_entry_landing` includes:

- schema
  - `schemas/system_compiler.front_page_entry_landing.v0.schema.json`
- exporter
  - `scripts/export_system_compiler_front_page_entry_landing.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_landing.py`
- smoke
  - `scripts/system_compiler_front_page_entry_landing_smoke.ps1`

## Current outputs

By default the exporter leaves behind:

- `front-page.entry-landing.summary.json`
- `front-page.entry-landing.report.md`
- `front-page.entry-landing.check.txt`

## What the landing plan records

The current summary records:

- the input capability summary and its root surface
- one concrete primary landing tab
- an ordered, de-duplicated `landing_tabs` list
- `secondary_landings`
- one `fallback_mode_order`
- de-duplicated `provenance_roots`
- a compact `landing_status` block with:
  - recommended mode
  - entry tier
  - primary tab id
  - direct review / compare / biography / evidence availability

That means a tool no longer needs to infer:

- which preferred entry should win first
- whether two capabilities point to the same route entry
- whether provenance roots should be shown as separate expandable worlds

## Current ordering rules

Landing order is currently mode-driven.

Examples:

- `review`
  - `grouped_review`
  - `shelf_compare`
  - `candidate_shelf`
  - `baseline_shelf`
  - then biography / compare / evidence
- `compare`
  - `counterfactual_verdict`
  - `delivery_biography`
  - `supporting_evidence`
- `biography`
  - `delivery_biography`
  - `supporting_evidence`
  - `supporting_testimony`

If multiple capabilities resolve to the same route entry, the landing plan
collapses them into one tab and keeps the capability aliases together.

## Provenance roots

`provenance_roots` are intentionally separate from normal landing tabs.

They are meant for expandable "open another declared front-page root" actions,
not for duplicate top-level tabs.

So if grouped review and route provenance both point back to the same root
summary, the landing plan keeps:

- one normal landing tab
- zero duplicate route tabs
- separate provenance roots only when they lead to distinct summary worlds

## Manual example

```powershell
python ./scripts/export_system_compiler_front_page_entry_landing.py `
  --summary cmake-build-system-compiler-front-page-entry-capability-smoke/review-provenance/front-page.entry-capability.summary.json `
  --output-root cmake-build-system-compiler-front-page-entry-landing-smoke/review-provenance
```

Then validate the exported landing object:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_landing.py `
  --summary cmake-build-system-compiler-front-page-entry-landing-smoke/review-provenance/front-page.entry-landing.summary.json
```

Or run the dedicated smoke:

```powershell
./scripts/system_compiler_front_page_entry_landing_smoke.ps1 -Clean
```

## Why this matters

This object is useful because it lets your upcoming explain-entry consumers stay
very thin.

Instead of consuming:

- route summary
- capability summary
- preferred-entry heuristics
- provenance dedup rules

a tool can consume one smaller object that already says:

- open this tab first
- show these tabs next
- expose these provenance roots as expandable follow-on worlds

That keeps UI and tool logic closer to "render what the artifact says" and
further from "re-derive routing policy in code".
