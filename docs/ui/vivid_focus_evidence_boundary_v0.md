# Vivid Focus Evidence Boundary v0

> status: `contract`

本文定义 Vivid v0 的 input focus 与 ordinary style state 边界：

```text
hovered / pressed / disabled -> ordinary style evidence
focused                      -> focus/navigation artifact
```

focus 可以改变视觉，但不能无边界进入普通 style mask。否则 style 组合、cache key、输入导航语义与视觉
派生语义会被耦合。

## Style 法律

Button 的 ordinary `StyleStateEvidence` 必须包含 hovered、pressed、disabled，不包含 focused。

同一 Button 在 normal 与 focused lookup 下，resolved style evidence 必须保持一致：

```text
style_key same
color_hash same
metrics_hash same
```

focus 视觉差异由 focus ring/render artifact 承接，不由普通 Button resolved style 承接。

## Invalidation 法律

focus state delta 是 paint-only intent：

```text
invalidation=paint_only
artifact=focus_ring
```

input source 可以是 programmatic、keyboard、d-pad、pointer 或 accessibility，但 source 扩展不得改变
style boundary。dirty impact 必须约束在受影响 target 的绘制边界内。

## Artifact 生命周期

focus ring 必须是可进入、可退出、可审计的 overlay artifact：

- set/transfer focus 后，render artifact 必须反映目标变化；
- clear focus 后，command/pixel evidence 必须回到 unfocused baseline；
- focus 变化不得伪造业务 state delta；
- final causal verdict 必须连接 style exclusion、stable resolved style、artifact mutation 与 clear-back。

causal verdict 资格见 [`vivid_causal_verdict_law_v0.md`](vivid_causal_verdict_law_v0.md)。跨 target 的提交
语义见 [`vivid_focus_transfer_evidence_v0.md`](vivid_focus_transfer_evidence_v0.md)。

## 证据入口

`Examples/ui/vivid/focus_boundary_demo` 验证单 target 的 style exclusion、paint-only invalidation、bounded
dirty artifact 与 clear-back-to-baseline。具体 case 数、stdout token 和当前 API 由 demo、manifest 与测试定义，
不在本文复制。
