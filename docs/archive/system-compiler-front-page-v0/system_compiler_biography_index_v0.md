# System Compiler Biography Index v0

`system_compiler.biography_index/v0` is the next directory layer above a single
`system_compiler.biography/v0`.

If two shelves later need to be compared as one grouped delivery object, the
next layer up is `system_compiler.biography_index_compare/v0`.

It does not replace:

- `runtime evidence bundle`
- `witness bundle`
- `world compare`
- `system compiler biography`

It answers a thinner question:

> If we already have one or more self-explaining biographies, how do we shelve
> them into a browsable world directory that can be validated, published, and
> compared as a group?

## One-sentence version

- `runtime evidence bundle` answers whether this runtime evidence pass closed.
- `witness bundle` answers what testimony the candidate world stands on.
- `world compare` answers how the candidate world moves relative to a baseline.
- `system compiler biography` compresses one world into a self-explaining front page.
- `system compiler biography index` shelves multiple biographies into one
  delivery page so the world set can be browsed and audited together.

If `biography` is the cover page for one world, then `biography index` is the
front desk or shelf:

> Which worlds are on this shelf, which ones are compare-attached, which ones
> are still witness-only, and where should the next review question go?

## Current object boundary

Current `system compiler biography index` includes:

- schema
  - `schemas/system_compiler.biography_index.v0.schema.json`
- export script
  - `scripts/export_system_compiler_biography_index.py`
- assembly wrapper
  - `scripts/assemble_system_compiler_world_shelf.ps1`
- review wrapper
  - `scripts/review_system_compiler_world_shelf.ps1`
- validate script
  - `scripts/validate_system_compiler_biography_index.py`
- gate script
  - `scripts/check_system_compiler_biography_index_summary.ps1`

Current inputs:

- one or more `system compiler biography` summaries

Current outputs:

- `biography.index.summary.json`
- `biography.index.report.md`
- `biography.index.check.txt`

When the assembly wrapper is used, the delivery root also gets sidecars such as:

- `discovered_biographies.txt`
- `biography.index.export.log`
- `biography.index.validate.log`
- `biography.index.gate.log`

## Current export semantics

The current shelf keeps five responsibilities stable:

### 1. Shelf summary

It records:

- how many biographies are on the shelf
- how many unique worlds are present
- how many biographies are compare-attached
- how many are still `not-attached`
- how many verdicts are `standing / improved / drifted / collapsed`

This is the fastest answer to "what kind of world set am I looking at?"

### 2. Entry directory

Each entry keeps only the minimal fields needed to reopen the underlying world:

- world identity
- profile
- board / active facets
- verdict / compare-attached state
- top-level thesis
- evidence path
- next questions
- paths back to biography / runtime / witness / compare summaries

Those routed paths should prefer the underlying `system_compiler.biography/v0`
`front_page.supporting_surfaces` and only fall back to older `artifact_context`
summary fields when needed.

The shelf is intentionally not a second full copy of the biography body.

### 3. Aggregated questions

The shelf also gathers:

- `core_questions`
- `compare_questions`
- `next_questions`

That keeps the shelf usable as a review queue rather than only a file list.

### 4. Delivery closure

Like the lower layers, the shelf emits:

- `summary`
- `report`
- `check`

So the shelf itself can be:

- validated
- attached to CI artifacts
- rendered into step summaries
- handed over as a first read for reviewers

### 5. Machine front page

The shelf now also emits a machine-readable `front_page`.

That front page keeps:

- the shelf's own `summary / report / check`
- one supporting surface per biography on the shelf

Each supporting surface routes back to the underlying
`system_compiler.biography/v0` front page instead of copying the whole
biography body upward again.

This is intentionally different from:

- `delivery`
  - where this shelf landed
- `artifact_context`
  - which biographies were assembled to make it

`front_page` is the answer to a narrower routing question:

> If a tool or reviewer opens this shelf first, which concrete surfaces should
> it follow next?

## Recommended usage

### Assemble a shelf by recursive discovery

If a delivery root already contains one or more `biography.summary.json` files,
the recommended path is to let the assembler discover them:

```powershell
./scripts/assemble_system_compiler_world_shelf.ps1 `
  -SearchRoot out/minimal-kernel-runtime-system-compiler-witness `
  -OutputRoot out/minimal-kernel-runtime-system-compiler-witness/world-shelf `
  -Profile minimal-kernel-runtime-system-compiler-witness-shelf `
  -RequireBiographyCount 2 `
  -RequireUniqueWorldCount 1 `
  -RequireOkCount 2 `
  -MaxFailCount 0 `
  -RequireCompareAttachedCount 1 `
  -RequireNotAttachedCount 1 `
  -RequireStandingCount 1
```

This wrapper does four things in one pass:

- discover biography summaries
- validate each discovered biography
- export and validate the shelf
- run the shelf gate

It also leaves behind the discovered biography list and the export / validate /
gate logs so the shelf itself can explain how it was assembled.

### Review a candidate shelf against a baseline from one wrapper

If the caller starts from biographies rather than from already-built shelves,
the next recommended path is the review wrapper:

```powershell
./scripts/review_system_compiler_world_shelf.ps1 `
  -SearchRoot out/minimal-kernel-runtime-system-compiler-witness `
  -BaselineBiographySummary out/minimal-kernel-runtime-system-compiler-witness/biography.summary.json `
  -OutputRoot out/minimal-kernel-runtime-system-compiler-witness `
  -CandidateProfile minimal-kernel-runtime-system-compiler-witness-shelf `
  -CandidateRequireBiographyCount 2 `
  -CandidateRequireCompareAttachedCount 1 `
  -BaselineProfile minimal-kernel-runtime-system-compiler-witness-shelf-baseline `
  -BaselineRequireBiographyCount 1 `
  -CompareRequireVerdict improved `
  -CompareMaxRegressions 0
```

