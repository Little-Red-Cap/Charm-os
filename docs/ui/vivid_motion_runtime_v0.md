# Vivid Motion Runtime v0

这份文档记录 Vivid motion runtime v0 的当前闭环。

它不是完整动画系统，也不是页面转场 API。它只定义一条可验证的运行时骨架：

```text
MotionTime
  -> LayerMotion
  -> MotionRecipe
  -> MotionTransitionRunner
  -> MotionTransitionTrace
  -> LayerComposeSpec
  -> MotionComposeDryRun
  -> MotionComposeProfileDecision
  -> MotionComposeExecute
  -> PageMotionTransition
```

目标是让 motion 在进入真实 Layer compose / backend 前，已经具备：

- 托管时间
- profile 降级
- transform 采样
- 生命周期托管
- 运行证据
- compose plan dry-run
- budget 到 effective profile 的裁决
- 真实 Scene compose 执行入口
- PageLayer freeze / thaw 过渡桥

## 非目标

当前 v0 不负责：

- 执行真实 `LayerComposePlan`
- 管理 PageLayer freeze / thaw
- 接入 Player 页面转场
- 提供复杂 Motion DSL
- 提供 shared element / mask / z-order compositor
- 替代 `vivid_layer_runtime_v0.md`

## 模块职责

### `motion_time.cppm`

负责时间采样，不知道 layer、snapshot、recipe。

核心类型：

- `MotionTier`
- `MotionTimeSpec`
- `MotionTick`

核心函数：

- `sample_motion_time()`
- `motion_frame_ms()`
- `motion_progress()`

当前 tier 语义：

```text
Rich60Fps      连续采样
Cheap30Fps     33ms 量化采样，到达 duration 时 clamp 到终点
StaticCut      立即采样终点
EinkDissolve   结束前保持起点，结束后采样终点
None           无采样需求，直接终点
```

### `motion_plan.cppm`

负责把 `MotionTick` 投影为 `LayerTransform`。

核心类型：

- `LayerMotionSpec`
- `LayerMotionFrame`

核心函数：

- `motion_tier_from_layer_profile()`
- `sample_layer_motion()`

关键规则：

- `LayerProfile` 决定使用哪个 `MotionTier`
- `LayerProfile` 决定 opacity 是否量化 / cut
- 这一层不生成 `LayerComposeSpec`

### `motion_recipe.cppm`

负责把最小 recipe 翻译为 `LayerMotionSpec`。

当前 recipe：

- `cut`
- `fade`
- `slide`
- `fade_slide`

核心函数：

- `motion_cut()`
- `motion_fade()`
- `motion_slide()`
- `motion_fade_slide()`
- `make_layer_motion_spec()`
- `sample_motion_recipe()`

关键规则：

- recipe 是愿望，不是执行
- recipe 不知道 snapshot / backend
- recipe 输出仍由 profile 和 budget 裁决

### `motion_transition.cppm`

负责托管 transition 生命周期。

核心类型：

- `MotionTransitionState`
- `MotionTransitionSpec`
- `MotionTransitionFrame`
- `MotionTransitionTrace`
- `MotionTransitionRunner`

当前生命周期：

```text
Idle -> begin -> Running -> sample -> Running / Finished
Running -> cancel -> Canceled
Finished / Canceled -> reset -> Idle
```

关键规则：

- `cancel()` 只对 `Running` 生效
- `Finished` 后再次 `sample()` 返回终点帧
- `Canceled` 后再次 `sample()` 返回非 compose 帧
- trace 记录 sample / compose / finish / cancel 等最小证据

### `motion_compose.cppm`

负责把 transition frame 接到 Layer Runtime 的 compose 输入。

核心类型：

- `MotionComposeRequest`
- `MotionComposeBridgeResult`
- `MotionComposeDryRunResult`
- `MotionComposeProfileDecision`

核心函数：

- `make_motion_compose_spec()`
- `dry_run_motion_compose()`
- `decide_motion_compose_profile()`

关键规则：

- 缺少 `SnapshotHandle` 时不生成 compose spec
- frame 不需要 compose 时不生成 compose spec
- dry-run 只调用 `SnapshotStore::make_compose_plan()` 和 `check_budget()`
- budget 证据通过 `decide_layer_profile()` 转为 effective profile / fallback reason
- 这一层仍不执行真实 compose

### `motion_execute.cppm`

负责把 motion compose request 接到 `Scene` 的真实执行入口。

核心类型：

- `MotionComposeExecuteResult`

核心函数：

