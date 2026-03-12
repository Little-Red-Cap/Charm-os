# Structured View Model v1（最小规范片段）

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

## 禁止事项（v1）

- 不混主题（tokens/Style/ThemePreset）。
- 不混绘制（draw_cmd/render/canvas）。
- 不混 payload/pool/handle 生命周期。
- 不做二维框架。
- 不做完整虚拟化系统（row cache/recycling/diff/page）。
