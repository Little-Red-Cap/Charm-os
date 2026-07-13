# SSU 局部契约

## 文档状态

- `status`: `supporting`
- `scope`: `kernel.ssu` 元数据、EDA 接入、Scheduler 观测与 submit 分类
- `authority`: 受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
  [`charm_core_contract.md`](../architecture/charm_core_contract.md) 约束

SSU（Schedulable Semantic Unit）是 kernel/scheduler 的局部实现机制，不是 Charm Core
原语，也不定义应用模型。本文只描述当前代码已经提供的行为。

## 当前接口

[`Modules/system/kernel/ssu.cppm`](../../Modules/system/kernel/ssu.cppm) 定义：

- `ExecutionDomain`: `isr_only`、`task_only`、`anywhere`
- `TriggerKind`: `event`、`io_ready`、`timer`、`frame`、`demand`
- `BudgetKind`: `single_step`、`budgeted`
- `BlockingKind`: `non_blocking`、`may_block`
- `Meta`: 上述四项元数据和一个 `name`
- `HasStaticMeta`、`HasEventStep`、`HasDrainStep`、`SsuUnit` concepts
- `as_event_unit()`：为 EDA 风格对象提供轻量适配

这些类型描述执行属性。它们不创建任务、不调度执行，也不自动实施预算、阻塞或
上下文约束。

## EDA 与 Scheduler 接入

[`Modules/system/kernel/eda.cppm`](../../Modules/system/kernel/eda.cppm) 的 `TaskRegistry`：

- 可读取 task 的 `ssu_meta()`；
- 对未声明元数据的 task 返回默认 `Meta`；
- 在 `CHARM_KERNEL_REQUIRE_SSU_META=1` 时，通过 `static_assert` 拒绝缺少
  `ssu_meta()` 的 EDA task。

严格模式只检查声明是否存在，不验证声明与运行时行为是否一致。

Scheduler 的 task/trace snapshot 会附带 registry 返回的 SSU 元数据，
`kernel.scheduler_export` 可导出触发、预算、阻塞和执行域分布。该观测面用于诊断，
不改变调度策略。

## Submit 分类

Scheduler 当前提供三类显式提交入口：

| 分类 | 代码入口 | 当前含义 |
|---|---|---|
| `event-submit` | `post()` / `post_token()` | 离散事件或普通任务推进 |
| `io-ready-submit` | `post_io_ready()` / `post_io_ready_token()` | IO ready 后交给任务上下文处理 |
| `demand-submit` | `post_demand()` / `post_demand_token()` | 下游需求或继续推进 |

这些入口共享现有 Scheduler 队列与执行机制。分类会进入来源统计，但当前不保证不同的
优先级、预算或隔离策略。

`system.run_loop` 另有 `SubmitProjection`，用于记录 step 的来源分类。它是审计标签，
不是 `kernel::ssu::Meta` 的运行时绑定，也不改变 step 行为。

## 已有代码证据

当前仓库中已有以下接入：

- `system.reactor_pump`: `task_only + io_ready + budgeted + non_blocking`
- `input.pump`: `task_only + timer + budgeted + non_blocking`
- `canopen.pump`: `task_only + timer + single_step + non_blocking`
- EDA、thread 和部分示例 task 声明了 `ssu_meta()`
- Reactor、input、CANopen、IPC 和 sync 路径使用了显式 submit 入口
- Scheduler export 可输出 SSU overview、hotspots 和 event source 统计

以上只能证明局部接入存在，不能证明所有执行模型已经统一。

## 约束

- 新增到严格模式 `TaskRegistry` 的 task 必须声明准确的 `ssu_meta()`。
- `non_blocking` 和 `budgeted` 是待评审的行为承诺，不是由类型自动执行的保证。
- ISR 只应完成必要的记录或通知；重处理应进入明确的 task 路径。
- 新增 submit 或内部推进旁路时，必须说明原因、影响范围、退出条件和回收路径。
- 不得仅凭 SSU 元数据把 scheduler、Reactor、RunLoop 或 audio data plane 宣称为同一模型。

具体评审动作见 [`ssu_review_checklist.md`](ssu_review_checklist.md)。

## 未决问题

以下内容保留为可继续验证的问题，不是当前契约：

- 是否需要把元数据承诺升级为可执行的预算或上下文检查；
- submit 分类是否应影响调度策略；
- RunLoop projection 是否应与 SSU 元数据建立正式映射；
- 设备时钟主导的数据面是否适合采用相同抽象；
- 当前热点阈值能否产生稳定、可操作的诊断结论。
