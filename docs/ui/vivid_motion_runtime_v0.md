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
- `PixelDouble` admission 下才捕获 source / destination PixelSurface；其它 admission 先走 static cut 路径。
- static cut 不做 PixelSurface capture，但仍执行 destination prepare，再提交 page truth。
- commit 后 source hidden，destination live / visible。
- cancel 或 begin failure 后恢复 begin 前 page truth。
- runner 获取的 snapshot 必须由 runner 释放；runner 回到 idle 时不得持有 `SnapshotHandle`。

新增验证入口：

```text
Examples/ui/vivid/page_transition_demo
```

当前覆盖：

- normal commit：双 snapshot capture、双层 compose、commit 后 `snapshot_count == 0`
- cancel during compose：abort 后恢复 begin 前可见性，`snapshot_count == 0`
- low budget static cut：不发生 PixelSurface capture，直接提交目标页
- destination prepare fail：释放已捕获的 source snapshot，恢复 page truth

## 2026-05 补记：PageTransition interrupt law

`PageTransitionRunner::begin()` 在已有 active transaction 时会先执行旧事务的 `cancel()` 收尾，再开始新事务。

规则：

- re-begin 必须释放旧 source / destination snapshot。
- re-begin 必须恢复旧事务 begin 前的 page truth，再捕获新事务 snapshot。
- 新事务 trace 通过 `interrupt_count` 记录这次隐式 abort。
- 后续 commit / cancel 后 `interrupt_count` 必须保留为可审计证据。

`Examples/ui/vivid/page_transition_demo` 已覆盖 `rebegin_interrupt`：旧事务 compose 后再次 begin，新事务重新持有两个 snapshot，最终 cancel 后 `snapshot_count == 0`。
