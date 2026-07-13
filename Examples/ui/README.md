# UI 示例入口

## 文档状态

- `status`: `supporting`
- `scope`: Host Vivid fixture 路由
- `authority`: [`../../docs/ui/README.md`](../../docs/ui/README.md)、demo source、CMake/CTest

本目录只提供 fixture。行为、失败语义和 evidence field 由 UI 专题契约与当前测试定义，不在每个 demo
目录复制 README。

## 路由

| 目标 | 契约 | fixture |
|---|---|---|
| object widget truth/edge、SoA scene/helper | [`vivid_widget_state_observe.md`](../../docs/ui/vivid_widget_state_observe.md) | [`widget_state_demo`](vivid/widget_state_demo/)、[`widget_signal_demo`](vivid/widget_signal_demo/)、[`scene_state_demo`](vivid/scene_state_demo/)、[`dropdown_popup_demo`](vivid/dropdown_popup_demo/)、[`menu_tree_demo`](vivid/menu_tree_demo/) |
| structured row/list/table/tree | [`structured_view_model_v1.md`](../../docs/ui/structured_view_model_v1.md) | [`list_row_flags_demo`](vivid/list_row_flags_demo/)、[`soa_demo`](vivid/soa_demo/) |
| focus、semantic、motion、transaction、render evidence | [`Vivid Evidence Lab`](../../docs/ui/vivid_evidence_lab_manifest_v0.md) | [`vivid/`](vivid/) 下对应 CTest demo |
| fullframe/tile/text/theme/navigation | [`Vivid architecture`](../../Modules/ui/vivid/ARCHITECTURE.md) | [`fullframe_demo`](vivid/fullframe_demo/)、[`tile_demo`](vivid/tile_demo/)、[`text_demo`](vivid/text_demo/)、[`theme_demo`](vivid/theme_demo/)、[`nav_demo`](vivid/nav_demo/) |
| port template 与 assets | 当前 source/config | [`port_template`](vivid/port_template/)、[`assets`](vivid/assets/) |

`soa_demo` 的专用 run/dump/replay workflow 仍见
[`soa_demo/README.md`](vivid/soa_demo/README.md)。其它 fixture 的 target、参数和 pass gate 直接查看各目录
`CMakeLists.txt` 与 source。Host fixture 不替代产品或真实板证据。