- `execute_motion_compose()`

关键规则：

- 只有这一层知道 `Scene`
- 先复用 `make_motion_compose_spec()`
- 再通过 `Scene::make_snapshot_compose_plan()` 生成执行计划
- `CommandBuffer` 走 `Scene::replay_command_snapshot()`
- `PixelSurface` 走 `Scene::compose_pixel_snapshot()`
- 返回 replay 结果和 compose pixel 证据

### `motion_page_transition.cppm`

负责把 `PageLayer` 的 snapshot 生命周期接到 motion transition。

核心类型：

- `PageMotionTransitionSpec`
- `PageMotionTransitionFrame`
- `PageMotionTransition`

核心流程：

```text
PageLayer::freeze()
  -> PageLayer::mark_transitioning()
  -> MotionTransitionRunner::begin()
  -> sample()
  -> execute_motion_compose()
  -> finish()
  -> PageLayer::thaw()
```

关键规则：

- 这一层只做单页 frozen snapshot transition
- 失败时不启动 runner
- `hide_live_root` 控制 freeze 后是否隐藏 live root
- `finish()` 负责 thaw 并释放 snapshot
- 仍不负责多页面导航或 Player 转场状态机

## 验证入口

最小验证示例：

```text
Examples/ui/vivid/motion_time_demo
```

当前示例覆盖：

- `Rich60Fps / Cheap30Fps / StaticCut / EinkDissolve / None`
- `LayerProfile` 到 `MotionTier`
- `LayerTransform` 采样
- `fade / slide / fade_slide / cut`
- transition begin / sample / finish / cancel / reset
- trace sample / compose / finish / cancel 计数
- transition frame 到 `LayerComposeSpec`
- `SnapshotStore` dry-run compose plan
- composite pixel budget overrun
- stale snapshot 拒绝
- budget 到 effective profile / fallback reason 的裁决
- `Scene` pixel snapshot compose 最小执行路径
- `PageLayer` freeze / transitioning / execute / thaw 最小路径

构建：

```bash
cmake -S Examples/ui/vivid/motion_time_demo -B cmake-build-vivid-motion-time-demo-codex -G Ninja
cmake --build cmake-build-vivid-motion-time-demo-codex -j 22
cmake-build-vivid-motion-time-demo-codex/vivid-motion-time-demo
```

## 当前闭环

现在 v0 已经证明：

```text
recipe + profile + time
  -> frame
  -> trace
  -> compose spec
  -> dry-run plan
  -> budget result
  -> effective profile decision
  -> optional scene execute
  -> page layer transition bridge
```

这意味着 motion runtime 已经可以在不进入 PageLayer 页面转场的情况下回答：

- 当前帧是否应该 compose？
- 当前 transform 是什么？
- 这次 transition 采样了多少次？
- compose 会覆盖多少像素？
- 当前预算是否允许？
- 如果预算不允许，effective profile 应降级到哪里？
- 如果进入最小执行，Scene replay 是否成功？
- 如果接入 PageLayer，snapshot 是否能被释放并 thaw 回 live？

## 下一步

下一步不建议继续发明新 recipe。

更值得做的是把这条链接入多页面 transition runner 的最小闭环：

```text
source PageLayer freeze
  -> destination PageLayer freeze / prepare
  -> dual compose
  -> release source
  -> thaw destination
```

第一阶段仍应保持 Vivid-only，优先验证：

- 不碰 Player
- 不引入复杂 compositor
- 不绕过 `LayerBudget`
- 不让 recipe 直接接触 backend
- failure / stale / over-budget 都有确定行为

## 2026-05 补记：PageTransitionRunner v0

`page_transition.cppm` 已经把双页迁移从“动画参数”提升为 Vivid runtime 事务：

```text
begin()
  -> admission
  -> freeze source
  -> prepare destination
  -> freeze destination
  -> sample compose
  -> commit / cancel
```

v0 规则：

- `PageTransitionRunner` 只知道 `source PageLayer`、`destination PageLayer`、prepare callback、profile、budget 和 motion recipe，不知道 Player 业务页面。
- `PixelDouble` admission 下捕获 source / destination PixelSurface；`PixelSingle` admission 下只捕获 source PixelSurface，destination 保持 live。
- `CommandSnapshot` / `StaticCut` admission 先走 static cut 路径，CommandSnapshot 的双页 replay 留给后续阶段。
- static cut 不做 PixelSurface capture，但仍执行 destination prepare，再提交 page truth。
- commit 后 source hidden，destination live / visible。
- cancel 或 begin failure 后恢复 begin 前 page truth。
- runner 获取的 snapshot 必须由 runner 释放；runner 回到 idle 时不得持有 `SnapshotHandle`。

