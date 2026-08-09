# Vivid Motion Runtime v0

## 文档状态

- `status`: `supporting`
- `scope`: managed UI time、motion recipe、compose bridge 与 page transition 事务
- `authority`: `Modules/ui/vivid/core/motion_*.cppm`、
  [`page_transition.cppm`](../../Modules/ui/vivid/core/page_transition.cppm)

本文记录 Motion Runtime 的稳定边界，不复制类型/函数清单、demo case、当前覆盖矩阵或后续排期。

## 责任链

```text
runtime-owned time
    -> recipe + requested profile
    -> sampled layer transform
    -> admission / effective profile
    -> compose request
    -> layer execute
    -> transition evidence
```

模块职责：

| 层 | 责任 |
|---|---|
| motion time | 从 runtime time source 产生 bounded/quantized progress |
| plan/recipe | 把产品 motion intent 投影为 layer transform，不接触 snapshot/backend |
| transition runner | 管理 begin、sample、finish、cancel/reset 与 trace |
| compose bridge | 从有效 frame/snapshot 形成 compose request，并做 dry-run/budget 裁决 |
| execute | 唯一可接触 Scene replay/compose 的 motion 层 |
| page transition | 组合 source/destination PageLayer、prepare、capture、commit 与 rollback |

Layer artifact、stale、payload 和 budget 规则由
[`vivid_layer_runtime_v0.md`](vivid_layer_runtime_v0.md) 定义。Motion 不重新定义 snapshot ownership。

## Managed UI Time

页面和 widget 不读取任意 wall clock，也不各自维护不一致的 frame time。runtime 提供当前 tick，motion
按 profile 采样：

| Tier | 时间语义 |
|---|---|
| Rich | 连续进度；实际帧率仍由 runtime/backend 决定 |
| Cheap | bounded step 量化，并在 duration 终点收敛 |
| Static | 直接取终点，不启动连续 motion |
| EInk | 只表达受控刷新阶段，不假设动画帧 |
| None | 不请求 motion；调用方按拒绝/无 motion 语义处理 |

duration、overflow、零时长和终点 clamp 必须确定。采样次数或 wall time 不能绕过 budget/admission。

## Recipe 与 Profile

recipe 只表达 `cut/fade/slide/fade_slide` 等产品意图，不选择 snapshot kind、cache slot 或 backend fast
path。requested profile 也不是执行保证。

有效执行形态由 profile、budget、backend capability、snapshot slot 和调用方支持的 compose path 共同
决定。Cheap 可以量化时间/opacity；Static 可以裁为 cut；None 可以拒绝；unsupported 形态必须显式
fallback 或 reject，不能静默启动另一套页面动画。

当 target 配置 `layer_cache_slots=0` 时，所有需要 snapshot 的 admission 都必须降级为 `StaticCut` 或
`Reject`，不能返回随后必然以 `NoSnapshotSlot` 失败的 command/pixel snapshot 形态。
product profile 未启用 pixel storage 时，PageTransition 只允许全程 255 opacity 的 recipe 采用单 source
command snapshot；需要整体 opacity、prepare 后 epoch 已变化或实际 command 工作量越过 budget 时，必须在
首帧前正规化为 `StaticCut` 或 `Reject`，并在 trace/ledger 保留具体 fallback reason。

新增 recipe 前应先证明现有 recipe 无法表达真实 consumer，并补齐各 profile 的成功、降级和拒绝语义。

## Compose Boundary

compose bridge 只在以下条件满足时生成请求：

- frame 明确要求 compose；
- snapshot handle、generation、payload 与 artifact kind 有效；
- transform/clip/opacity 在对应 compose path 支持范围内；
- admission 与 budget 允许本次工作量。

dry-run 只能证明计划和预算可形成，不证明 Scene 已执行。真实 execute 必须返回 replay/compose 状态和
工作量证据；失败不能提交 page truth。当前 pixel compose 与 command replay 都支持整数平移和整体 opacity；
command 的中间 opacity 使用固定 tile workspace，并依赖 capture-time occupancy 跳过空白 tile。索引不可用时
必须保守执行全部候选 tile，不能产生漏绘。`PageTransitionRunner` 的 command 形态只复用该能力，不扩大
transform 集合。

