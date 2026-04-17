# 最小内核 Trap / Syscall 边界契约（草案）

这份文档用于把“异常入口已经建立以后，怎样把 trap / syscall 语义接到最小内核运行时里”写清楚。

它的目标不是一步走到 Linux syscall 兼容，也不是现在就承诺完整用户态模型，而是先把下面这件事做成一条稳定、可验证、可回归的窄桥面：

- trap frame / trap request
- service 编号到内核动作的映射
- 参数与返回值表达
- 失败时留下可观察证据

## 一句话版本

- 异常入口负责“把机器态 trap 变成可解释请求”
- trap bridge 负责“把请求变成最小内核动作”

只要这两件事分开，后面无论往 ARMv7-A SVC、最小用户任务、还是更高层 POSIX facade 继续长，都不会把异常入口和内核服务入口重新揉成一团。

## 当前模块分层

当前建议把这条路径收成四层：

### 1) `kernel.runtime_glue`

位置：`Modules/system/kernel/runtime_glue.cppm`

职责是最薄的运行时原语：

- `tick -> scheduler`
- `ISR defer -> post`
- `yield/sleep` 的最底层 glue

### 2) `kernel.runtime_bridge`

位置：`Modules/system/kernel/runtime_bridge.cppm`

职责是带状态的运行时桥：

- 绑定 scheduler
- 绑定 idle task / idle event
- 绑定 runtime trace

### 3) `kernel.runtime_trap`

位置：`Modules/system/kernel/runtime_trap.cppm`

职责是把“trap/service 语义”独立出来，而不是让 thread/task 直接理解 scheduler 的事件驱动细节。

它当前提供：

- `TrapFrameView`
- `TrapRequest`
- `TrapYieldCurrentView`
- `TrapSleepUntilView<Tick>`
- `TrapDebugWriteView`
- `TrapCapabilityCallView`
- `TrapResult`
- `RuntimeTrapBridge`
- `RuntimeTrapPort`
- `RuntimeTrapServiceFacade`
- `RuntimeTrapTraceBuffer`

### 4) `kernel.runtime_service`

这层提供一个 task-side facade：

- `RuntimeTrapServiceFacade<Transport>`

它不关心底下绑定的是 `RuntimeTrapPort` 还是 `RuntimeTrapIngressCaller`，只负责把任务侧真正会用到的最小 trap service 入口收成统一形状：

- `yield_current(...)`
- `sleep_current_until(...)`
- `debug_write(...)`
- `capability_call(...)`

这让 task context、host verifier、未来更高层的 syscall facade 都不需要直接暴露 transport 细节。

更完整的 task-side 边界说明见：

- `docs/system/minimal_kernel_runtime_service_contract.md`
- `docs/system/minimal_kernel_task_runtime_api_contract.md`
- `docs/system/minimal_kernel_task_syscall_api_contract.md`

### 5) 未来的 arch trap ingress

这层现在已经有了第一版上半层草图：

- `Modules/system/kernel/runtime_trap_ingress.cppm`
- `docs/system/minimal_kernel_trap_ingress_contract.md`

它的目标已经清楚：

- ARMv7-A 的 SVC / trap handler 不直接调 scheduler
- 它先把寄存器现场解释成 `TrapFrameView`
- 再交给 `RuntimeTrapIngress` / `RuntimeTrapBridge`
- host/stub 证据路径上的调用侧则可以通过 `RuntimeTrapIngressCaller` 复用最小 frame builder，并在任务侧进一步包进 `RuntimeTrapServiceFacade`

## 当前最小 service 面

当前草图先承诺下面四项已经落到代码里：

- `yield_current`
- `sleep_until`
- `debug_write`
- `capability_call`

这代表当前 trap 面的重点不是“服务多”，而是“服务入口和运行时入口不再混用”。

## 当前最小 service catalog

当前上半层已经把这四个最小 service 收进统一的 catalog：

- `trap_service_catalog_entry(service)`

每个 entry 当前至少描述：