### PageTransition v0 运行形态矩阵

这张表是当前 `PageTransitionRunner` 的阶段性收口。它描述的是已经由 demo 验证的行为，不是未来目标。

| requested / admission | runtime shape | source ownership | destination shape | begin result | page truth result | evidence |
| --- | --- | --- | --- | --- | --- | --- |
| `Rich/Cheap -> PixelDouble` | 双 PixelSurface snapshot compose | runner 持有 source snapshot | runner 持有 destination snapshot | `Started` | commit 后 source hidden，destination live | `normal_commit` / `fade_slide_pixel_double` / `cancel_during_compose` |
| `Rich/Cheap -> PixelSingle` | source snapshot over live destination | runner 持有 source snapshot | destination prepare 后保持 live | `Started` | commit 后 source hidden，destination live；cancel 后恢复 begin 前 | `pixel_single_fade_slide_live_destination` / `pixel_single_cancel` / `pixel_single_rebegin_interrupt` |
| `Static -> StaticCut` | 主动 static cut | 无 snapshot | destination prepare 后直接提交 | `StaticCut` | source hidden，destination live | `static_profile_static_cut` |
| `None -> Reject` | 拒绝事务 | 无 snapshot | 不调用 prepare | `Rejected` | begin 前 page truth 不变 | `none_profile_reject` |
| budget / caps -> `CommandSnapshot` | 当前合法降级为 static cut | 无 snapshot / 无 command payload | destination prepare 后直接提交 | `StaticCut` | source hidden，destination live | `command_snapshot_static_cut` |
| source capture fail | begin failure rollback | 捕获失败，无持有 | 不进入 prepare / capture | `SourceCaptureFailed` | 恢复 begin 前 page truth | `source_capture_fail` |
| destination capture fail | begin failure rollback | 释放已捕获 source snapshot | destination capture 失败 | `DestinationCaptureFailed` | 恢复 begin 前 page truth | `destination_capture_fail` |
| active begin interrupt | 旧事务 abort，新事务 begin | 先释放旧事务持有 snapshot | 先恢复旧 page truth，再进入新事务 | 新事务 result | 由新事务 commit/cancel 决定 | `rebegin_interrupt` / `pixel_single_rebegin_interrupt` |

收口原则：

- `Static` 是主动运行形态；`None` 是主动拒绝形态。
- `CommandSnapshot` 目前只是 admission 结果，不代表双页 command replay 已实现。
- `PixelSingle` 是降级后的真实合成路径，不是 static cut。
- `PageTransitionRunner` 回到 idle 时不得持有任何 `SnapshotHandle`。
- 所有路径都必须能被 `PageTransitionTrace` / `PageTransitionLedger` 审计。

新增验证入口：

```text
Examples/ui/vivid/page_transition_demo
```

当前覆盖：

- normal commit：双 snapshot capture、双层 compose、commit 后 `snapshot_count == 0`
- fade slide pixel double：`fade_slide` recipe 在双 snapshot compose 中同时输出 transform / opacity
- fade slide cheap quantized：同一 recipe 在 Cheap profile 下量化 motion time 与 opacity
- cancel during compose：abort 后恢复 begin 前可见性，`snapshot_count == 0`
- static profile：`LayerProfile::Static` 主动选择 `StaticCut` admission，不依赖预算失败
- none profile：`LayerProfile::None` 主动拒绝事务，不调用 prepare，不改变 page truth
- pixel single：只捕获 source snapshot，destination 保持 live，commit / cancel 后 `snapshot_count == 0`
- command snapshot static cut：`CommandSnapshot` admission 在双页 replay 实现前不发生 capture，显式 static cut 并直接提交目标页
- destination prepare fail：释放已捕获的 source snapshot，恢复 page truth

## 2026-05 补记：PageTransition interrupt law

`PageTransitionRunner::begin()` 在已有 active transaction 时会先执行旧事务的 `cancel()` 收尾，再开始新事务。

规则：

- re-begin 必须释放旧事务持有的 source / destination snapshot；`PixelSingle` 路径只有 source snapshot 也必须满足同一条律。
- re-begin 必须恢复旧事务 begin 前的 page truth，再捕获新事务 snapshot。
- 新事务 trace 通过 `interrupt_count` 记录这次隐式 abort。
- 后续 commit / cancel 后 `interrupt_count` 必须保留为可审计证据。

