# Vivid Scene Support Boundary v0

This document defines the v0 boundary for the internal support layers now split out from `charm.ui.scene`.

`Scene` remains the public runtime surface. The extracted support modules help keep implementation legible, but they do not create new product-facing entry points by themselves.

## Positioning

The public surface remains:

```text
charm.ui.scene
```

Current internal support layers are:

```text
charm.ui.scene.builder_support
charm.ui.scene.layer_support
scene:render_detail
```

Their purpose is separation of responsibility, not API proliferation.

## Layer Roles

### Builder Support

`builder_support` owns scene construction helpers and builder-facing aliases.

It is the place for:

```text
SceneBuilder
SceneAccess
widget construction helpers
semantic request / focus aliases re-exported for scene construction
builder-side convenience types
```

It should not own:

```text
render execution
snapshot replay
pixel blending
artifact evidence formatting
```

### Layer Support

`layer_support` owns layer-runtime-adjacent storage and scene-visible stats/value types.

It is the place for:

```text
CmdStats / ExecStats / TileStats / TileConfig
LayerCaptureResult / LayerReplayResult
command snapshot payload storage
pixel snapshot payload storage
layer replay support types
```

It should not own:

```text
widget tree mutation
semantic request policy
demo-side evidence helpers
product-side page truth policy
```

### Render Detail

`scene:render_detail` owns scene-private render conversion and composition details.

It is the place for:

```text
DrawCmd stats -> Scene stats conversion
snapshot pixel decode/blend helpers
scene-private render glue
```

It should not become:

```text
a new public rendering API
a demo evidence API
a second draw_cmd surface
```

## Public Boundary

Product code and Evidence Lab should continue to treat `Scene` as the boundary:

```text
Scene::build / access / dispatch / render
Scene::last_cmd_stats()
Scene::last_exec_stats()
Scene::layer_stats()
```

Rules:

- Product UI code should not import `builder_support` or `layer_support` as independent architectural concepts unless it is already working through `Scene`.
- Evidence Plane should observe scene-level facts, not `scene:render_detail` internals.
- Internal support layers may be refactored again without changing the public runtime story, as long as `Scene` contracts remain stable.

## Evidence Relationship

This split matters to Evidence Plane in one specific way:

```text
DrawCmd / render evidence still belongs at the Scene boundary.
```

`vivid_draw_cmd_evidence_boundary_v0.md` already defines that render evidence observes:

```text
Scene::last_cmd_stats()
Scene::last_exec_stats()
dirty / artifact summaries
```

This law extends that idea upward:

```text
support-layer refactors do not automatically become new evidence surfaces
```

## Non-Goals

- This law does not introduce a new demo.
- This law does not promise `builder_support` or `layer_support` as stable standalone product APIs.
- This law does not replace `vivid_draw_cmd_evidence_boundary_v0.md`.
- This law does not move demo helpers into core.
