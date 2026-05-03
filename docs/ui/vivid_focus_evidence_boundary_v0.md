# Vivid Focus Evidence Boundary v0

本文定义 Vivid v0 对 `focused` 的最小证据边界。

## 定位

`focused` 是输入导航与可访问性语义，不是普通 Button style cache 的扩展维度。

v0 先立一条硬边界：

```text
hovered / pressed / disabled -> ordinary style state evidence
focused                      -> focus ring / navigation evidence
```

这条边界的目标不是否认 focus 可以影响视觉，而是避免 focus 无边界进入普通 style mask，导致 style 组合爆炸、cache key 膨胀，以及焦点导航语义和视觉派生语义混在一起。

## v0 法律

### Law 1：focused 不进入普通 Button style mask

`StyleStateEvidence` 必须证明 Button 的普通 style mask：

```text
includes_hovered=1
includes_pressed=1
includes_disabled=1
includes_focused=0
```

### Law 2：focused 改变 artifact，不改变 resolved style evidence

同一个 Button 在 normal 与 focused lookup 下，`ResolvedStyleEvidence` 应保持一致：

```text
style_key same
color_hash same
metrics_hash same
```

focus 变化后的视觉差异由 focus ring / render artifact 承接，而不是由普通 Button resolved style 承接。

### Law 3：focus state delta 是 paint-only intent

v0 将 focus 状态变化声明为：

```text
source=programmatic
invalidation=paint_only
artifact=focus_ring
```

后续如果接入键盘导航、遥控器或 accessibility source，可以扩展 `source` 字段，但不改变 focus boundary。

### Law 4：focus clear 必须回到 baseline artifact

当 focus 被清除时，命令证据与像素证据应回到未聚焦基线：

```text
cmd_hash baseline
pixel_hash baseline
```

这证明 focus ring 是可进入、可退出、可审计的 overlay artifact。

## 首个落点

`Examples/ui/vivid/focus_boundary_demo` 是 Focus Evidence Boundary v0 的第一条运行证据。

它验证：

- Button style mask 包含 hovered / pressed / disabled，但不包含 focused。
- normal 与 focused style lookup 产生相同的 `style_key / color_hash / metrics_hash`。
- `set_focused(true)` 后 draw command evidence 与 render artifact 发生变化。
- focus repaint 的 dirty rect 保持在 Button bounds 内。
- `set_focused(false)` 后 artifact 回到 baseline。

stdout 遵守 `vivid_evidence_stdout_law.md`，最终 CTest 约束：

```text
[fb] run=focus_boundary_demo phase=end result=ok cases=6
```

## 后续方向

Focus Evidence Boundary v0 只证明单 Button 的 focus ring 边界。

后续可以继续扩展：

- navigation focus source：Tab / d-pad / rotary encoder。
- focus transfer evidence：old focus out + new focus in。
- component focus scope：focus ring 不越出 component boundary。
- accessibility evidence：focus truth 与 semantic focus target 对齐。

## 2026-05 补记：Focus Transfer

`Examples/ui/vivid/focus_transfer_demo` 已将 focus evidence 从单点 focus ring 推进到迁移事务：真实 input dispatch 产生 `FocusOut(old)` 与 `FocusIn(new)`，`input_focused` 提交到新 target，style evidence 保持稳定，render artifact 迁移到 destination。详细法律见 `vivid_focus_transfer_evidence_v0.md`。