`Examples/ui/vivid/page_transition_demo` 已覆盖 `rebegin_interrupt`：旧事务 compose 后再次 begin，新事务重新持有两个 snapshot，最终 cancel 后 `snapshot_count == 0`。

同一入口也覆盖 `pixel_single_rebegin_interrupt`：旧 PixelSingle 事务只持有 source snapshot，再次 begin 时先释放旧 snapshot，新事务重新持有一个 source snapshot，最终 cancel 后 `snapshot_count == 0`。

## 2026-05 补记：PageTransition capture failure law

`PageTransitionRunner` 的 capture 失败路径现在也有明确证据：

- source capture fail：不进入 destination prepare / capture，恢复 begin 前 page truth，`snapshot_count == 0`。
- destination capture fail：释放已获取的 source snapshot，恢复 begin 前 page truth，`snapshot_count == 0`。
- `PageTransitionTrace::begin_status` 会记录 `SourceCaptureFailed` / `DestinationCaptureFailed`，并保留对应 `LayerCaptureStatus`。

`Examples/ui/vivid/page_transition_demo` 已覆盖 `source_capture_fail` 与 `destination_capture_fail`。

## 2026-05 补记：PageTransition ledger v0

`PageTransitionLedger` 已作为 `PageTransitionRunner` 的事务账本接入。

账本 v0 记录：

- begin status / final state / admission / requested profile / effective profile
- begin / sample / commit / abort / interrupt / static cut 计数
- source / destination capture status 与 capture count
- source / destination snapshot bytes 与 peak layer bytes
- destination / source / total composite pixels
- committed / aborted / static_cut / interrupted / snapshots_released 布尔证据

`Examples/ui/vivid/page_transition_demo` 已对 normal commit、cancel、Static/None profile、CommandSnapshot static-cut、prepare fail、capture fail 与 rebegin interrupt 的账本字段做断言。

## 2026-05 补记：PageTransition fade_slide recipe evidence

`PageTransitionRunner` 已有第一条 Motion recipe 接入证据：`fade_slide_pixel_double`，并补充了 Cheap profile 下的量化证据 `fade_slide_cheap_quantized`。

这条用例验证的是：

- `PixelDouble` admission 下 source / destination 都以 PixelSurface snapshot 参与 compose。
- `PixelSingle` admission 下 source 以 PixelSurface snapshot 参与 compose，destination 保持 live 且不执行 destination snapshot compose。
- `MotionRecipeKind::FadeSlide` 会进入 `MotionTransitionTrace`。
- sample frame 同时携带位移与 opacity。
- source compose plan 使用 sampled transform / opacity。
- destination snapshot 仍以 identity transform 参与 compose。
- Rich profile 下 sample time 直接按输入时间推进。
- Cheap profile 下 motion time 量化到 30fps step，opacity 量化到 4-step 档位；该规则同时覆盖 PixelDouble 与 PixelSingle。
- Static profile 下同一 `fade_slide` 请求仍直接 static cut，不启动 motion runner。
- None profile 下同一 `fade_slide` 请求仍直接 reject，不调用 prepare，也不启动 motion runner。
- commit 后释放所有 snapshot，回到 idle。

### PageTransition fade_slide profile 矩阵

这张表收口的是同一个 `fade_slide` 请求在不同 admission / profile 下的已验证行为。

| requested / admission | recipe execution | time / opacity policy | composed artifacts | evidence |
| --- | --- | --- | --- | --- |
| `Rich -> PixelDouble` | motion runner starts | 直接采样输入时间；opacity 不量化 | source snapshot + destination snapshot | `fade_slide_pixel_double` |
| `Cheap -> PixelDouble` | motion runner starts | 30fps 时间量化；opacity 4-step 量化 | source snapshot + destination snapshot | `fade_slide_cheap_quantized` |
| `Rich -> PixelSingle` | motion runner starts | 直接采样输入时间；opacity 不量化 | source snapshot over live destination | `pixel_single_fade_slide_live_destination` |
| `Cheap -> PixelSingle` | motion runner starts | 30fps 时间量化；opacity 4-step 量化 | source snapshot over live destination | `pixel_single_fade_slide_cheap_quantized` |
| `Static -> StaticCut` | motion runner 不启动 | recipe 被 profile 裁掉 | 无 snapshot compose，直接提交目标页 | `static_profile_static_cut` |
| `None -> Reject` | motion runner 不启动 | recipe 被 profile 拒绝 | 无 prepare / capture / compose | `none_profile_reject` |
| budget / caps -> `CommandSnapshot` | motion runner 不启动 | command replay 未实现，合法降级 | 无 snapshot / command compose，直接提交目标页 | `command_snapshot_static_cut` |

