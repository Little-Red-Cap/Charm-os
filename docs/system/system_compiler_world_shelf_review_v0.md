# System Compiler World Shelf Review v0

`system_compiler.world_shelf_review/v0` sits one workflow layer above
`system_compiler.biography_index/v0` and
`system_compiler.biography_index_compare/v0`.

It is the first object in this shelf line that answers the packaging question:

- which candidate shelf was reviewed
- which baseline shelf, if any, was used
- which compare verdict the review landed on
- which next questions should continue from this review seam

It does not replace the lower objects.

- `biography index` still owns one grouped shelf
- `biography index compare` still owns shelf-to-shelf drift semantics
- `world shelf review` only wraps those objects into a review envelope that
  CI, publish steps, and higher-level witness surfaces can consume directly

## Machine front page

`system_compiler.world_shelf_review/v0` now also emits a machine-readable
`front_page`.

That front page keeps:

- the review envelope's own `summary / report / check`
- the candidate shelf as the first follow-up surface
- the shelf compare surface when compare is enabled
- the baseline shelf as an additional follow-up surface when compare is enabled

This is intentionally narrower than `artifact_context`.

- `artifact_context` answers which paths were used to assemble and validate the
  review
- `front_page` answers which concrete lower surfaces a router or reviewer
  should open next after landing on the review summary itself

In other words, `world shelf review` is no longer only a wrapper object.
It can now route the next read without forcing higher layers to rediscover the
candidate shelf, compare seam, or baseline shelf on their own.

## Current shape

Current `system compiler world shelf review` includes:

- schema
  - `schemas/system_compiler.world_shelf_review.v0.schema.json`
- review wrapper
  - `scripts/review_system_compiler_world_shelf.ps1`
- validate script
  - `scripts/validate_system_compiler_world_shelf_review.py`

## Current outputs

By default the review wrapper leaves behind:

- `world-shelf.review.summary.json`
- `world-shelf.review.md`
- `world-shelf.check.txt`
- `world-shelf.review.validate.log`

and, depending on mode:

- `world-shelf/`
- `world-shelf-baseline/`
- `world-shelf-compare/`

## Manual example

```powershell
./scripts/review_system_compiler_world_shelf.ps1 `
  -SearchRoot out/minimal-kernel-runtime-system-compiler-witness `
  -BaselineBiographySummary out/minimal-kernel-runtime-system-compiler-witness/biography.summary.json `
  -OutputRoot out/minimal-kernel-runtime-system-compiler-witness `
  -CandidateProfile minimal-kernel-runtime-system-compiler-witness-shelf `
  -CandidateRequireBiographyCount 2 `
  -CandidateRequireCompareAttachedCount 1 `
  -BaselineProfile minimal-kernel-runtime-system-compiler-witness-shelf `
  -BaselineRequireBiographyCount 1 `
  -CompareRequireVerdict improved `
  -CompareMaxRegressions 0
```

This wrapper assembles the candidate shelf, optionally assembles or reopens the
baseline shelf, runs the shelf compare when enabled, writes the review summary,
and validates the new review object in one pass.

## CI entry points

The minimal-kernel CI wrappers can now land on this seam directly:

```powershell
./scripts/ci_minimal_kernel_runtime_system_compiler_witness_bundle.ps1 `
  -OutputRoot out/minimal-kernel-runtime-system-compiler-witness `
  -RunWorldShelfFlow `
  -Clean
```

```powershell
./scripts/ci_minimal_kernel_runtime_system_compiler_world_compare.ps1 `
  -OutputRoot out/minimal-kernel-runtime-system-compiler-world-compare `
  -RunWorldShelfFlow `
  -Clean
```

The witness path uses a two-entry candidate shelf against a one-entry baseline
and should land on `review_verdict = improved`.

The world-compare path uses one shelf against itself and should land on
`review_verdict = standing`.

## Relationship to existing objects

### 1. Versus `system compiler biography index`

`biography index` is still the grouped directory object for one shelf.

`world shelf review` only adds the review envelope that points at the selected
candidate shelf and, when present, its baseline shelf.

### 2. Versus `system compiler biography index compare`

`biography index compare` still owns the shelf drift semantics.

`world shelf review` simply promotes that compare verdict into a higher-level
review artifact together with the candidate shelf, the review questions, and a
front-page route back down into the lower shelf surfaces.

### 3. Versus `world compare`

`world compare` answers whether one candidate witness world still stands
relative to a baseline world.

`world shelf review` answers how a grouped set of biographies was reviewed as
one delivery seam after those lower-level world questions had already been
compressed.

## Current non-goals

This v0 still does not try to solve:

- a separate gate object above the review summary
- multi-shelf batch review
- cross-world review aggregation
- replacing the lower shelf compare object
