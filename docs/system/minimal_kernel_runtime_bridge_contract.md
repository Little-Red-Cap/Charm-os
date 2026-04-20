# 最小内核运行时 Bridge 契约（草案）

这份文档用于把“下半层架构入口”和“上半层最小运行时胶水”的接缝写清楚。

目标不是提前重写内核，也不是把 ARMv7-A / QEMU / 板级细节抽成假大空接口，而是先把下面这几件事收成一条稳定的最小闭环：

- `tick -> scheduler`
- `ISR deferral -> scheduler post`
- `idle bootstrap`
- `worker bootstrap`
- `yield / sleep` 的最小任务侧入口

## 一句话版本

- 下半层负责“什么时候进入运行时”
- 上半层负责“进入运行时以后，最小调度语义怎么闭环”

只要这句话不变，我们就可以同时支持：

- host/stub 上的最小闭环证据
- ARMv7-A QEMU 的真实 ingress 接线
- 后续真实板级把同一语义映射到异常/中断/时钟硬件

## 当前模块分层

当前建议把运行时胶水分成七层：

### 1) `kernel.runtime_glue`

位置：`Modules/system/kernel/runtime_glue.cppm`

职责是“无状态的最薄原语”，直接把现有 scheduler/thread/timer 能力拼成可复用的最小动作：

- `runtime_advance_tick(...)`
- `runtime_defer_from_isr(...)`
- `runtime_bootstrap_idle(...)`
- `runtime_bootstrap_worker(...)`
- `runtime_yield_current(...)`
- `runtime_sleep_current_until(...)`
- `runtime_run_once_or_idle(...)`

它适合做：

- 最靠近内核语义的 glue
- 单元级 / host 级证据
- 后续更高一层 bridge 的底座

它暂时不负责：

- 保存任何平台状态
- 绑定固定 idle task
- 给任务侧暴露更友好的入口对象

### 2) `kernel.runtime_bridge`

位置：`Modules/system/kernel/runtime_bridge.cppm`

职责是“带状态的运行时入口桥”。

它把这些运行时上下文绑在一起：

- `Scheduler`
- `idle task`
- `idle event`
- 可选 trace buffer

当前桥对象是：

- `RuntimeBridge<Scheduler, TraceBuffer>`

它提供的入口是：

- `advance_tick(now)`
- `defer_from_isr(task, event)`
- `bootstrap_idle()`
- `bootstrap_worker(task, event)`
- `yield_current(event)`
- `sleep_current_until(due, event)`
- `run_once_or_idle(now)`

当前专门验证 RuntimeBridge stateful binding seam 的 host 证据是：

- `Examples/kernel/runtime_bridge_binding_host`

它直接覆盖 `bind_idle(...)`、`bind_trace(...)`、getter 对齐，以及 fallback idle retarget，不把这类状态绑定语义混进 tick / thread-side / trap transport 证据里。

这层更适合被下半层持有，因为它已经把“当前这条运行时实例是谁、idle 是谁、trace 放哪里”这几个问题收住了。

当前专门验证这条 tick seam 的 host 证据是：

- `Examples/kernel/runtime_tick_host`

它直接覆盖 `advance_tick(now) -> scheduler.tick(now)` 的未到期/到期路径，以及 timer source 统计与 runtime tick trace 的对齐。

当前专门验证这条 ISR deferral seam 的 host 证据是：

- `Examples/kernel/runtime_isr_defer_host`

它直接覆盖 `defer_from_isr(task, event) -> scheduler.post_demand(...)` 的正反路径，以及 worker wait/idle/deferred resume 的最小闭环。

### 3) `RuntimeLoopPort<Tick>`

位置：同 `kernel.runtime_bridge`

职责是“给下半层 / leaf 持有的最小 runtime loop 入口”，把 `RuntimeBridge` 这组 stateful 行为收成不暴露 `Scheduler` 模板细节的 type-erased port。

当前只保留最靠近 lower-half ingress 的几项：

- `advance_tick(now)`
- `defer_from_isr(task, event)`
- `bootstrap_idle()`
- `bootstrap_idle(event)`
- `bootstrap_worker(task, event)`
- `run_once_or_idle(now)`

