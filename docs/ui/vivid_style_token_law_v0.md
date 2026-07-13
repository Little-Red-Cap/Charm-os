# Vivid Style Token Law v0

> status: `contract`

本文定义 Vivid style token 的最小运行时法律：

```text
semantic token
  -> role patch / resolved style
  -> state mask
  -> invalidation impact
  -> render artifact evidence
```

它不要求一次 token 化所有视觉数值，只要求 token、widget role、状态组合和失效影响可解释、可审计。

## Semantic Token

token 名称表达 UI 意图，不表达某个固定 RGB 或像素值。例如 `accent`、`on_accent`、`surface`、
`outline`、`focus_ring`。页面临时几何和产品专属视觉值不得仅通过改名伪装成 framework token。

组件通过 `StyleRolePatch` 连接 style 字段与 `StyleRole`，不直接依赖具体 palette 数值：

```text
Button.bg_color     -> Accent
Button.font_color   -> OnAccent
Button.border_color -> Accent
```

这样 theme 变化经 `StyleSheet` 重新解析，而不是在页面逻辑中散布颜色选择。

## State Mask

进入 resolved style/cache 的状态必须有明确边界：

```text
hovered / pressed / disabled -> ordinary style state mask
focused                      -> focus/navigation artifact
```

focus 不能为了组合方便而扩大普通 Button style mask。完整边界见
[`vivid_focus_evidence_boundary_v0.md`](vivid_focus_evidence_boundary_v0.md)。state mask 的当前源码事实见
[`style_evidence.cppm`](../../Modules/ui/vivid/core/style_evidence.cppm)。

## Invalidation Impact

token domain 必须声明最小影响层级：

| domain | required impact |
|---|---|
| color、decoration | paint |
| spacing、radius、border width | paint + possible layout |
| font | paint + text metrics + layout |

具体枚举和判定表由
[`style_impact.cppm`](../../Modules/ui/vivid/core/style_impact.cppm) 定义。文档不复制枚举清单；行为变化时必须
保持以下约束：

- paint-only change 不得声称 layout changed；
- metrics-affecting change 不能只做 repaint；
- radius/border 是否触发布局由实际 box model 决定，不能全局假定；
- dirty scope 应限制在真实受影响的 widget/component/page 范围。

## Resolved Evidence

resolved style 必须能提供稳定摘要，以区分 palette 变化与 metrics 变化。摘要至少能关联 token/stylesheet
version、state mask、style key、color evidence、metrics evidence 与 impact verdict。

对 color-only token change，证据链应满足：

```text
semantic token changed
resolved color/style key changed
metrics remained stable
impact=paint_only
render artifact changed within claimed bounds
```

单独的 version increment、hash difference 或 screenshot 都不足以证明这条链。final `causal_chain`
必须由 required segments 推导，并遵守
[`vivid_causal_verdict_law_v0.md`](vivid_causal_verdict_law_v0.md)。

## 证据入口

`Examples/ui/vivid/style_token_law_demo` 验证 semantic token、role patch、state mask、impact decision、resolved
evidence 与 bounded render consequence。具体 case 数、stdout token 和 helper 名称由源码、manifest 与测试定义，
不在本文复制。
