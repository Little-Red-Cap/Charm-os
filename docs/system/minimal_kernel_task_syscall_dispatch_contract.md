# 最小内核 task syscall dispatch 契约（草案）

这份文档用于把“最小 syscall 编号已经存在以后，怎样把它稳定地分派到当前 task-side transport / handler surface”单独收口。

它对应当前新增的：

- `Modules/system/kernel/task_syscall_dispatch.cppm`

目标不是现在就做真正的用户态 syscall ABI，也不是一次性引入完整 handler 注册框架，而是先把下面这件事做成一条薄而稳的 task-side 桥面：

- `TaskSyscallId`
- `TaskSyscallRequest`
- `dispatch(request) -> TrapResult`

## 一句话版本

- `TaskSyscallCatalog` 负责“一个 syscall 号是什么意思”
- `TaskSyscallDispatch` 负责“这个 syscall 号怎么落到最小 transport / handler 上”

前者是目录面，后者是分派面。

## 为什么现在值得加这一层

当前上半层已经有：

- `TaskSyscallApi`
- `TaskSyscallCatalog`
- `RuntimeTrapServiceFacade`
- `RuntimeTrapPort`
- `RuntimeTrapIngressCaller`

这已经足够表达“当前任务可以发起哪些最小 syscall-facing 调用”，也足够表达“这些调用对应哪些 trap service”。

但如果继续往 future syscall number / table / ABI 长，还会缺一块很关键的中间层：

- 目录已经有了
- 调用名已经有了
- 但“怎么把 syscall request 分派给现有 transport / handler surface” 还没有一个稳定落点

如果这一层不单独收出来，后面 host verifier、trace printer、future syscall table 草图很容易又各自维护一份
“`yield` 该调 `yield_current(...)`、`capability_call` 该怎么组装参数” 的重复逻辑。

## 模块位置与关系

模块位置：

- `Modules/system/kernel/task_syscall_dispatch.cppm`

当前建议关系是：

1. `kernel.task_runtime_api`
   - current-task runtime 命名面
2. `kernel.task_syscall_api`
   - current-task syscall-facing 命名面
3. `kernel.task_syscall_catalog`
   - syscall id / 命名 / trap service catalog
4. `kernel.task_syscall_dispatch`
   - syscall request / dispatch / port

这意味着：

- `TaskSyscallApi` 不负责分发表
- `TaskSyscallCatalog` 不负责真正去调用 transport
- `RuntimeTrapServiceFacade` 也不需要自己知道 `TaskSyscallId`

## 当前核心类型

当前新增的核心类型与函数是：

- `TaskSyscallRequest`
- `make_task_syscall_yield_request(...)`
- `make_task_syscall_sleep_until_request(...)`
- `make_task_syscall_debug_write_request(...)`
- `make_task_syscall_capability_call_request(...)`
- `task_syscall_request_from_trap_request(...)`
- `trap_request_from_task_syscall_request(...)`
- `TaskSyscallDispatcher<Surface, TraceBuffer>`
- `TaskSyscallDispatchPort<Tick>`
- `make_task_syscall_dispatch_port(...)`

以及一套独立 trace：

- `TaskSyscallDispatchTraceEvent`
- `TaskSyscallDispatchTraceBuffer<Capacity>`

## 当前最小 request 形状

`TaskSyscallRequest` 当前只表达最小分派所需字段：

- `syscall`
- `arg0`
- `arg1`
- `arg2`
- `arg3`

它故意不在这一层引入：

- 完整用户态寄存器现场
- errno ABI
- 用户态地址空间指针语义
- trap return/writeback 细节

这些都应该继续停在 trap ingress 或 future user ABI 那一侧。

## 当前 dispatcher 责任

`TaskSyscallDispatcher<Surface, ...>` 当前只做一件事：

- 把 `TaskSyscallRequest` 通过 `TaskSyscallCatalogEntry` 的 view/type 信息分派到 `Surface`

当前它要求 `Surface` 至少能提供：

- `valid()`
- `yield_current(TrapYieldCurrentView)`
- `sleep_current_until(TrapSleepUntilView<Tick>)`
- `debug_write(TrapDebugWriteView)`
- `capability_call(TrapCapabilityCallView)`

这意味着它天然可以对接现有这几类 surface：

- `RuntimeTrapServiceFacade`
- `RuntimeTrapPort`
- `RuntimeTrapIngressCaller`
- host/stub verifier 中的 fake transport

## 当前错误语义

这层仍然直接返回：

- `TrapResult`

也就是说，它继续沿用已有：

- `TrapDisposition`
- `TrapError`
- `value`

不引入第二套 syscall dispatch result 协议。

当前最小规则是：

- surface 未绑定 / `valid()==false`
  - `TrapDisposition::rejected`
  - `TrapError::unbound_bridge`
- syscall id 当前不在最小 surface 内
  - `TrapDisposition::unsupported`
  - `TrapError::unsupported_service`

## 当前 port 形状

为了让这层像现有 trap port 一样可挂接，当前还提供：

- `TaskSyscallDispatchPort<Tick>`

它支持：

- `dispatch(request)`
- `dispatch(trap_request)`
- `yield()`
- `sleep_until(...)`
- `debug_write(...)`
- `capability_call(...)`

这层的意义不是取代 `TaskSyscallApi`，而是给：

- host verifier
- future syscall table 草图
- future arch/user boundary

提供一条更接近“syscall request 分派口”的稳定接口。

## 当前 observability

当前 dispatcher 自带独立 trace buffer：

- `TaskSyscallDispatchTraceBuffer`

每条 trace 至少记录：

- `sequence`
- `syscall`
- `trap_service`
- `disposition`
- `error`
- `arg0..arg3`
- `value`

并且当前已经支持：

- `task_syscall_request_from_trace_event(event)`
- `task_syscall_semantic_projection(event)`

也就是说，host verifier 和 future trace printer 不需要再各自手写一套
“这个 syscall dispatch event 里的参数应该叫 `due` 还是 `capability-id`” 的私有格式化逻辑。

## 与现有层的分工

当前建议这样分：

- `TaskSyscallApi`
  - 负责“当前任务怎样调用”
- `TaskSyscallCatalog`
  - 负责“这些调用的编号和命名是什么意思”
- `TaskSyscallDispatch`
  - 负责“这些编号怎样稳定地落到 transport / handler surface”

这三层拆开以后：

- 调用名可以继续演化
- 编号可以单独管理
- 分派桥可以单独验证

而不需要再把名字、编号和 transport 调用细节揉成一个文件。

## 当前证据路径

当前与这层直接相关的独立证据路径是：

- `Examples/kernel/runtime_task_syscall_dispatch_host`

它当前验证：

- request builder / request 与 trap request 的最小转换
- `TaskSyscallDispatcher` 对 `yield/sleep/debug_write/capability_call` 的分派
- `TaskSyscallDispatchPort` 的最小调用口
- 未绑定和 unsupported syscall 的负向路径
- dispatch trace 的语义投影

如果当前要继续看“syscall 号怎样被组织成静态 handler table”，见：

- `docs/system/minimal_kernel_task_syscall_table_contract.md`

## 当前非目标

当前这层仍然不处理：

- 真正用户态 syscall ABI
- 动态 / 可变的 syscall handler registry
- per-process / per-namespace syscall table
- 用户态地址空间和指针校验
- trap ingress 的 decode / writeback 细节

它只是把“最小 syscall request 分派桥”先稳定下来，让 future syscall table 或 user ABI 有干净的落点。