它的目的不是替代 `RuntimeBridge` 本体，而是给 ARMv7-A / QEMU / future board leaf 一个更薄、更稳定的 runtime 落点：

- leaf 可以持有 port，而不是直接知道上半层 bridge 的具体模板参数
- 这条边界和任务侧 `RuntimeThreadPort` 分开，避免把 lower-half loop 入口和 thread-side yield/sleep 混成一团

当前专门验证这条 lower-half runtime loop seam 的 host 证据是：

- `Examples/kernel/runtime_loop_port_host`

它直接覆盖 `RuntimeBridge -> RuntimeLoopPort -> tick / ISR defer / idle bootstrap / worker bootstrap / run_once_or_idle`，把下半层真正会持有的那一小组动作收成独立证据。

### 4) `RuntimeThreadPort<Tick>`

位置：同 `kernel.runtime_bridge`

职责是“给任务上下文暴露最小运行时能力”，目前只保留两项：

- `yield_current(...)`
- `sleep_current_until(...)`

它的目的不是替代 scheduler，也不是把任务代码直接绑死到底层调度器模板参数，而是给 thread / worker step 一个很薄的 runtime 侧口子。

当前通过：

- `make_runtime_thread_port(runtime_bridge)`

从 `RuntimeBridge` 派生出来。

当前专门验证这条 thread-side seam 的 host 证据是：

- `Examples/kernel/runtime_thread_port_host`

它直接覆盖 `RuntimeBridge -> RuntimeThreadPort -> scheduler/timer`，不经过 trap/syscall transport。

### 5) `kernel.runtime_trap`

位置：`Modules/system/kernel/runtime_trap.cppm`

职责是把 trap/service 语义从直接 runtime 调用里单独提出来。

它当前提供：

- `TrapFrameView`
- `TrapRequest`
- `TrapResult`
- `RuntimeTrapBridge`
- `RuntimeTrapPort`

更完整的边界说明见：

- `docs/system/minimal_kernel_trap_syscall_contract.md`

### 6) `kernel.runtime_service`

位置：`Modules/system/kernel/runtime_service.cppm`

职责是“给任务侧一个稳定、友好的 trap service facade”，把：

- `RuntimeTrapPort`
- `RuntimeTrapIngressCaller`

这类 transport 的调用形状收口成同一组 task-side 入口：

- `yield_current(...)`
- `sleep_current_until(...)`
- `debug_write(...)`
- `capability_call(...)`

它更适合被 worker/task context 直接持有，因为它不要求任务代码知道底下绑定的是 runtime bridge 直连 port，还是 host/stub 证据路径上的 ingress caller。

更完整的 task-side 边界说明见：

- `docs/system/minimal_kernel_runtime_service_contract.md`
- `docs/system/minimal_kernel_task_runtime_api_contract.md`
- `docs/system/minimal_kernel_task_syscall_api_contract.md`

### 7) `kernel.runtime_trap_ingress`

位置：`Modules/system/kernel/runtime_trap_ingress.cppm`

职责是把真实 arch trap frame 和 `TrapFrameView` / `TrapResult` 之间的翻译动作单独收口。

更完整的边界说明见：

- `docs/system/minimal_kernel_trap_ingress_contract.md`

## 下半层和上半层各自负责什么

### 下半层负责

- 异常向量、IRQ/FIQ、trap 入口
- 硬件 tick / generic timer / board timer
- context switch ABI
- `CurrentContext` 与 arch-specific current task seam
- ISR 中什么时候只记账、什么时候触发 deferred post
- 何时进入一次调度轮转

换句话说，下半层决定“机器什么时候把控制权交给 runtime bridge”。

### 上半层负责

- scheduler / thread / timer 的最小闭环语义
- idle / worker 的引导方式
- `yield / sleep` 的任务侧最小入口
- trace 上的语义证据

换句话说，上半层决定“进入运行时后，语义怎么往前走”。

## 当前建议调用形状

### 启动阶段

1. 创建并启动 scheduler
2. 绑定 `RuntimeBridge`
3. 指定 idle task
4. 用 `bootstrap_worker(...)` 或 `bootstrap_idle(...)` 投递最初事件

### 中断 / tick 阶段

