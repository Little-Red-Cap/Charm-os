# Structured View Model v1

> status: `contract`

目标：用最小公共模型约束结构控件，避免 ListView/menu_tree/TableView 各自长出半内核。

## 正式接口（v1）

必须使用以下四件套作为公共模型层语义：

- `StructuredDataProvider`
- `StructuredSelectionModel`
- `StructuredVisibleRange`
- `StructuredViewportMapper`

## 行模型原则（v1）

- 统一按 **row-oriented** 处理。
- `TableView` 的列信息是行内附加数据，不引入二维模型。
- `menu_tree` 的子菜单能力属于 **菜单扩展协议**，不得污染基础接口。
- menu-backed list surface 如果需要 branch/group/disabled 语义，应优先复用 `StructuredMenuView::row_flags()` 与共享 `kStructuredListRowFlagGroup` / `kStructuredListRowFlagDisabled`，不要在 helper 里手写私有位值。

## 禁止事项（v1）

- 不混主题（tokens/Style/ThemePreset）。
- 不混绘制（draw_cmd/render/canvas）。
- 不混 payload/pool/handle 生命周期。
- 不做二维框架。
- 不做完整虚拟化系统（row cache/recycling/diff/page）。

## Runtime 与回归边界

`TableView`、`TreeView` 和 `ListItem` 必须保持各自 payload ownership：创建 table/tree 只能推进对应 payload
peak，不能把 `ListItem` 当作隐式 backing store。scroll、style 和 recorder probe 不得产生新的 collection
payload allocation；allocation failure、payload overflow 与 text overflow 都是硬失败。

当前 table/tree regression 将以下交互视为 paint-only：header/body scroll、horizontal scrollbar
page/back/clamp/drag、fixed-width scroll、tree scroll 和纯视觉 header style change。结构或 text/content 变化
仍可要求 layout，不能由该 regression 推导“所有 TableView mutation 都是 paint-only”。

证据入口是 `Examples/ui/vivid/soa_demo --soa-ci --regress-ui`。`table_tree_ok=1` 只表示 payload、
invalidation、geometry/recorder 和 command failure 本地 segment 闭合；它不声明 Evidence Lab `AxisCausal`，
也不替代产品视觉或 screenshot gate。recorder probing 的例外见
[`vivid_draw_cmd_evidence_boundary_v0.md`](vivid_draw_cmd_evidence_boundary_v0.md)。
