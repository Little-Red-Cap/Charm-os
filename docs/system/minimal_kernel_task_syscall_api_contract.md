# 最小内核 task syscall API 契约（草案）

这份文档用于把“如果我们想给当前任务一层更明确、也更接近 future syscall surface 的名字，那么这层 API 应该长什么样”单独收口。

它对应当前新增的：

- `Modules/system/kernel/task_syscall_api.cppm`

目标不是立即定义完整用户态 syscall ABI，而是先在内核 task/worker 侧，把 future syscall surface 的命名边界钉住。

## 一句话版本

- `TaskRuntimeApi<...>` 表达“当前任务的 runtime 自服务”
- `TaskSyscallApi<...>` 表达“当前任务看到的最小 syscall surface”

前者更像 runtime 语义命名，后者则更像 future syscall facade 的名字投影。

## 为什么还要再加这一层

当前我们已经有：

- `kernel.runtime_service`
- `kernel.task_runtime_api`

它们已经能稳定表达：

- `yield`
- `sleep_until`
- `debug_write`
- `capability_call`

但如果我们想继续把这条线长到“future syscall surface”，仍然会遇到一个命名问题：

- `TaskRuntimeApi` 讲的是 runtime 语义
- future syscall facade 讲的是 syscall surface

也就是说，即便能力暂时完全一样，命名边界也值得先拆开。

当前建议做法是：

- 保留 `TaskRuntimeApi`
- 在其之上新增 `TaskSyscallApi`

这样 future 如果再往用户态 trap/syscall boundary 长，当前任务侧已经有一层更合适的命名落点。

## 模块位置与关系

模块位置：

- `Modules/system/kernel/task_syscall_api.cppm`

当前它直接复用并转导出：

- `kernel.task_runtime_api`

也就是说，task-side 只 import：

- `kernel.task_syscall_api`

就能同时拿到：

- `TaskSyscallApi<...>`
- `TaskRuntimeApi<...>`
- `RuntimeTrapServiceFacade<...>`
- `TrapResult`
- 最小 trap view 词汇

当前推荐关系是：

1. `kernel.runtime_service`
   - transport -> task-side services
2. `kernel.task_runtime_api`
   - services -> current-task runtime API
3. `kernel.task_syscall_api`
   - runtime API -> future syscall-facing naming surface

## 当前核心类型

当前核心类型：

- `TaskSyscallApi<Runtime>`
- `make_task_syscall_api(runtime)`

其中 `Runtime` 当前通常是：

- `TaskRuntimeApi<RuntimeTrapServiceFacade<Transport>>`

在新增 `task_syscall_frame` caller 之后，它现在也可以是：

- `TaskSyscallFrameCaller<Frame, Tick>`

但这层并不要求底下 transport 的具体来源，只要求 `Runtime` 已经能提供：

- `valid()`
- `yield(...)`
- `sleep_until(...)`
- `debug_write(...)`
- `capability_call(...)`

## 当前 API 形状

`TaskSyscallApi<Runtime>` 当前对 task/worker context 暴露：

- `valid()`
- `runtime()`
- `bind_runtime(...)`
- `sys_yield()`
- `sys_yield(TrapYieldCurrentView)`
- `sys_sleep_until(due)`
- `sys_sleep_until(TrapSleepUntilView<Tick>)`
- `sys_debug_write(value)`
- `sys_debug_write(TrapDebugWriteView)`
- `sys_capability_call(capability_id, operation, payload=0)`
- `sys_capability_call(TrapCapabilityCallView)`

这层最关键的作用不是增加新能力，而是给“future syscall surface”一个更明确的命名空间：

- `sys_yield()`
- `sys_sleep_until(...)`
- `sys_debug_write(...)`
- `sys_capability_call(...)`

这样未来无论我们要不要再接用户态 trap/syscall compatibility，都不会再反过来重塑 `TaskRuntimeApi` 的语义名字。

## 语义边界

### 1) 这层不重写结果协议

