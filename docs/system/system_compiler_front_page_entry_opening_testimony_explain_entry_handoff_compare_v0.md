# System Compiler Front Page Entry Opening Testimony Explain Entry Handoff Compare v0

`system_compiler.front_page_entry_opening_testimony_explain_entry_handoff_compare/v0`
compares two opening testimony explain-entry handoff summaries.

It consumes only:

- `system_compiler.front_page_entry_opening_testimony_explain_entry_handoff/v0`

It does not consume opening testimony explain entries, routes, landings,
open-event witnesses, runtime session summaries, world compare summaries,
witness bundles, or raw kernel runtime evidence.

## Role

The opening testimony chain can now expose this compare seam:

```text
opening_testimony_explain_entry
  -> opening_testimony_explain_entry_handoff
  -> opening_testimony_explain_entry_handoff_compare
```

The compare answers:

```text
Do these handoffs still open the same explain target with the same action?
```

It is intentionally narrower than route compare. It checks the downstream open
instruction surface, not the underlying testimony or route reasoning.

## Verdict Policy

The handoff compare emits:

- `standing` when the candidate is ready and preserves the same open target and
  handoff action.
- `improved` when the baseline was blocked and the candidate recovers to ready.
- `drifted` when the candidate is ready but the target, action, or decision
  surface changes.
- `collapsed` when the candidate handoff is blocked.

Supporting targets and typed next questions are compared and reported as side
context drift. They do not replace the primary open target policy.

## Current Shape

Current `system_compiler.front_page_entry_opening_testimony_explain_entry_handoff_compare`
includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_testimony_explain_entry_handoff_compare.v0.schema.json`
- comparer
  - `scripts/compare_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare.py`
- smokes
  - `scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare_route_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare_route_explain_entry_handoff_compare_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare_route_compare_explain_entry_handoff_compare_smoke.ps1`

The comparer leaves behind:

- `front-page.entry-opening-testimony.explain-entry.handoff.compare.summary.json`
- `front-page.entry-opening-testimony.explain-entry.handoff.compare.report.md`
- `front-page.entry-opening-testimony.explain-entry.handoff.compare.check.txt`

## Route Closure

The compare summary exposes `front_page.supporting_surfaces` for:

- `baseline_opening_testimony_explain_entry_handoff`
- `candidate_opening_testimony_explain_entry_handoff`

That makes the generic `front_page_route/v0` traversal able to inspect the
baseline and candidate handoff surfaces without adding handoff-specific route
logic.

## Manual Example

Run the handoff compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare_smoke.ps1 -Clean
```

Run the route closure smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare_route_smoke.ps1 -Clean
```

Run the return-seam guard smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare_route_explain_entry_handoff_compare_smoke.ps1 -Clean
```

This guard keeps the downstream loop narrow:

```text
handoff_compare
  -> front_page_route
  -> opening_testimony_explain_entry
  -> opening_testimony_explain_entry_handoff
  -> opening_testimony_explain_entry_handoff_compare
```

It checks that the route-derived handoff can still be compared by the existing
handoff compare surface, without reading lower testimony, runtime-session, or
world-compare evidence.

Run the route-compare return-seam guard smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare_route_compare_explain_entry_handoff_compare_smoke.ps1 -Clean
```

This keeps the route-compare loop narrow:

```text
handoff_compare
  -> front_page_route
  -> front_page_route_compare
  -> opening_testimony_explain_entry
  -> opening_testimony_explain_entry_handoff
  -> opening_testimony_explain_entry_handoff_compare
```

It checks that a handoff selected through a route-compare explain-entry still
opens the same target with the same action as the route-derived handoff. The
source route may change, but the downstream handoff decision must remain
standing.

Export directly:

```powershell
python ./scripts/compare_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py `
  --baseline cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-smoke/clean-route-handoff/front-page.entry-opening-testimony.explain-entry.handoff.summary.json `
  --candidate cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-smoke/landing-compare-route-handoff/front-page.entry-opening-testimony.explain-entry.handoff.summary.json `
  --output-root cmake-build-opening-testimony-explain-entry-handoff-compare
```

Then validate:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare.py `
  --summary cmake-build-opening-testimony-explain-entry-handoff-compare/front-page.entry-opening-testimony.explain-entry.handoff.compare.summary.json
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-SMOKE] case=self-standing verdict=standing candidate_status=ready target_changed=False
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-SMOKE] case=ready-to-blocked verdict=collapsed candidate_status=blocked target_changed=True
```

## Boundary

This is not a handoff exporter, an explain-entry compare replacement, a route
compare replacement, or a witness compare. It is the smallest downstream guard
for a deterministic handoff action: the selected explain target and the action
used to open it.