- `service_name`
- `view_kind`
- `wire_argument_count`
- `wire_argument_names`
- `result_name`
- `supported`

也就是说，后面的 caller、trace printer、host verifier、未来 syscall 草图，都不需要再各自手写一份
“`yield_current` 有几个参数、`capability_call` 应该按哪种 view 理解” 的私有表。

当前最小 catalog 可理解为：

- `yield_current` -> `TrapYieldCurrentView` / `wire_argument_count=0` / `result_name=accepted`
- `sleep_until` -> `TrapSleepUntilView<Tick>` / `wire_argument_count=1` / `wire_argument_names={ due }`
- `debug_write` -> `TrapDebugWriteView` / `wire_argument_count=1` / `wire_argument_names={ value }`
- `capability_call` -> `TrapCapabilityCallView` / `wire_argument_count=3` / `wire_argument_names={ capability-id, operation, payload }`

并且当前上半层已经开始提供统一的：

- `trap_semantic_projection(request)`
- `trap_semantic_projection(trap_trace_event)`

也就是说，调用侧、trace printer、host verifier 不需要再各自维护一套
“这个 service 应该把哪些字段打印成什么名字”的重复逻辑。

## 当前最小 task syscall catalog

在 `trap_service_catalog_entry(...)` 之外，上半层现在还单独提供：

- `Modules/system/kernel/task_syscall_catalog.cppm`
- `Modules/system/kernel/task_syscall_dispatch.cppm`
- `Modules/system/kernel/task_syscall_table.cppm`
- `Modules/system/kernel/task_syscall_frame.cppm`
- `TaskSyscallId`
- `TaskSyscallCatalogEntry`
- `task_syscall_catalog_entry(...)`
- `trap_service_from_task_syscall(...)`
- `task_syscall_from_trap_service(...)`
- `task_syscall_semantic_projection(...)`

它的作用不是替代 trap service catalog，而是把下面三者之间的关系钉住：

- `TaskSyscallApi` 上的 `sys_*` 命名面
- 当前 trap/service catalog
- future syscall number / dispatch surface

这让我们可以同时保留：

- trap/service 侧的 `yield_current`
- task syscall 侧的 `yield`

而不需要把 runtime 语义命名和 syscall-facing 命名重新揉回同一层。

并且 `task_syscall_dispatch` 还进一步把：

- `TaskSyscallRequest`
- `TaskSyscallId`
- `dispatch(request) -> TrapResult`

收成一层很薄的 task-side dispatch bridge。

在这之上，`task_syscall_table` 又把：

- `TaskSyscallHandlerEntry`
- `TaskSyscallTable`

收成最小静态“号 -> handler”表。

再往前一层，`task_syscall_frame` 则把：

- `TaskSyscallFrameView`
- `TaskSyscallFrameAdapter<Frame>`
- `TaskSyscallFrameBridge<Table, Frame, ...>`

收成最小“numbered frame -> request -> table -> writeback”桥。

## 当前核心数据结构

### `TrapFrameView`

它表达“从架构 trap frame 中抽出来的、架构无关的最小视图”：

- `service_id`
- `arg0..arg3`
- `return_pc`
- `stack_pointer`
- `status`
- `origin`
- `task / task_valid`

这里有意不把它定义成真正的 ARMv7-A trap frame。

原因是：

- ARMv7-A 的真实保存布局应该继续停在 arch/leaf 侧
- 上半层只需要一个“能被解释的最小视图”

### `TrapRequest`

它表达“已经完成基本解释的 trap 请求”。

也就是说：

- `TrapFrameView` 更像 arch ingress 的输入视图
- `TrapRequest` 更像 runtime trap bridge 的调度输入

### `TrapYieldCurrentView`

它表达“当前任务主动让出”这件事本身。

这层 view 当前没有额外字段，但它依然有价值，因为它把：

- `yield_current` 这个 service 的命名语义

和

- 调用侧是否直接手写一个 `TrapRequest`

拆开了。

