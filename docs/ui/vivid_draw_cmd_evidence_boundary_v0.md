# Vivid DrawCmd Evidence Boundary v0

This document defines the v0 observation boundary between Vivid DrawCmd internals and the Evidence Plane.

`fa030f72` split `charm.gfx.draw_cmd` internally into `schema / buffer / executor` module partitions. That split is useful architecture, but it does not make partition internals part of evidence vocabulary. Evidence Lab should observe DrawCmd through stable scene-level stats and render artifacts unless a demo is explicitly an internal DrawCmd regression.

## Positioning

The public draw command entry remains:

```text
charm.gfx.draw_cmd
```

The internal implementation is organized as:

```text
schema
buffer
executor
```

The Evidence Plane observes the result as:

```text
Scene::last_cmd_stats()
Scene::last_exec_stats()
dirty summary
render artifact summary
```

This keeps Render Evidence focused on product/runtime facts rather than payload layout facts.

## Observable Evidence

Render evidence may use these stable summary fields:

```text
cmd_count
cmd_bytes
exec_cmds
failed_cmds
dirty_count
dirty_hash
cmd_hash
pixel_hash
```

Rules:

- `cmd_count` and `cmd_bytes` describe recorded draw intent size.
- `exec_cmds` and `failed_cmds` describe execution outcome at the scene evidence boundary.
- `dirty_count` and `dirty_hash` describe invalidation/render locality.
- `cmd_hash` is a stable stats/evidence summary, not a byte-for-byte command stream golden.
- `pixel_hash` is backend artifact evidence for the tested backend, not product visual approval by itself.

## Forbidden Coupling

Render Evidence must not depend on:

```text
CmdHeader binary layout
payload struct layout
draw_cmd_schema.cppm private organization
draw_cmd_buffer.cppm arena offsets
draw_cmd_executor.cppm dispatch grouping internals
executor batch implementation details as product semantics
```

Evidence docs may name `schema / buffer / executor` as architecture boundaries, but demos should not assert their private shape unless they are explicitly internal DrawCmd regression tests.

## Allowed Internal Regression Boundary

Internal DrawCmd regression tools may inspect command bytes, arena layout, replay encoding, or executor grouping when the test target is DrawCmd itself.

Those tests must not be used as product UI evidence unless they also expose scene-level summaries. Product UI evidence should remain:

```text
state / invalidation / dirty / cmd stats / exec stats / artifact hash / causal verdict
```

`Examples/ui/vivid/soa_demo` has one narrow recorder-level exception when the test target is TableView/TreeView
recording behavior. It may decode a temporary `DefaultDrawCmdBuffer` to inspect header/body text alignment, fill
shape, divider placement and scrollbar geometry, and must still assert `failed_cmds=0`.

This exception does not make `CmdHeader` layout, payload bytes or executor grouping a product UI contract. The probe
must remain in regression code and expose stable SoA CI summaries; it cannot become the sole product visual proof.
The collection model and payload/invalidation rules remain in
[`structured_view_model_v1.md`](structured_view_model_v1.md).

## Relationship To Render Evidence

`vivid_render_evidence_chain_v0.md` defines the broader state-to-artifact causal chain.

`vivid_evidence_vocabulary_law_v0.md` defines the field meaning for `RenderEvidence` and `RenderArtifactDeltaEvidence`.

`vivid_evidence_vocabulary_law_v0.md` keeps current render evidence helpers demo-side until a backend-neutral core shape exists.

This law adds the DrawCmd-specific boundary: Evidence Plane can rely on scene-level stats and artifact summaries, but not on partition-private encoding.

## Non-Goals

- This law does not add a new demo.
- This law does not change `charm.gfx.draw_cmd` public imports.
- This law does not promote `RenderEvidence` or `vivid_evidence_support.hpp` into Vivid core.
- This law does not define screenshot golden policy.
- This law does not forbid internal DrawCmd regression tests from inspecting internals.