`TaskSyscallApi` 当前仍然直接返回 `TrapResult`。

这意味着：

- `ok()/disposition/error/value` 仍然由 trap/runtime 层定义
- 这层不引入 errno 风格包装
- 这层不引入新的 syscall result 对象

它只是 syscall-facing 命名面，不是 syscall ABI 本身。

### 2) 这层不直接暴露 services 细节

和 `TaskRuntimeApi` 相比，这层把：

- `services()`
- `bind_services(...)`

进一步收成：

- `runtime()`
- `bind_runtime(...)`

也就是说，这层默认假设调用者关心的是“我绑定了一套当前任务 syscall surface”，而不是底下到底有多少 services/transport 层。

### 3) 这层仍然允许 view overload

虽然它提供了 `sys_*` 的标量重载，但 view 重载依旧保留，方便：

- host verifier
- future syscall surface 验证
- 语义更明确的单元测试

## 与 `TaskRuntimeApi` 的分工

当前建议这样分：

- `TaskRuntimeApi`
  - 用 runtime 语义命名当前任务可直接调用的最小自服务能力
- `TaskSyscallApi`
  - 用 syscall-facing 语义命名同一组最小能力

这两层当前能力看起来很像，但职责不同：

- `TaskRuntimeApi` 是 runtime 语义收口
- `TaskSyscallApi` 是 future syscall surface 预留

只要把这两个名字分开，后面不管是继续长 current-task syscall facade，还是接真正用户态 trap/syscall boundary，都会更从容。

## 当前证据路径

当前与这层直接相关的证据路径有五条：

- `Examples/kernel/runtime_task_syscall_host`
- `Examples/kernel/runtime_minimal_host`
- `Examples/kernel/runtime_trap_armv7a_host`
- `Examples/kernel/runtime_task_syscall_frame_caller_host`
- `Examples/kernel/runtime_binding_chain_host`

它们当前分别承担：

- `runtime_task_syscall_host`
  - 独立验证 `TaskSyscallApi` 的 `valid()`、`bind_runtime(...)`、`sys_*` 命名入口与 view overload
- `runtime_minimal_host`
  - 验证 generic host 的真实 worker context 已切到 `TaskSyscallApi<...>`
- `runtime_trap_armv7a_host`
  - 验证 ARMv7-A host trap mapping 证据路径上的 worker context 也已切到这层
- `runtime_task_syscall_frame_caller_host`
  - 验证 `TaskSyscallApi` 现在也可以直接挂在架构无关 numbered syscall frame caller 之上，形成 `sys_* -> frame -> table` 的独立 host 闭环
- `runtime_binding_chain_host`
  - 验证在完整 `TaskSyscallApi<TaskRuntimeApi<RuntimeTrapServiceFacade<...>>>` 链上，`bind_runtime(...)` 与来自下层 `bind_services(...)` / `bind_transport(...)` 的重绑定效果都能被顶层 `sys_*` 直接观察

这说明 `TaskSyscallApi` 已经不是纯概念层，而是开始成为现有 host 证据链里的最高一层 task-facing surface；并且它作为 wrapper 顶层时，对下层 runtime / service 重绑定的可见性也已经有独立证据。

## 当前非目标

当前这层仍然不处理：

- 完整用户态 syscall ABI
- syscall number 分派表
- errno / libc facade
- 用户态地址空间与内存校验
- 完整 capability namespace 对象模型

这些都应该建立在“task-side syscall-facing 命名面已经稳定”之后，再继续往上长。

如果当前要继续讨论“最小 syscall 编号 / catalog 与 trap service 的稳定映射”，见：

- `docs/system/minimal_kernel_task_syscall_catalog_contract.md`

如果当前要继续讨论“最小 syscall request 怎么稳定地分派到 transport / handler”，见：

- `docs/system/minimal_kernel_task_syscall_dispatch_contract.md`

如果当前要继续讨论“最小 syscall 号怎样进入静态 handler table”，见：

- `docs/system/minimal_kernel_task_syscall_table_contract.md`