当前代码里，`RuntimeTrapPort`、`RuntimeTrapIngressCaller` 与 `RuntimeTrapServiceFacade` 都已经接受这层 view。

### `TrapSleepUntilView<Tick>`

它表达“当前任务睡到某个 tick”为止：

- `due`

这里保留模板参数 `Tick`，是为了让 trap 调用侧继续沿用自己所在 runtime 的 tick 类型；
而线上编码仍然统一收进 `TrapRequest.arg0` 这一个最小槽位里。

### `TrapCapabilityCallView`

它表达“已经从 `TrapRequest` 里抽出来的最小 capability 调用语义”：

- `capability_id`
- `operation`
- `payload`

这层 view 的作用不是替代 `TrapRequest`，而是把：

- trap wire format 仍然保持 `arg0..arg2`

和

- capability handler / caller 侧想要使用的命名字段

拆开。

当前代码里，`RuntimeTrapPort`、`RuntimeTrapIngressCaller` 与 `RuntimeTrapServiceFacade` 都已经接受这层 view；
而 capability handler 也可以先把 `TrapRequest` 收成 `TrapCapabilityCallView`，再决定是否接受这次调用。

### `TrapDebugWriteView`

它表达“最小 debug 输出服务想写出的 payload”：

- `value`

它的意义和 `TrapCapabilityCallView` 一样，都是把：

- trap 层线上仍然保持的紧凑参数槽位

和

- caller / handler 侧真正想表达的命名语义

拆开。

当前代码里，`RuntimeTrapPort`、`RuntimeTrapIngressCaller` 与 `RuntimeTrapServiceFacade` 也都已经接受这层 view；
而 debug handler 则可以通过 `trap_debug_write_view(request)` 统一解码。

### `TrapResult`

它表达“这次 trap/service 调用的处理结果”：

- `disposition`
- `error`
- `value`

当前约定里：

- `disposition=handled` 表示服务已被消费
- `disposition=rejected` 表示语义上不允许或参数不成立
- `disposition=unsupported` 表示服务号当前不在最小实现面内

## 当前 origin 语义

当前 `TrapOrigin` 先收成四类：

- `kernel_thread`
- `user_task`
- `supervisor`
- `isr`

当前 trap bridge 明确拒绝：

- `origin=isr`

因为 ISR 路径的责任应当继续保持为：

- 最短路径记账
- defer 到统一调度语义

而不是把“中断上下文发起 syscall/trap”混进当前主线。

## 当前参数与返回值规则

### 参数

当前最小规则是：

- `yield_current`：
  - 语义视图是 `TrapYieldCurrentView{}`
  - 线上最小编码不需要额外参数
- `sleep_until`：
  - 语义视图是 `TrapSleepUntilView<Tick>{ due }`
  - 线上的最小编码仍然是 `arg0 = due tick`
- `debug_write`：
  - 语义视图是 `TrapDebugWriteView{ value }`
  - 线上的最小编码仍然是 `arg0 = debug payload`
- `capability_call`：
  - 语义视图是 `TrapCapabilityCallView{ capability_id, operation, payload }`
  - 线上的最小编码仍然是 `arg0 = capability_id`、`arg1 = operation`、`arg2 = payload`

这意味着 trap 调用方不需要理解 scheduler 内部的事件驱动细节。

### 返回值

当前最小规则是：

- `value=1`：`yield_current` 已被接受
- `value=due`：`sleep_until` 已被接受并记录了目标 tick
- `value=bytes_written`：`debug_write` 已被消费并写入 debug sink
- `value=handler-defined response`：`capability_call` 已被 capability handler 消费

当前 host verifier 的 capability 样本是：

- `capability_id=7`
- `operation=2`
- `payload=33`
- 返回 `value=42`

- `error!=none`：调用失败

当前没有引入更复杂的 errno / negative-return ABI。

这不是否定以后会有那层，而是避免现在为了未来 ABI 一次性把当前最小内核主线拖复杂。

## 为什么需要 policy

当前内核主线仍然是事件驱动的。

这意味着 trap 面要解决一个关键问题：

