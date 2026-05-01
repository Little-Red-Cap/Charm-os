# Vivid Product UI Runtime Charter

本文是 Vivid 进入 `Runtime Spine v0` 后的方向宪章。

它不是 API 契约，也不替代 `ui_kernel_contract.md`、`vivid_replay_workflow.md` 或 `vivid_layer_runtime_v0.md`。它负责回答一个更上层的问题：

> Vivid 为什么不是 widget collection，而是 resource-governed product UI runtime。

## 定位

Vivid 的目标不是只提供按钮、列表、图片和卡片，而是让产品 UI 在不同资源宇宙中以可解释、可裁决、可验证的方式成立。

Vivid 需要回答：

```text
页面什么时候 live？
页面什么时候 frozen？
什么情况下允许双层 PixelSurface？
什么情况下只能 CommandSnapshot？
什么情况下直接 cut？
Motion 是否允许 alpha / slide / 连续时间？
页面重不重，证据在哪里？
```

## 四个平面

### Semantic Plane

语义平面回答：这个 UI 在产品语义上是什么。

典型对象：

- `Page`
- `Layer`
- `Pattern`
- `Component`
- `Focus`
- `Navigation`
- `Theme state`
- `Playback state`
- `Storage state`

规则：

- 页面文件应表达产品语义和装配关系。
- `Pattern` 应表达稳定产品模式，而不是后端绘制细节。
- `Pattern` 不应知道 `PixelSurface`、`DrawCmd`、SDL 或具体 backend。

### Artifact Plane

渲染产物平面回答：语义 UI 被 runtime 压成什么可执行或可合成产物。

典型对象：

- `DrawCmdBuffer`
- `CommandSnapshot`
- `PixelSurface`
- `TileSurface`
- `GlyphRun`
- `ImageRef`

规则：

- live tree 与 frozen artifact 必须分清。
- `freeze()` 是渲染策略，不是可见性。
- `hide()` 是可见性，不是冻结。
- artifact 可以被 replay、compose、release、invalidate。

### Policy Plane

策略平面回答：在当前设备、预算和后端能力下允许怎么做。

典型对象：

- `LayerProfile`
- `LayerBudget`
- `LayerAdmission`
- `MotionTier`
- `StyleTokenLaw`
- `BackendCaps`

规则：

- Profile 是请求，不是保证。
- Admission 发生在 capture 前。
- Budget 既是日志，也是裁决依据。
- Backend caps 只能被 runtime / policy 消费，不应泄漏到 Pattern。
- Motion recipe 必须服从 Profile / Budget / Admission。

### Evidence Plane

证据平面回答：性能、稳定性和可移植性是否有证据。

典型对象：

- `cmd_count`
- `layer_bytes`
- `composite_pixels`
- `alpha_pixels`
- `snapshot_count`
- `capture_count`
- `fallback_reason`
- `ui-ci`
- `dump/replay`
- screenshot / hash

规则：

- 没有 Evidence 的优化只能算猜测。
- 每个 runtime 机制都应尽量有 UI CI 或 replay 证据。
- fallback 不只要发生，还要可观察。
- snapshot 生命周期必须能证明无泄漏。

## 运行时脊柱

当前 Vivid Runtime Spine v0 的核心脊柱是：

```text
PageLayer
Snapshot
Profile
Budget
Admission
Compose
CI Evidence
```

它表达的链路是：

```text
Motion 是愿望
Budget 是法律
Profile 是裁决
Admission 是准入
Layer Runtime 是执行
CI 是审计
```

这条脊柱优先级高于单个控件 API 的漂亮程度。

## Layer Runtime 原则

### PageLayer

`PageLayer` 是 live root 与 frozen artifact 的生命周期边界。

规则：

- `PageLayer` 可以 show / hide live root。
- `PageLayer` 可以 freeze / thaw / release snapshot。
- `PageLayer` 不应退化成单纯页面可见性 helper。

### Snapshot

Snapshot 是 frozen render artifact。

规则：

- `CommandSnapshot` 依赖当前执行环境，必须严格处理 stale。
- `PixelSurface` 是 frozen pixel fact，stale 表示新鲜度，不等同于不可绘制。
- `TileSurface` 未来应服务更小内存 profile，而不是替代所有 snapshot。

### Admission

Admission 是 capture 前准入制度。

规则：

