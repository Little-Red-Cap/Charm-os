# System Compiler Front Page Route v0

`system_compiler.front_page_route/v0` is the first explicit consumer-side
object for the machine-readable `front_page` language.

It sits one layer above the summary objects that already expose
`front_page.supporting_surfaces`, such as:

- `system_compiler.witness_bundle/v0`
- `system_compiler.biography/v0`
- `system_compiler.biography_index/v0`
- `system_compiler.biography_index_compare/v0`
- `system_compiler.world_shelf_review/v0`

It does not create new facts.

It answers a narrower question:

- if a tool opens one root summary first
- which supporting surfaces should it follow next
- which visited summaries also expose machine-readable `route_provenance`
- where does that route expand cleanly
- where does it revisit an already opened summary
- where does it close a real cycle

That makes it a useful bridge between the current artifact layer and a future
stronger explain surface.

## Current shape

Current `system_compiler.front_page_route` includes:

- schema
  - `schemas/system_compiler.front_page_route.v0.schema.json`
- exporter
  - `scripts/export_system_compiler_front_page_route.py`
- validator
  - `scripts/validate_system_compiler_front_page_route.py`
- smoke
  - `scripts/system_compiler_front_page_route_smoke.ps1`

## Current outputs

By default the exporter leaves behind:

- `front-page.route.summary.json`
- `front-page.route.report.md`
- `front-page.route.check.txt`

When the route is emitted by the root witness flows instead of by a manual
one-off export, the default output root is:

- `<witness-output-root>/front_page_route/`

That integrated flow also leaves behind:

- `front-page.route.export.log`
- `front-page.route.validate.log`

## Witness bundle integration

The minimal-kernel system-compiler witness flows now treat
`front_page_route` as a first-class consumer artifact.

- `scripts/minimal_kernel_runtime_system_compiler_witness_bundle.ps1`
  exports and validates a default route object after the root witness
  `front_page` has been refreshed with the currently available supporting
  surfaces
- `scripts/ci_minimal_kernel_runtime_system_compiler_witness_bundle.ps1`
  and `scripts/ci_minimal_kernel_runtime_system_compiler_world_compare.ps1`
  re-run that route export after the final `world_shelf_review` front-page
  mutation, so the consumer route stays aligned with the last published root
  summary

This is intentionally kept as a separate sidecar artifact.

- the route overlay is not appended back into the root witness `report.md`
  or `check.txt`
- wrapper flows are free to update the root `front_page` late in the pipeline
  without leaving a stale embedded route snapshot behind

## What the route records

The route summary currently records:

- the selected root surface
- preorder traversal order across reachable `front_page` edges
- per-entry `route_provenance_count`
- flattened `route_provenance_entries` observed while opening visited summaries
- actual summary schema and kind loaded at each stop
- `revisit` versus `cycle`
- how many supporting surfaces each stop exposes
- how many route-provenance owners and lower front-page roots were observed
- route-level counts such as:
  - `entry_count`
  - `unique_summary_count`
  - `repeated_entry_count`
  - `cycle_entry_count`
  - `leaf_entry_count`
  - `max_depth`

The traversal semantics stay intentionally conservative.

- graph expansion still follows only `front_page.supporting_surfaces`
- `route_provenance` is consumed as a sidecar witness of which lower
  front-page roots that summary actually used while assembling itself

That keeps consumer behavior stable while still lifting lower-layer route
evidence into one higher-level object.

The distinction between `revisit` and `cycle` is important.

- `revisit` means the route reached a summary that had already been seen before
- `cycle` means the route reached a summary that is still active on the current
  ancestry chain

For example:

- `witness bundle -> biography -> witness bundle` is a cycle
- `world shelf review -> candidate shelf` and later
  `world shelf review -> baseline shelf` can be a revisit without being a cycle

And if that `world shelf review` summary also emits `route_provenance`, the
route object can now record that it actually consumed:

- `candidate_shelf`
- `shelf_compare`
- `baseline_shelf`

without pretending those provenance records are new traversal edges by
themselves.

## Manual example

```powershell
python ./scripts/export_system_compiler_front_page_route.py `
  --summary cmake-build-codex-system-compiler-front-page-smoke/witness-ci-shelf/summary.json `
  --output-root cmake-build-system-compiler-front-page-route-smoke/witness-ci-shelf
```

Then validate the exported route:

```powershell
python ./scripts/validate_system_compiler_front_page_route.py `
  --summary cmake-build-system-compiler-front-page-route-smoke/witness-ci-shelf/front-page.route.summary.json
```

Or run the dedicated smoke across the four current minimal-kernel system
compiler entry worlds:

```powershell
./scripts/system_compiler_front_page_route_smoke.ps1 -Clean
```

Or inspect the route artifact that now appears automatically under a witness
flow output root such as:

```text
cmake-build-codex-witness-front-page-route-shelf/front_page_route/front-page.route.summary.json
```

## Relationship to explain surface

This object is intentionally modest.

It is not yet a full explain surface query engine.

Instead, it proves a more basic claim:

> A higher-level consumer can now start from one stable root summary and follow
> declared artifact surfaces while also preserving the lower route-provenance
> witness that those summaries emitted, without bypassing the artifact layer or
> hard-coding the lower object graph.

That makes `front_page_route` a good staging object for future explain surface
consumers, review tools, and route-aware CI summaries.