- trap 调用者希望表达“yield / sleep”
- 但 scheduler 侧实际需要的是“投递哪个 resume event / wake event”

因此 `RuntimeTrapBridge` 当前引入了很薄的 `RuntimeTrapPolicy`：

- `yield_resume_event`
- `sleep_resume_event`
- `sleep_event_factory(due)`

它的作用是把：

- trap/service 语义

和

- 当前 event-driven kernel 内部细节

隔开。

这是这一层最值钱的事情之一，因为它避免 future user task / SVC path 直接绑死在 `EventId::user1` 这类内部约定上。

## 当前 observability

当前 trap bridge 带有独立的 `RuntimeTrapTraceBuffer`。

它记录：

- 时间戳
- service
- origin
- task
- disposition
- error
- `arg0..arg3`
- `value`

并且当前 trace event 已经保留了足够的参数槽位，所以上半层可以继续通过：

- `trap_yield_current_view(trace_event)`
- `trap_sleep_until_view<Tick>(trace_event)`
- `trap_debug_write_view(trace_event)`
- `trap_capability_call_view(trace_event)`

把 trace 重新投影回同一套 service view，而不需要每个 verifier 自己猜 `arg0` 到底代表什么。

这意味着最小 trap 面已经满足一个重要原则：

- 成功调用有证据
- 失败调用也有证据

当前 host verifier 已经显式覆盖下面这些失败面：

- `no_current_task`
- `invalid_origin`
- `invalid_argument`
- `unsupported_service`
- `decode_failed`
- `unbound_bridge`

后面如果 ARMv7-A SVC ingress 接上来，我们至少不会处在“只知道 trap 进来了，但不知道被怎样处理掉了”的黑盒状态。

## 当前证据路径

当前证据 example：

- `Examples/kernel/runtime_minimal_host`
- `Examples/kernel/runtime_trap_armv7a_host`

它已经通过 trap port 走通：

- `yield_current`
- `sleep_until`
- `debug_write`
- `capability_call`

并且当前两条 host 证据已经不再各自维护一份临时 `RuntimePort`，而是共用：

- `RuntimeTrapCallFrameAdapter`
- `RuntimeTrapIngressCaller`

并在 worker/task context 一侧统一通过：

- `RuntimeTrapServiceFacade`

其中 `Examples/kernel/runtime_minimal_host` 当前还额外提供：

- `debug_write` 被实际消费后的 debug sink 证据
- `capability_call` 被 capability handler 消费后的 capability sink 证据

并且已经把下面这些负向证据也收进回归：

- `unknown service / unsupported_service`
- `capability_call / invalid_argument`
- `yield_current` 在无 current context 下返回 `no_current_task`
- `origin=isr` 返回 `invalid_origin`
- ingress capture 失败返回 `decode_failed`
- 未绑定 trap bridge 的 port 返回 `unbound_bridge`

并且输出：

- runtime trace
- trap trace
- capability sink trace
- debug sink trace
- scheduler trace

这条证据说明：在不触碰 arch hot zone 的前提下，最小 trap/service 语义已经能落到现有 runtime bridge 上。

## 对 ARMv7-A SVC ingress 的意义

这层的真正意义不是 host demo 本身，而是给后续 arch ingress 一个稳定落点：

1. arch trap handler 解释硬件现场
2. 生成 `TrapFrameView`
3. 调 `dispatch_frame(...)`
4. 由 trap bridge 把服务映射到 runtime bridge

这样就形成了一条更健康的责任链：

- arch 负责寄存器现场和异常返回
- trap bridge 负责服务语义
- runtime bridge 负责最小内核运行时

## 当前非目标

当前这层还不处理下面这些问题：

- 完整 Linux syscall 编号体系
- 完整 errno ABI
- 用户态地址空间隔离
- ELF 进程装载
- trap return frame 的真实硬件恢复顺序
- capability / handle 的完整对象模型

这些内容都应该在“最小 trap 面已经稳定”之后，再按真实需求往上长。