This wrapper keeps the same stable shelf outputs:

- `world-shelf/`
- `world-shelf-baseline/`
- `world-shelf-compare/`

and also emits a thin review front page:

- `world-shelf.review.md`
- `world-shelf.check.txt`

The minimal-kernel witness CI wrapper now lands on this same seam directly:

```powershell
./scripts/ci_minimal_kernel_runtime_system_compiler_witness_bundle.ps1 `
  -OutputRoot out/minimal-kernel-runtime-system-compiler-witness `
  -RunWorldShelfFlow `
  -Clean
```

That flow keeps the root witness export and the self-compare export in the CI
wrapper, then hands the biography-level shelf review to
`review_system_compiler_world_shelf.ps1`.

With `-RunWorldShelfFlow`, the CI wrapper also appends a short `World Shelf
Review` overlay back into the root `report.md` and `check.txt`, so the delivery
front page can expose the shelf verdict without a manual jump into
`world-shelf/`.

When calling the review wrapper directly from raw PowerShell, prefer
`-SearchRoot` or a splatted hashtable for multi-biography inputs. That is more
reliable than relaying nested array literals through another script layer.

### Build a shelf from root + self-compare biographies manually

```powershell
python ./scripts/export_system_compiler_biography_index.py `
  --biography out/minimal-kernel-runtime-system-compiler-witness/biography.summary.json `
  --biography out/minimal-kernel-runtime-system-compiler-witness/self-compare/biography.summary.json `
  --output-root out/minimal-kernel-runtime-system-compiler-witness/world-shelf `
  --profile minimal-kernel-runtime-system-compiler-witness-shelf
```

Then validate it:

```powershell
python ./scripts/validate_system_compiler_biography_index.py `
  --bundle-root out/minimal-kernel-runtime-system-compiler-witness/world-shelf
```

If this shelf should also become a CI gate, check the exported summary directly:

```powershell
./scripts/check_system_compiler_biography_index_summary.ps1 `
  -Summary out/minimal-kernel-runtime-system-compiler-witness/world-shelf/biography.index.summary.json `
  -RequireBiographyCount 2 `
  -RequireUniqueWorldCount 1 `
  -RequireOkCount 2 `
  -MaxFailCount 0 `
  -RequireCompareAttachedCount 1 `
  -RequireNotAttachedCount 1 `
  -RequireStandingCount 1 `
  -RequireImprovedCount 0 `
  -RequireDriftedCount 0 `
  -RequireCollapsedCount 0
```

### Build a one-entry shelf for a compare-closed delivery

```powershell
python ./scripts/export_system_compiler_biography_index.py `
  --biography out/minimal-kernel-runtime-system-compiler-world-compare/biography.summary.json `
  --output-root out/minimal-kernel-runtime-system-compiler-world-compare/world-shelf `
  --profile minimal-kernel-runtime-system-compiler-world-compare-shelf
```

Then gate the one-entry compare shelf:

```powershell
./scripts/check_system_compiler_biography_index_summary.ps1 `
  -Summary out/minimal-kernel-runtime-system-compiler-world-compare/world-shelf/biography.index.summary.json `
  -RequireBiographyCount 1 `
  -RequireUniqueWorldCount 1 `
  -RequireOkCount 1 `
  -MaxFailCount 0 `
  -RequireCompareAttachedCount 1 `
  -RequireNotAttachedCount 0 `
  -RequireStandingCount 1 `
  -RequireImprovedCount 0 `
  -RequireDriftedCount 0 `
  -RequireCollapsedCount 0
```

The same case can also be assembled by discovery:

```powershell
./scripts/assemble_system_compiler_world_shelf.ps1 `
  -SearchRoot out/minimal-kernel-runtime-system-compiler-world-compare `
  -OutputRoot out/minimal-kernel-runtime-system-compiler-world-compare/world-shelf `
  -Profile minimal-kernel-runtime-system-compiler-world-compare-shelf `
  -RequireBiographyCount 1 `
  -RequireCompareAttachedCount 1 `
  -RequireStandingCount 1 `
  -MaxFailCount 0
```

## Relationship to existing objects

### 1. Versus `system compiler biography`

`biography` is still the unit front page for one world.

`biography index` is only the shelf above it. It should stay thinner than the
biography body and mainly answer:

- what worlds are present
- what verdict mix they currently form
- where the next review pass should start

### 2. Versus `world compare`

`world compare` still owns the counterfactual verdict.

The shelf only records whether a biography already carries that verdict and
where the underlying compare artifact lives.

### 3. Versus `witness bundle`

`witness bundle` still owns the testimony set for one candidate world.

The shelf never tries to restate witness entries in full; it only points to the
biography that already compressed them.

### 4. Versus `system compiler biography index compare`

`biography index` is still the shelf itself.

If two shelves need to be reviewed as baseline vs candidate, that compare
belongs in `docs/system/system_compiler_biography_index_compare_v0.md`.

If the caller wants one wrapper that starts from biographies and lands on both
shelves plus the shelf compare handoff, use
`scripts/review_system_compiler_world_shelf.ps1`.

If the caller also wants the machine-readable review envelope that wraps that
handoff, continue with
`docs/system/system_compiler_world_shelf_review_v0.md`.

## Current non-goals

This v0 still does not try to solve:

- full multi-world chronology
- automatic clustering of worlds into capability species
- repo-wide shelf generation for every biography in the tree
- automatic narrative ranking of which world should be read first

The current goal is narrower:

> Give Charm a validated world shelf so multiple biographies can be delivered as
> one explainable directory object instead of as disconnected JSON files.
