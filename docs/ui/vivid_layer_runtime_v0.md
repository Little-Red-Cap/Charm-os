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

它占用较少 payload，但每次显示仍需执行命令。当前 replay 支持 identity、整数平移和整体 opacity：平移叠加到
Canvas 既有 origin，执行后恢复；target clip 先逆变换到 source 坐标，命令流内的嵌套 clip 只能继续收窄。
中间 opacity 通过固定 tile workspace 保持整层混合语义，不能改成逐命令乘 alpha。

capture 用一次 command stream 解码同时验证编码结构、按 snapshot bounds 构建 fixed occupancy，并按每 8 条
命令记录 byte offset、union bounds 与 clip-state 标记。半透明 replay 不再为每个 tile 重扫 command bounds，
并可跳过确认不命中的无状态 chunk；包含
`PushClip` / `PopClip` 的 chunk 始终保序执行。occupancy 与 chunk index 容量分别随 target envelope 和
DrawCmd command cap 进入静态内存 admission；bounds 超出 envelope 或索引构建失败时完整回放全部候选 tile
与 command，skip evidence 必须为零。该能力不改变快照槽仍按 profile mode 定额的事实：`COMMAND` 只支付
command slot，`HYBRID` 才按 pixel/command 较大者支付。

运行时 `SnapshotRecord::bytes`、layer budget 与 transition ledger 统计 command、text、blob 三段已用
payload 的总和，不能只统计 command stream。固定槽、occupancy 与 chunk index 的常驻容量由静态内存
admission 单独证明，不重复伪装成每次 capture 的已用 payload。

### PixelSurface Snapshot

像素快照是捕获时的 frozen pixel fact。live tree 后续变化不会自动改写该像素；只有 payload 丢失、
generation 失配、显式 stale 或 compose 前置条件失败时才拒绝。

它需要明确 pixel format、stride、bounds、alignment 和容量。offset、clip、opacity/alpha 语义以 source
实现和对应测试为准，不能从 backend blit 能力反推 Layer Runtime 已支持。

### Fallback

容量不足、artifact 不受支持或 stale 无法重建时，可以选择 command fallback、static cut、direct redraw
或 reject。选择由 profile、budget、backend capability 和调用方可执行路径共同决定，必须记录原因。
profile 未编译对应 snapshot kind 时，capture 必须在 reserve 前返回 `UnsupportedKind`；能力已编译但 slot
为零时返回 `NoSnapshotSlot`，两者不能合并成模糊的 store failure。

command capture 必须先完成 record，再写入 payload，最后发布 snapshot metadata。command/text/blob、
traversal workspace 溢出或 traversal phase 冲突属于 `RecordFailed`；完整 record 无法写入 payload 才属于
`StoreFailed`。任一失败都必须释放已保留的 slot 与已构造的 payload，且后续 capture 可以继续复用容量。

`Scene` 只向调用方发布完整 capture。slot reserve、payload write、metadata publish 和 epoch assignment 是
一次 capture 内部的事务阶段，不是产品可逐步调用的生命周期 API。stale command snapshot 必须重新 capture，
不能只刷新 epoch 后继续使用旧 payload。

capture result 以值语义返回 kind、实际 payload bytes 和 command count。generation、epoch、payload slot、
occupied/stale record 属于 Scene private partition；command/pixel payload 与 replay workspace 属于 internal
runtime module。产品 evidence 不持有 store/record 指针，也不以内存 slot 编号证明资源复用。

compose plan 是由目标 `Scene` 签发的短生命周期值，内部 32-bit seal 覆盖 Scene identity、handle、kind、
source/target rectangles、transform 与 budget evidence。调用方可以复制和读取 plan，但手工构造、字段篡改
或跨 Scene 使用必须在 budget/replay 前返回无效；plan 保持 trivially copyable，尺寸上限为 80B，不增加
Scene 常驻 RAM。

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

layer profile 是运行策略请求，不是编译期能力保证。product profile 的 snapshot storage mode 先限定可选
kind，预算再覆盖 payload slot、格式/stride、同时存活层数和 compose 工作量；
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
- indexed replay 与同场景 conservative replay 的 framebuffer hash 一致，clip push/pop 数量与顺序不变；
- command chunk skip、实际 command reads 与索引失效时的零 skip 回退；
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
