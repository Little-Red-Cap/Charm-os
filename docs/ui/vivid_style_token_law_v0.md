# Vivid Style Token Law v0

## 2026-05 补记：Focus Boundary

Focus 的运行证据不继续塞进 `style_token_law_demo`。`Examples/ui/vivid/focus_boundary_demo` 与 `docs/ui/vivid_focus_evidence_boundary_v0.md` 专门承接 focus boundary：它证明 focused 不进入普通 Button style mask，但会通过 focus ring 改变 render artifact，并且 clear focus 后回到 baseline。

本文定义 Vivid Style Token Law 的第一版最小边界。

它的目标不是把所有视觉数字一次性 token 化，而是先把 style 从“局部数值选择”推进为可审计的运行时事实：

```text
semantic token
  -> role patch / resolved style
  -> state mask
  -> invalidation impact
  -> render artifact evidence
```

## 定位

Style Token Law 属于 Vivid Policy Plane。

它回答：

```text
这个颜色 / 尺寸 / 字体为什么存在？
它是语义 token，还是页面临时数值？
状态变化是否参与 style mask？
token 变化应该触发 paint、layout、text_metrics 还是 render_cache？
resolved style 是否能被证据链审计？
```

## v0 法律

### Law 1：Token 是语义，不是数值

token 名字必须表达 UI 意图，而不是只表达具体颜色或像素值。

例如：

```text
accent
on_accent
surface
surface_variant
outline
focus_ring
```

v0 允许 `ThemeTokens` 继续保持紧凑结构，但 demo 与文档必须按语义说明 token，而不是只说 RGB 值。

### Law 2：Role patch 连接 token 与 widget

组件不应直接关心 `rgba{...}`。

v0 使用 `StyleRolePatch` 把 widget style 字段连接到 `StyleRole`：

```text
Button.bg_color     -> Accent
Button.font_color   -> OnAccent
Button.border_color -> Accent
```

这让 theme token 变化可以通过 `StyleSheet` 重新解析，而不是散落在页面逻辑中。

### Law 3：State mask 必须有边界

Vivid 需要明确哪些状态进入 style cache / resolved style。

v0 先确认：

```text
hovered / pressed / disabled -> style state mask
focused                      -> 暂不进入普通 Button style mask
```

focus 未来应优先进入 focus ring / navigation / accessibility evidence，而不是无边界扩大普通 style cache 组合。

`charm.core.style_evidence` 提供 v0 的 state evidence：

- `StyleStateEvidence` 记录 `mask / state_count / includes_hovered / includes_pressed / includes_disabled / includes_focused`。
- `make_style_state_evidence()` 从 `WidgetKind` 读取当前 style state mask。
- `style_state_evidence_matches_interactive_law()` 验证 interactive widget 的普通 style mask 包含 hovered / pressed / disabled，但不包含 focused。

### Law 4：Token 必须声明 invalidation impact

token 变化必须能说明影响层级。

v0 先采用最小分类：

```text
color token  -> paint_only
spacing      -> layout
font         -> text_metrics + layout
radius       -> paint_only 或 layout，取决于 box model
```

如果 token 只改变颜色，demo 应证明 metrics 不变，并声明 `impact=paint_only`。

`charm.core.style_impact` 提供 v0 的最小判定表：

- `StyleTokenDomain::Color` -> `PaintOnly`
- `StyleTokenDomain::Spacing` -> `PaintOnly | Layout`
- `StyleTokenDomain::Font` -> `PaintOnly | TextMetrics | Layout`
- `StyleTokenDomain::Radius` -> `PaintOnly | Layout`
- `StyleTokenDomain::BorderWidth` -> `PaintOnly | Layout`
- `StyleTokenDomain::Decoration` -> `PaintOnly`

### Law 5：ResolvedStyle 必须可审计

StyleSheet 解析结果应能输出稳定摘要：

```text
token_version
stylesheet_version
style_state_mask
style_key
color_hash
metrics_hash
state_count
impact
```

v0 使用 stdout evidence 证明 token 变化会改变 resolved color / render artifact，但不改变 metrics。

`charm.core.style_evidence` 提供 v0 的最小证据压缩：

- `make_resolved_style_evidence()` 产出 `color_hash / metrics_hash / style_key`。
- `style_color_evidence_equal()` 比较 resolved color 证据。
- `style_metrics_evidence_equal()` 比较 metrics 证据。
- `style_evidence_matches_impact()` 检查证据变化是否符合 impact 判定。

## 首个落点

`Examples/ui/vivid/style_token_law_demo` 是 Style Token Law v0 的第一条运行证据。

它验证：

- semantic token 以 `accent / on_accent` 形式进入 Button role patch。
- Button style state mask 包含 hovered / pressed / disabled，但不包含 focused。
- accent token 变化后 `token_version` 增加，`stylesheet_version` 不需要变化。
- resolved style `color_hash / style_key` 变化，但 `metrics_hash` 不变。
- color token 变化经 `decide_style_token_impact(Color)` 判定为 `paint_only`，并产生新的 render artifact。

核心 stdout 字段：

```text
mask=<mask>
hovered=1
pressed=1
disabled=1
focused_in_style_mask=0
law=interactive_without_focus
style_key=<hash>
color_hash=<hash>
metrics_hash=<hash>
impact=<impact_name>
impact_mask=<mask>
color_changed=1
metrics_same=1
artifact_delta=1
dirty_within_component=1
```

这些字段证明：

- hovered / pressed / disabled 进入普通 style state evidence。
- focused 被保留在普通 style mask 外，后续应进入 focus ring / navigation evidence。
- style evidence 变化发生在 color 层。
- metrics evidence 保持不变。
- impact resolver 的 `paint_only` 裁决有证据支撑。
- render artifact 随 color token 变化而变化。

stdout 遵守 `vivid_evidence_stdout_law.md`。
