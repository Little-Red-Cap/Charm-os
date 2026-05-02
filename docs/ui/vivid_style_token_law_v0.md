# Vivid Style Token Law v0

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

### Law 5：ResolvedStyle 必须可审计

StyleSheet 解析结果应能输出稳定摘要：

```text
token_version
stylesheet_version
style_state_mask
style_key
impact
```

v0 使用 stdout evidence 证明 token 变化会改变 resolved color / render artifact，但不改变 metrics。

## 首个落点

`Examples/ui/vivid/style_token_law_demo` 是 Style Token Law v0 的第一条运行证据。

它验证：

- semantic token 以 `accent / on_accent` 形式进入 Button role patch。
- Button style state mask 包含 hovered / pressed / disabled，但不包含 focused。
- accent token 变化后 `token_version` 增加，`stylesheet_version` 不需要变化。
- resolved style color / style key 变化，但 metrics 不变。
- color token 变化声明为 `paint_only`，并产生新的 render artifact。

stdout 遵守 `vivid_evidence_stdout_law.md`。
