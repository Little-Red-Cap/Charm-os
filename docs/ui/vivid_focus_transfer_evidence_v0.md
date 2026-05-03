# Vivid Focus Transfer Evidence v0

本文定义 Vivid v0 对焦点迁移的最小证据。

## 定位

`Focus Evidence Boundary v0` 证明了单个控件的 `focused` 不进入普通 style mask，而是通过 focus ring 改变 render artifact。

`Focus Transfer Evidence v0` 继续向前一步：证明焦点可以从一个 focusable target 迁移到另一个 target，并留下可审计的事件、truth 与 artifact 证据。

## v0 法律

### Law 1：transfer 必须发出 FocusOut + FocusIn

当已有 focus target，新的 focusable target 收到 press 时，输入链应产生：

```text
FocusOut(old target)
FocusIn(new target)
```

v0 允许同一 dispatch 内同时存在原始 pointer event，例如 `MouseDown`，但 `FocusOut / FocusIn` 必须可从 `SceneAccess::input_event()` 读取。

### Law 2：focus truth 必须提交到 new target

transfer 后：

```text
input_focused == new target
```

这条 evidence 证明焦点迁移不是单纯的事件通知，而是 kernel input truth 已提交。

### Law 3：style evidence 保持稳定

focus transfer 不应扩展普通 style mask。

v0 继续要求：

```text
focused_in_style_mask=0
style_same=1
```

### Law 4：artifact 迁移由像素证据证明

当 source 与 destination 是同构控件时，draw command shape 可能相同，`cmd_hash` 不一定变化。

因此 v0 使用 `pixel_hash` / `dirty_hash` / `target` 共同证明：

```text
old target focus artifact
new target focus artifact
artifact_changed=1
focus_ring=1
```

## 首个落点

`Examples/ui/vivid/focus_transfer_demo` 是 Focus Transfer Evidence v0 的第一条运行证据。

它使用两个 `ScrollContainer`，因为它们由 factory 明确设为 focusable，且 press 不会改变 checked/value/selected 等业务 truth，适合验证纯 focus transfer。

stdout 最终约束：

```text
[ft] run=focus_transfer_demo phase=end result=ok cases=7
```

核心字段：

```text
focus_out=1
focus_in=1
focus_out_source=1
focus_in_destination=1
transfer_committed=1
style_same=1
artifact_changed=1
```

## 后续方向

下一步可以把 transfer evidence 推到更高层：

- keyboard / d-pad navigation source。
- focus scope：已由 `vivid_focus_scope_evidence_v0.md` 与 `Examples/ui/vivid/focus_scope_demo` 承接第一版 runtime focus admission / inside dispatch allow / outside dispatch reject / no-leak artifact。
- focus trap：modal / popup 内焦点不泄漏。
- accessibility focus：semantic focus target 与 visual focus artifact 对齐。