## Page Transition 事务

双页 transition 的顺序为：

```text
begin
  -> admission
  -> capture/prepare source and destination as required
  -> sample + compose
  -> commit or cancel
  -> release all owned snapshots
  -> idle
```

`PageTransitionRunner` 只消费 PageLayer、prepare callback、profile、budget 和 recipe，不包含 Player 页面
身份或 navigation policy。

### 运行形态

| Admission/Profile | Runtime shape | 结果边界 |
|---|---|---|
| PixelDouble | source 与 destination 均为 frozen pixel snapshot | commit/cancel 后释放两者 |
| PixelSingle | source snapshot 合成到 live destination | destination 不产生 snapshot；结束后释放 source |
| CommandSnapshot | source command snapshot 合成到 live destination | 只占一个 slot；结束或降级后释放 source |
| Static/StaticCut | 不启动 motion/capture，prepare 后直接提交目标页 | 是合法运行形态，不是错误 |
| None/Reject | 不 prepare、不 capture、不改变 page truth | 返回明确拒绝 |

recipe 不能绕过这些形态。Static 与 None 不同：Static 提交目标页，None 保持 begin 前状态并拒绝。

### Commit、Cancel 与 Interrupt

- commit 后 source hidden，destination live/visible，runner 不再持有 snapshot；
- cancel 恢复 begin 前 page truth，释放已取得的 source/destination snapshot；
- active runner 再次 begin 时，先按 cancel/interrupt 收尾旧事务，再开始新事务；
- interrupt 证据必须保留，不能把旧事务覆盖成从未发生；
- finish/cancel/reject 后再次调用的行为必须由明确状态机约束，不依赖调用顺序侥幸成立。

### Failure Rollback

- source capture 失败时不进入 destination prepare/capture；
- destination prepare/capture 失败时释放 source，并恢复 begin 前 page truth；
- compose/execute 失败时不提交成功状态，调用方必须进入 rollback/cancel；
- 所有 early return 最终都满足 snapshot count/ownership 回到事务前边界。

## Evidence

Motion/PageTransition evidence 至少覆盖：

- 每个 time tier 的起点、量化、终点和零时长；
- recipe 在 Rich/Cheap/Static/None 下的执行、降级和拒绝；
- PixelDouble 与 PixelSingle 的 artifact/ownership 差异；
- CommandSnapshot 的单槽 source replay，以及 opacity、budget、epoch 不满足时的首帧前降级；
- CommandSnapshot 半透明 replay 的候选/命中/跳过 tile 与 bounds/execute 命令流读取，确认 indexed replay
  不再重复扫描 bounds command，并同时覆盖
  稀疏与密集页面的完整 transition workload envelope；
- source/destination capture failure rollback；
- commit、cancel、rebegin interrupt 后 page truth 与 snapshot release；
- stale、over-budget 和 unsupported compose 的负例；
- trace/ledger 能关联 requested/effective profile、admission、capture、compose 与 final state。

推荐 fixture 从 [`vivid_evidence_lab_manifest_v0.md`](vivid_evidence_lab_manifest_v0.md) 进入，因果 verdict
遵守 [`vivid_causal_verdict_law_v0.md`](vivid_causal_verdict_law_v0.md)。COMMAND envelope 只描述 Vivid
regression workload 的 sample 数、累计与峰值读取；在出现真实产品 COMMAND consumer 前，它不定义
PRODUCT admission 阈值。demo 成功只证明其 case，不能替代产品性能、视觉结果或实板 display/cache 证据。

primary evidence routes 是 `Examples/ui/vivid/page_transition_demo` 与
`Examples/ui/vivid/motion_time_demo`；两者的 final `causal_chain` 必须由上述 segment 推导。

## 非目标

- 不提供任意动画 DSL、shared-element、mask 或通用 compositor。
- 不管理产品 navigation、Player 页面状态或 backend frame loop。
- 不让页面私有 timer/animation 绕过 managed time 和 admission。
- 不用日志字段、case 数或当前实现清单定义 API。
- 不在本文维护阶段 addendum、完成度或 roadmap。
