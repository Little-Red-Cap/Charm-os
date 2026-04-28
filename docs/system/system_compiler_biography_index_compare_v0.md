# System Compiler Biography Index Compare v0

`system_compiler.biography_index_compare/v0` sits one directory layer above a
single `system_compiler.biography_index/v0`.

It does not replace:

- `runtime evidence bundle`
- `witness bundle`
- `world compare`
- `system compiler biography`
- `system compiler biography index`

It answers a narrower but important question:

> If we already know how to shelve one or more biographies into a validated
> world shelf, how do we compare two shelves as one grouped delivery object?

## One-sentence version

- `runtime evidence bundle` answers whether one runtime evidence pass closed.
- `witness bundle` answers what testimony one candidate world stands on.
- `world compare` answers how one candidate world moves relative to one baseline.
- `system compiler biography` compresses one world into a self-explaining front
  page.
- `system compiler biography index` shelves multiple biographies into one
  delivery page.
- `system compiler biography index compare` compares two shelves so a grouped
  world delivery can be reviewed as `standing / improved / drifted / collapsed`.

If `biography index` is the shelf, then `biography index compare` is the shelf
review record:

> Which shelf entries were added, removed, or drifted, which world set got
> better or worse, and what is the smallest shelf-level collapse surface now?

## Current object boundary

Current `system compiler biography index compare` includes:

- schema
  - `schemas/system_compiler.biography_index_compare.v0.schema.json`
- compare script
  - `scripts/compare_system_compiler_biography_index.py`
- validate script
  - `scripts/validate_system_compiler_biography_index_compare.py`
- gate script
  - `scripts/check_system_compiler_biography_index_compare_summary.ps1`

Current inputs:

- one baseline `system compiler biography index` summary
- one candidate `system compiler biography index` summary

Current outputs:

- `summary.json`
- `report.md`
- `check.txt`

## Current compare semantics

The current compare object keeps five responsibilities stable.

### 1. Shelf verdict

It answers whether the candidate shelf is:

- `standing`
- `improved`
- `drifted`
- `collapsed`

This verdict is shelf-level, not single-world-level.

### 2. Shelf drift

It records whether the shelf itself changed:

- title / summary / profile
- aggregated questions
- world-name set

That keeps "the shelf constitution changed" separate from "one entry changed."

### 3. Entry drift

Entries are matched by a stable anchor built from:

- `world_name`
- `profile`
- `board`
- `active_facets`
- `compare_attached`

This avoids keying directly on `entry.id`, which may churn when verdict text
changes.

Each entry drift is projected as:

- `added / removed / changed`
- `regression / improvement / neutral`
- left / right result and verdict
- minimal path and question drift

### 4. Collapse surface

The compare object also compresses shelf-level risk into one projection:

- regressed shelf entries
- removed worlds
- newly added failing entries
- affected worlds / profiles
- short narratives that explain where the shelf first starts to crack

### 5. Delivery closure

Like the lower layers, the compare object emits:

- `summary`
- `report`
- `check`

So grouped shelf compare can become:

- a CI gate
- a step-summary front page
- a review handoff object

## Recommended usage

### Compare a one-entry witness shelf to a two-entry compare-attached shelf

```powershell
python ./scripts/compare_system_compiler_biography_index.py `
  --baseline out/minimal-kernel-runtime-system-compiler-biography-index-baseline-smoke/biography.index.summary.json `
  --candidate out/minimal-kernel-runtime-system-compiler-biography-index-smoke/biography.index.summary.json `
  --output-root out/minimal-kernel-runtime-system-compiler-biography-index-compare-smoke
```

Then validate it:

```powershell
python ./scripts/validate_system_compiler_biography_index_compare.py `
  --bundle-root out/minimal-kernel-runtime-system-compiler-biography-index-compare-smoke
```

If this compare should become a CI gate, check the exported summary directly:

```powershell
./scripts/check_system_compiler_biography_index_compare_summary.ps1 `
  -Summary out/minimal-kernel-runtime-system-compiler-biography-index-compare-smoke/summary.json `
  -RequireVerdict improved `
  -MaxRegressions 0 `
  -RequireAddedEntries 1 `
  -RequireRemovedEntries 0 `
  -RequireChangedEntries 1 `
  -RequireImprovementCount 1 `
  -RequireAddedWorlds 0 `
  -RequireRemovedWorlds 0 `
  -MaxAddedFailedEntries 0
```

### Compare a one-entry shelf against itself

```powershell
python ./scripts/compare_system_compiler_biography_index.py `
  --baseline out/minimal-kernel-runtime-system-compiler-biography-index-single-smoke/biography.index.summary.json `
  --candidate out/minimal-kernel-runtime-system-compiler-biography-index-single-smoke/biography.index.summary.json `
  --output-root out/minimal-kernel-runtime-system-compiler-biography-index-self-compare-smoke
```

Then gate the standing result:

```powershell
./scripts/check_system_compiler_biography_index_compare_summary.ps1 `
  -Summary out/minimal-kernel-runtime-system-compiler-biography-index-self-compare-smoke/summary.json `
  -RequireVerdict standing `
  -MaxRegressions 0 `
  -RequireAddedEntries 0 `
  -RequireRemovedEntries 0 `
  -RequireChangedEntries 0 `
  -RequireImprovementCount 0 `
  -RequireAddedWorlds 0 `
  -RequireRemovedWorlds 0 `
  -MaxAddedFailedEntries 0
```

## Relationship to existing objects

### 1. Versus `system compiler biography index`

`biography index` is still the directory object for one grouped shelf.

`biography index compare` sits above it and only answers how one shelf moved
relative to another.

### 2. Versus `world compare`

`world compare` still owns the verdict for one candidate world.

`biography index compare` uses already-compressed biography entries and answers
the next question up: how the grouped world shelf moved as one delivery.

### 3. Versus shelf assembly

`assemble_system_compiler_world_shelf.ps1` is a producer convenience wrapper.

`biography index compare` starts only after both shelves already exist.

## Current non-goals

This v0 still does not try to solve:

- full multi-shelf chronology
- automatic ranking of the best shelf baseline
- repo-wide compare scheduling across every shelf in the tree
- root-cause analysis beyond the first shelf-level collapse surface

The current goal is narrower:

> Give Charm a validated shelf-to-shelf compare object so grouped world
> deliveries can be reviewed, gated, and handed over as one explainable diff.