收口原则：

- Recipe 是请求，不是保证；profile / admission 先裁决 runtime shape。
- `PixelDouble` 与 `PixelSingle` 都可以执行同一个 recipe，但 artifacts 不同。
- Cheap 的时间与 opacity 量化是 profile law，不是单独页面逻辑。
- Static / None / CommandSnapshot static-cut 不能因为 recipe 存在而启动 motion runner。

这不是完整 Motion system，只是 PageTransition 事务骨架上的第一块 recipe 肌肉：同一 recipe 已开始受 profile 裁决，并且不能绕过 Static / None 的运行时宪法。

## 2026-05 补记：PageTransition None profile law

`LayerProfile::None` 是显式禁用转场事务的 profile。它与 `Static` 不同：`Static` 会提交目标页，`None` 会拒绝本次 begin。

当前 `PageTransitionRunner` 在 requested profile 为 `None` 时：

- admission 为 `Reject`。
- begin status 为 `Rejected`。
- 不调用 destination prepare。
- 不捕获 source / destination snapshot。
- 不 commit、不 abort、不 static cut。
- 不改变 begin 前 page truth。
- trace / ledger 保留 `requested_profile == None`、`effective_profile == None` 与 `admission == Reject`。
- 回到 idle 后 `snapshot_count == 0`，`peak_layer_bytes == 0`，`total_composite_pixels == 0`。

`Examples/ui/vivid/page_transition_demo` 已覆盖 `none_profile_reject`，该用例使用 `fade_slide` 请求来验证 recipe 不会绕过 `None` 的拒绝语义。

## 2026-05 补记：PageTransition Static profile law

`LayerProfile::Static` 是主动运行形态，不是错误路径，也不是预算失败后的事故现场。

当前 `PageTransitionRunner` 在 requested profile 为 `Static` 时：

- admission 直接为 `StaticCut`。
- 不捕获 source / destination snapshot。
- 仍执行 destination prepare。
- 直接提交 page truth：source hidden，destination live / visible。
- trace / ledger 保留 `requested_profile == Static`、`effective_profile == Static` 与 `admission == StaticCut`。
- 回到 idle 后 `snapshot_count == 0`，`peak_layer_bytes == 0`，`total_composite_pixels == 0`。

`Examples/ui/vivid/page_transition_demo` 已覆盖 `static_profile_static_cut`，该用例使用 `fade_slide` 请求来验证 recipe 不会绕过 `Static` 的 static cut 语义。

## 2026-05 补记：PageTransition CommandSnapshot static-cut law

双页 `CommandSnapshot` replay 仍未进入 v0 执行路径。当前 `PageTransitionRunner` 在 admission 裁决为 `CommandSnapshot` 时执行合法降级：

- 不捕获 source / destination PixelSurface。
- 不创建 CommandSnapshot payload。
- 仍执行 destination prepare。
- 以 `StaticCut` begin status 提交目标页。
- trace / ledger 保留 `admission == CommandSnapshot` 与 `effective_profile == Static`。
- 回到 idle 后 `snapshot_count == 0`，`peak_layer_bytes == 0`，`total_composite_pixels == 0`。

`Examples/ui/vivid/page_transition_demo` 已覆盖 `command_snapshot_static_cut`。

## 2026-05 补记：PageTransition PixelSingle path

`PageTransitionRunner` 已接入 `PixelSingle` admission 的真实执行路径。

语义：

- begin 前的 admission 若只允许一个 PixelSurface slot / budget，则选择 `PixelSingle`。
- source page 会被捕获为 PixelSurface snapshot，并按 recipe 参与 compose。
- destination page 不捕获 snapshot，而是在 prepare 后保持 live / visible，作为 live destination 背景。
- sample 阶段只执行 source snapshot compose，destination compose result 保持 invalid。
- commit / cancel 后 runner 释放所持 source snapshot，回到 idle 时 `snapshot_count == 0`。

`Examples/ui/vivid/page_transition_demo` 已覆盖 `pixel_single_fade_slide_live_destination`、`pixel_single_fade_slide_cheap_quantized`、`pixel_single_cancel` 与 `pixel_single_rebegin_interrupt`，并断言 source-only bytes、source-only composite pixels、destination capture count 为 0，以及 Cheap profile 量化、cancel / interrupt 后恢复 begin 前 page truth。