- 先裁决，再 capture。
- `PixelDouble` 只应在预算和 cache slot 都允许时发生。
- `PixelSingle` 是降级机会，不是默认替代双层路径。
- `CommandSnapshot` 是低内存路径，但需要 Motion / replay 支持。
- `StaticCut` 是合法运行形态，不是失败。
- `Reject` 表示当前 runtime 无可执行形态。

### Budget

Budget 是资源法律。

规则：

- pre-capture admission 使用预算防止不该发生的 capture。
- post-capture arbitration 使用真实 stats 纠偏。
- budget fail 必须带 fallback reason。
- debug / CI 应能看到预算账本。

## Managed UI Time

Vivid 的 Motion 不应只依赖页面局部的 `elapsed / duration`。

Vivid 应逐步进入托管 UI 时间：

```text
UI Runtime owns frame time.
Motion requests time.
Budget approves time.
Backend executes time.
```

不同 profile 下的时间形态不同：

```text
rich:
  连续帧，允许 alpha + slide

cheap:
  低帧率或关键帧，允许有限 opacity steps

static:
  无连续 UI 时间，只有 start/end

eink:
  时间表现为刷新阶段，而不是动画帧

none:
  无 motion
```

Motion recipe 的第一目标不是 API 好看，而是可降级、可审计、可被 runtime 托管。

当前 v0 已有最小核心：

- `charm.ui.scene.motion_time`
- `MotionTier`
- `MotionTimeSpec`
- `MotionTick`
- `sample_motion_time()`
- `charm.ui.scene.motion_plan`
- `LayerMotionSpec`
- `LayerMotionFrame`
- `sample_layer_motion()`
- `charm.ui.scene.motion_recipe`
- `MotionRecipe`
- `sample_motion_recipe()`
- `charm.ui.scene.motion_transition`
- `MotionTransitionRunner`
- `MotionTransitionTrace`
- `charm.ui.scene.motion_compose`
- `make_motion_compose_spec()`
- `Examples/ui/vivid/motion_time_demo`

这个核心只回答“时间如何被采样，recipe 如何投影成最小 `LayerTransform`，transition 如何托管 begin/sample/finish/cancel 生命周期，如何留下最小运行证据，以及如何生成 `LayerComposeSpec`”，暂不执行完整 layer compose 或页面转场。

## Pattern Layer

Pattern 是产品语义组件，不是绘图 helper。

候选模式：

- `TopBar`
- `BottomNav`
- `PathBar`
- `PillSurface`
- `MediaArtSlot`
- `ListCard`
- `MiniPlayer`
- `Sheet`
- `PopupMenu`
- `FocusRing`

规则：

- Pattern 表达语义、状态和布局关系。
- Pattern 不直接选择 PixelSurface / CommandSnapshot / backend。
- Pattern 可以声明 motion eligibility、fallback、theme role、focus role。
- Pattern 只有跨页面或跨产品稳定后才上收进 Vivid。

## Style Token Law

视觉常量应逐步离开页面局部魔法数，进入可验证的 token / layout / density 系统。

推荐分层：

```text
Global Token
Device Token
Product Token
Page Token
```

规则：

- 页面级 token 先稳定。
- 跨页面复用后升为 product token。
- 跨产品复用后升为 Vivid token。
- token 不只是整理数字，而是建立可迁移坐标系。

## 非目标

当前阶段不追求：

- 完整窗口管理器
- 任意层级 compositor
- 小型 Qt Quick
- 把所有 Player 私有模式上收
- Motion DSL 的表面优雅优先于裁决链

## 下一阶段建议

优先顺序：

1. 将 `Managed UI Time v0` 接入真实 Layer compose 执行路径
2. `Style Token Law`
3. `Component Lab / Screenshot CI`
4. `MediaArtSlot / CoverSlot`
5. `Static Reactive UI Source`

这些能力都应围绕 Runtime Spine 生长，而不是各自独立扩张。

## 当前结论

Vivid 的差异化不在于“比 LVGL 更现代”或“比 Qt 更轻”，而在于：

> 产品 UI 语义 + 嵌入式资源法律 + 可冻结渲染产物 + 可降级 Motion + 可审计性能证据 + PC/MCU 同构验证。

Vivid 的核心责任不是画 UI，而是让 UI 在不同资源宇宙中以可解释、可裁决、可验证的方式成立。
