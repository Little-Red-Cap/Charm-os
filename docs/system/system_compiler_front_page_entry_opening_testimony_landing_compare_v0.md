# System Compiler Front Page Entry Opening Testimony Landing Compare v0

`system_compiler.front_page_entry_opening_testimony_landing_compare/v0`
compares two opening testimony landing summaries.

It consumes only:

- `system_compiler.front_page_entry_opening_testimony_landing/v0`

It does not consume open-event witnesses directly, runtime session summaries,
world compare summaries, or raw kernel runtime evidence.

## Role

`opening_testimony_landing` answers:

```text
Which explain entry should this opening judgment testimony open as?
```

`opening_testimony_landing_compare` answers:

```text
Do two testimony landings still open the same explain entry for the same
opening testimony?
```

This is a landing drift judge.

It is not a witness drift judge and it is not a runtime-session compare.

## Current Shape

Current `system_compiler.front_page_entry_opening_testimony_landing_compare`
includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_testimony_landing_compare.v0.schema.json`
- comparer
  - `scripts/compare_system_compiler_front_page_entry_opening_testimony_landing.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_testimony_landing_compare.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opening_testimony_landing_compare_smoke.ps1`

The comparer leaves behind:

- `front-page.entry-opening-testimony.landing.compare.summary.json`
- `front-page.entry-opening-testimony.landing.compare.report.md`
- `front-page.entry-opening-testimony.landing.compare.check.txt`

The default output root is:

```powershell
out/system-compiler-front-page-entry-opening-testimony-landing-compare
```

The smoke output root is:

```powershell
cmake-build-system-compiler-front-page-entry-opening-testimony-landing-compare-smoke
```

## What It Compares

The compare summary records:

- `opening_identity_changes`
  - source open event id/status/reason
  - source judgment status/grade
  - source witness status
- `landing_decision_changes`
  - selected entry id
  - selected tab id
  - selected role
  - opening reason
- `testimony_preview_changes`
  - headline
  - source judgment summary
  - explanation lines
  - observation lines
- `artifact_target_changes`
  - witness-declared evidence refs
  - witness-declared artifact refs
- `next_question_changes`
  - typed explain-entry hints

It compares the landing contract, not arbitrary JSON.

## Verdicts

Current `landing_verdict` values are:

- `standing`
  - the two landings preserve the same explain-entry surface
- `improved`
  - a previously blocked landing recovered to `ready`
- `drifted`
  - the candidate is still ready but the explain-entry landing changed
- `collapsed`
  - the candidate landing no longer stands as `ready`

## Boundary

This object must not:

- re-prove source witness semantics
- inspect runtime/session/world-compare raw evidence
- parse runtime-session bridge summaries
- decide default focus or default explain hop

If the source open event or witness changes, that fact should already be
visible through the two landing summaries.

This compare only judges how that change affects the explain-entry landing.

## Manual Example

Run the smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_testimony_landing_compare_smoke.ps1 -Clean
```

Compare two landing summaries directly:

```powershell
python ./scripts/compare_system_compiler_front_page_entry_opening_testimony_landing.py `
  --baseline cmake-build-system-compiler-front-page-entry-opening-testimony-landing-smoke/clean-witness-landing/front-page.entry-opening-testimony.landing.summary.json `
  --candidate cmake-build-system-compiler-front-page-entry-opening-testimony-landing-smoke/drift-witness-landing/front-page.entry-opening-testimony.landing.summary.json `
  --output-root cmake-build-opening-testimony-landing-compare
```

Then validate:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_testimony_landing_compare.py `
  --summary cmake-build-opening-testimony-landing-compare/front-page.entry-opening-testimony.landing.compare.summary.json
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-COMPARE-SMOKE] case=opening-testimony-landing-self-standing verdict=standing changed=0
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-COMPARE-SMOKE] case=opening-testimony-landing-clean-to-drift verdict=drifted
[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-COMPARE-SMOKE] case=opening-testimony-landing-clean-to-blocked verdict=collapsed
```

## Why This Matters

The explain-entry testimony chain now has a compare seam:

```text
open_event_witness
  -> opening_testimony_landing
  -> opening_testimony_landing_compare
```

Later front-page tooling can consume one compare object instead of rebuilding
landing drift policy from the witness or from lower runtime evidence.
