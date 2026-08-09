# Vivid Layer Runtime v0

## 文档状态

- `status`: `supporting`
- `scope`: live UI、frozen render artifact、layer admission 与 compose 生命周期
- `authority`: [`layer_runtime.cppm`](../../Modules/ui/vivid/core/layer_runtime.cppm)、
  [`scene_layer_support.cppm`](../../Modules/ui/vivid/core/scene_layer_support.cppm)

本文记录 Layer Runtime 的稳定边界，不复制 C++ 类型、当前 Player 转场状态、日志字段或落地排期。

## 责任边界

Layer Runtime 位于 scene record/execute 与 backend 输出之间：

```text
live widget tree
    -> render artifact capture
    -> admission / ownership
    -> compose / replay
    -> backend
```

它负责 render artifact 的 capture、validate、compose/replay、stale 和 release；不拥有产品页面状态、
navigation 决策、backend 设备资源或 motion recipe。

页面内 `PageLayers`（Backdrop/Content/Chrome/Popup 分区）与 runtime layer 不是同一概念。前者组织页面
绘制，后者管理 live root 和 frozen artifact 的跨帧生命周期。

## Layer 与 Artifact

### PageLayer

`PageLayer` 关联 live root、当前 snapshot、生命周期状态和失效信息。

- `hide/show` 只改变可见性；
- `freeze` 请求捕获 render artifact；
- `thaw` 回到 live root；
- transition/cancel 必须显式处理 snapshot ownership；
- release 后旧 handle/generation 不得继续使用。

freeze 不隐含 hide，调用方必须明确是否隐藏 live root。失败的 capture 不能留下半拥有的 payload 或
错误的 page truth。

### CommandBuffer Snapshot

命令快照冻结 record 结果，replay 时仍依赖 scene 的执行环境、资源和 epoch。相关 layout/style/theme 或
资源语义失效后必须拒绝 stale replay，不能把“命令仍可解析”当作“页面仍正确”。

它占用较少 payload，但每次显示仍需执行命令。当前 replay 只支持 identity transform，并允许 target bounds
作为 clip；任何平移或非 255 opacity 都必须在命令执行前返回 `UnsupportedTransform`，不能产生部分像素写入。

### PixelSurface Snapshot

像素快照是捕获时的 frozen pixel fact。live tree 后续变化不会自动改写该像素；只有 payload 丢失、
generation 失配、显式 stale 或 compose 前置条件失败时才拒绝。

它需要明确 pixel format、stride、bounds、alignment 和容量。offset、clip、opacity/alpha 语义以 source
实现和对应测试为准，不能从 backend blit 能力反推 Layer Runtime 已支持。

### Fallback

容量不足、artifact 不受支持或 stale 无法重建时，可以选择 command fallback、static cut、direct redraw
或 reject。选择由 profile、budget、backend capability 和调用方可执行路径共同决定，必须记录原因。

`TileSurface` 或其它 snapshot kind 在进入 source、测试和 ownership 证据前不属于 v0 保证。

## 生命周期闭环

最小闭环只有四项：

```text
capture -> compose/replay -> invalidate -> release
```

典型 transition：

1. admission 在分配/capture 前选择可执行形态；
2. source/destination 按选择结果捕获或保持 live；
3. 每帧只消费已拥有且有效的 artifact；
4. commit 提交目标 page truth；
5. cancel、interrupt 和 failure 释放所有 snapshot，并恢复可重入状态。

任何路径都不得因 early return 泄漏 payload slot、保持隐藏 live root 或留下无法释放的 handle。

## Stale 与 Invalidation

stale 判定取决于 artifact kind，而不是一个全局布尔值：

- CommandBuffer 依赖 replay 所需 epoch 和资源；不匹配时拒绝。
- PixelSurface 保留捕获时像素；live tree 更新不自动使其不可绘制。
- content/style/layout/focus 的变化必须先映射为具体 invalidation impact，再决定 live repaint、re-capture
  或继续使用 frozen artifact。
- profile 可以决定 rebuild、fallback 或 cut，但不能绕过 handle/payload validity。

具体 epoch 字段和状态枚举属于 source，不在文档维护第二份列表。

## Admission 与 Budget

预算分两阶段：

| 阶段 | 目的 |
|---|---|
| pre-capture admission | 在分配前判断 PixelDouble、PixelSingle、CommandSnapshot、StaticCut 或 Reject 是否可行 |
| post-capture arbitration | 用实际 bytes、compose pixels 和 capture 结果确认或降低 effective profile |

profile 是请求，不是保证。预算必须覆盖 payload slot、格式/stride、同时存活层数和 compose 工作量；
fallback reason、峰值与释放结果需要可观察。页面或 motion 代码不能绕过 admission 直接申请全屏 surface。

## Motion 边界

Layer Runtime 提供 artifact 与 compose 生命周期，Motion 提供时间、recipe、transform 和 transition
事务。begin/commit/cancel/interrupt 规则见
[`vivid_motion_runtime_v0.md`](vivid_motion_runtime_v0.md)。

Pattern 和页面只声明 motion eligibility/intent，不选择 snapshot kind 或 backend fast path。

## Evidence

Layer evidence 至少覆盖：

- capture 成功与容量耗尽；
- stale/generation/payload 拒绝；
- compose/replay 的 clip、offset、opacity 和 unsupported 分支；
- commit、cancel、interrupt 后 snapshot 全部释放；
- pre-capture rejection 没有发生隐藏分配；
- budget 请求、effective 结果和 fallback reason。

推荐 fixture 和 stdout 入口见 [`vivid_evidence_lab_manifest_v0.md`](vivid_evidence_lab_manifest_v0.md) 与
[`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)。Player 或某个 demo 的成功不能代替
Layer Runtime 的负例和生命周期证据。

## 非目标

- 不建立窗口管理器、任意层级 compositor 或多窗口 scene graph。
- 不承诺 shared-element、irregular mask、GPU backend 或任意 transform。
- 不把 PixelSurface cache 当作所有设备的默认策略。
- 不用页面专用转场或日志字段定义 Layer Runtime API。
- 不在本文维护当前实现清单、性能结论或后续步骤。
