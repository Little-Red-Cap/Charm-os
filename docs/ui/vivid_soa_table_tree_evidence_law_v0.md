# Vivid SoA Table/Tree Evidence Law v0

This document defines the v0 evidence boundary for the SoA table/tree regression path in `Examples/ui/vivid/soa_demo`.

The purpose is narrow: make the recently strengthened TableView checks legible as Evidence Plane work without adding a new Evidence Lab manifest demo, changing Vivid core API, or promoting recorder helper code.

## Positioning

`soa_demo` remains a SoA runtime and renderer smoke sample. Its table/tree regression is a structured-collection evidence path:

```text
SoaFactory / SoaKernel collection setup
  -> TableView / TreeView payload allocation
  -> style / scroll / scrollbar mutation
  -> layout-vs-paint invalidation evidence
  -> recorder-level DrawCmd probes
  -> soa-ci summary verdict
```

This path is not a page transaction, not a semantic-action request, and not a screenshot approval gate.

The current guarded surface is:

```text
Examples/ui/vivid/soa_demo --soa-ci --regress-ui
[soa] table/tree regression OK
[soa-ci] ... table_tree_ok=1 ... ok=1 ...
```

The table/tree path is intentionally kept outside `vivid_evidence_lab_manifest_v0.md` for now. It feeds the broader SoA CI verdict instead of increasing the Evidence Lab manifest case count.

## Evidence Segments

The v0 table/tree evidence path must stay composed from observable runtime facts.

### Payload Lifecycle

The regression may observe SoA payload stats:

```text
table_view.peak
tree_view.peak
list_item.peak
table_view.alloc_fail
tree_view.alloc_fail
overflowed
text_overflowed
```

Rules:

- Creating one `TableView` and one `TreeView` should advance only their own payload peaks.
- `ListItem` payload usage must not grow as an accidental backing store for `TableView` or `TreeView`.
- Scroll, style, and recorder probes must not allocate new collection payloads.
- Allocation failures and payload/text overflows are hard failures.

### Invalidation Boundary

Table/tree interaction evidence should keep layout and paint consequences separate:

```text
header style change -> paint-only
body wheel scroll   -> paint-only
header wheel scroll -> paint-only horizontal movement
hscroll interaction -> paint-only horizontal movement
fixed width scroll  -> paint-only
tree wheel scroll   -> paint-only
```

Rules:

- Scroll and visual style changes must not trigger full layout in this regression path.
- Paint invalidation must be observed when visible collection state changes.
- Text/content or structural changes may still require layout in other paths; this law does not claim all TableView mutations are paint-only.

### Geometry And Recorder Evidence

The table path may use recorder-level probes to verify collection-specific rendering facts:

```text
header/body column alignment
header fill style sequence
header-only / body / full column divider placement
horizontal scrollbar page/back/clamp/drag behavior
failed_cmds == 0
```

These probes are allowed because the test target is the SoA collection recorder itself.

This is not a general product UI evidence pattern. Product-level render evidence should prefer the boundary defined by `vivid_draw_cmd_evidence_boundary_v0.md`:

```text
Scene / SoaGui stats
DrawCmd stats
exec stats
dirty or artifact summaries
```

## Allowed DrawCmd Probing

The table/tree regression may decode a temporary `DefaultDrawCmdBuffer` to collect text and fill probes when it is verifying recorder behavior.

Allowed:

```text
collect text positions for header/body alignment
collect fill rects for header style shape
assert executor failed_cmds == 0
summarize command count / bytes / batch stats in soa-ci
```

Forbidden:

```text
make CmdHeader layout a product UI contract
treat payload byte layout as TableView semantics
use executor dispatch grouping as visual approval
promote recorder probe helpers into Vivid core
treat pixel_hash or screenshots as the sole table correctness proof
```

## Summary Verdict

The current final verdict is the broader `soa-ci` line, not a `causal_chain` Evidence Lab row:

```text
table_tree_ok=1
ui_ok=1
failed_cmds=0
overflows(p/t/b)=0/0/0
alloc_fail=0
cmd_budget=1
ok=1
```

`table_tree_ok=1` means the structured collection path closed its local evidence segments. It does not by itself claim `AxisCausal` in the Evidence Lab manifest.

If table/tree evidence is later promoted into a CTest-gated Evidence Lab demo, the promotion must update:

```text
vivid_evidence_stdout_law.md
vivid_evidence_lab_manifest_v0.md
Examples/ui/vivid/evidence_lab_manifest_demo
docs/ui/README.md
```

## Relationship To Other Laws

`structured_view_model_v1.md` defines the row-oriented model boundary that keeps `TableView` from becoming a separate two-dimensional framework.

`vivid_widget_state_observe.md` defines why SoA `SceneAccess` and object-level widget `observe_*` are different surfaces.

`vivid_draw_cmd_evidence_boundary_v0.md` defines the default DrawCmd observation boundary; this law records the narrow recorder-regression exception used by `soa_demo`.

`vivid_render_evidence_chain_v0.md` governs product-facing state-to-render evidence chains. Table/tree probes should only become product evidence when they are converted into stable stats or artifact summaries.

## Non-Goals

- This law does not add a new demo or manifest entry.
- This law does not change `soa_demo` stdout.
- This law does not define a public TableView product API.
- This law does not promote `SoaGui` recorder helpers into core.
- This law does not replace screenshot, replay, or DrawCmd internal regression tooling.