- 定时中断推进软定时器时，调用 `advance_tick(now)`
- ISR 只做 deferred post 时，调用 `defer_from_isr(task, event)`
- 如果下半层不想直接持有具体 `RuntimeBridge<Scheduler, ...>` 类型，可以先持有 `RuntimeLoopPort<Tick>`

### 主循环 / run loop 阶段

- 调一次 `run_once_or_idle(now)`

它会负责：

- 先吃掉到期 tick
- 能跑任务就跑一个 step
- 如果此轮没有可运行工作，则自动 bootstrap idle

### 任务侧

- 任务若想主动让出，调用 `RuntimeThreadPort::yield_current(...)`
- 任务若想睡到某个 tick，调用 `RuntimeThreadPort::sleep_current_until(...)`
- 如果任务侧想走更接近未来 trap/syscall 的入口，则优先通过 `RuntimeTrapServiceFacade<Transport>` 进入 trap service 语义；其下可以绑定 `RuntimeTrapPort` 或 `RuntimeTrapIngressCaller`

## 当前非目标

这层 bridge 自身现在有意不处理下面这些问题：

- 用户态对象模型
- 真正的 context switch 保存/恢复布局
- VBAR / GIC / generic timer 真实寄存器
- 多核 / SMP 调度
- blocking primitive 的完整语义

其中 trap / syscall 入口已经开始由 `kernel.runtime_trap` 单独承接；
其余问题仍然比“最小运行时闭环”更靠后，应该在这条 seam 已经稳定后再接。

## 当前证据路径

当前证据 example：

- `Examples/kernel/runtime_minimal_host`
- `Examples/kernel/runtime_bridge_binding_host`
- `Examples/kernel/runtime_loop_port_host`
- `Examples/kernel/runtime_run_loop_host`
- `Examples/kernel/runtime_tick_host`
- `Examples/kernel/runtime_isr_defer_host`
- `Examples/kernel/runtime_thread_port_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

更细的 seam -> verifier 对照见：

- [`minimal_kernel_runtime_evidence_matrix.md`](minimal_kernel_runtime_evidence_matrix.md)

它验证的就是这条最小闭环：

- 一个 idle
- 一个 worker
- 一个 tick source
- 一次 cooperative `yield`
- 一次 ISR deferred resume
- 一次 `sleep until tick`
- 一条可观察 runtime trace

当前这条 example 已接入 `ctest`，可以作为“上半层运行时语义仍然闭环”的基础证据。

如果需要把当前这批上半层 `runtime_*_host` verifier 一次性做 configure/build/run 回归，可以直接跑 `scripts/minimal_kernel_runtime_host_smoke.ps1`。
日常迭代时更推荐直接走 `scripts/minimal_kernel_runtime_host_smoke_daily.ps1` 复用已有 `cmake-build-verify-*` 目录；只有在想确认冷重建路径、排除缓存影响时再走 `scripts/minimal_kernel_runtime_host_smoke_ci.ps1`，局部排查则可以在这两个入口后面继续加 `-Examples ...` 和 `-Jobs ...` 收窄批次；如果需要把每例状态与耗时固化成工件，则继续加 `-SummaryPath ...`，再用 `scripts/inspect_minimal_kernel_runtime_host_smoke.ps1 -Summary ...` 看慢项与回归。
如果需要把 ARMv7-A QEMU 叶子里的 `runtime-trap / runtime-live / task-syscall` 下半层聚焦 smoke 一次性回归，可以直接跑 `scripts/minimal_kernel_runtime_armv7a_qemu_smoke.ps1`。

## 对 ARMv7-A ingress 的意义

这条 bridge 的意义不是替代 ARMv7-A 的入口实现，而是给它一个更明确的落点：

- ARMv7-A ingress 不需要直接知道 worker/idle 语义细节
- 它只需要在合适的边界调用 `RuntimeBridge`
- 任务侧需要的最小能力通过 `RuntimeThreadPort`、`kernel.runtime_service`、`kernel.task_runtime_api` 或 `kernel.task_syscall_api` 暴露，而不是直接把 scheduler 模板灌进 task context

如果未来 ARMv7-A 的 exception/interrupt/context 继续稳定，这层 bridge 就能自然成为“架构入口到内核运行时”的第一层收口。
