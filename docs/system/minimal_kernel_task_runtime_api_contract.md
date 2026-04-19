# 最小内核 task runtime API 契约（草案）

这份文档用于把“当前任务自己发起 `yield / sleep / debug_write / capability_call` 时，代码侧到底应该拿到什么 API 形状”单独收口。

它对应当前新增的：

- `Modules/system/kernel/task_runtime_api.cppm`

目标不是重写 `TaskApi`，也不是把 `runtime_service` 再包成一层大而全框架，而是给 task/worker context 一个更接近未来 syscall facade 的最小自服务入口。

## 一句话版本

- `TaskApi<Scheduler>` 是“谁都可以拿着它管理某个 task”
- `TaskRuntimeApi<Services>` 是“当前 task 拿着它为自己发起最小 runtime/trap 服务”

只要这两层分工不混，后面我们既能保留 scheduler-bound 管理 API，也能继续把 task-side syscall facade 往上长。

## 为什么不直接复用 `TaskApi`

当前仓库里已经有：

- `Modules/system/kernel/task_api.cppm`
- `Modules/system/kernel/thread_api.cppm`

它们的共同特点是：

- 直接依赖 `Scheduler`
- 操作目标显式是 `TaskId`
- 更接近调度管理与对象控制

比如它们表达的是：

- `sleep(task, due)`
- `enable_task(task)`
- `terminate_task(task)`

而 `runtime_service`/trap 这一线表达的是：

- 当前任务自愿让出
- 当前任务睡到某个 tick
- 当前任务向最小 debug/capability service 发起请求

这两类语义的“主语”不同：

- `TaskApi` 的主语是“管理者”
- `TaskRuntimeApi` 的主语是“当前任务自己”

因此当前更健康的做法不是把 trap 语义塞回 `TaskApi`，而是在它旁边新增一条 task-self runtime API。

## 模块位置与关系

模块位置：

- `Modules/system/kernel/task_runtime_api.cppm`

当前它直接复用并转导出：

- `kernel.runtime_service`

也就是说，task-side 只 import：

- `kernel.task_runtime_api`

就能同时拿到：

- `TaskRuntimeApi<...>`
- `RuntimeTrapServiceFacade<...>`
- `TrapResult`
- 最小 trap view 词汇

推荐把当前上半层的关系理解成：

1. `kernel.runtime_trap`
   - 定义最小 trap/service 语义
2. `kernel.runtime_service`
   - 把 transport 细节收口成统一的 task-side service 面
3. `kernel.task_runtime_api`
   - 再把 service 面收成更接近“当前任务自服务”的命名形状
4. `kernel.task_api`
   - 保持 scheduler-bound 的 task 管理 API

如果我们想继续把这层再往“future syscall surface”推进，当前建议继续往上接：

- `docs/system/minimal_kernel_task_syscall_api_contract.md`

## 当前核心类型

当前核心类型是：

- `TaskRuntimeApi<Services>`
- `make_task_runtime_api(services)`

其中 `Services` 当前通常是：

- `RuntimeTrapServiceFacade<Transport>`

但这层并不强制底下 transport 的具体来源，只要求 `Services` 自己已经满足：

- `valid()`
- `yield_current(...)`
- `sleep_current_until(...)`
- `debug_write(...)`
- `capability_call(...)`

## 当前 API 形状

`TaskRuntimeApi<Services>` 当前对 task/worker context 暴露：

- `valid()`
- `services()`
- `bind_services(...)`
- `yield()`
- `yield(TrapYieldCurrentView)`
- `sleep_until(due)`
- `sleep_until(TrapSleepUntilView<Tick>)`
- `debug_write(value)`
- `debug_write(TrapDebugWriteView)`
- `capability_call(capability_id, operation, payload=0)`
- `capability_call(TrapCapabilityCallView)`

这层最关键的变化不是能力变多，而是命名更接近 task 自己的视角：

- `yield()` 代替 `yield_current()`
- `sleep_until(...)` 代替 `sleep_current_until(...)`

也就是说，当前任务代码已经不必再显式把“current”写进每个调用名里，因为这层 API 的存在本身就意味着“主语是当前任务”。

## 语义边界

### 1) 这层不重写 trap 结果协议

`TaskRuntimeApi` 当前仍然直接返回 `TrapResult`。

这意味着：

- `ok()/disposition/error/value` 仍然沿用 trap/runtime 层定义
- 这层不引入 errno 包装
- 这层不引入新的 syscall result 对象

它是 task-facing 命名收口，而不是结果协议重写。

### 2) 这层不暴露 transport 重建细节

和 `runtime_service` 相比，这层把：

- `transport()`
- `bind_transport(...)`

进一步收成：

- `services()`
- `bind_services(...)`

这样 task/worker context 看到的是“我绑定了一组 runtime services”，而不是“我操作某个 transport”。

### 3) 这层仍然允许显式 view

虽然它提供了更自然的标量重载：

- `yield()`
- `sleep_until(due)`
- `debug_write(value)`
- `capability_call(id, op, payload)`

但 view 重载依然保留，方便：

- host verifier
- future syscall facade
- 更明确的语义测试

## 与 `runtime_service` 的分工

当前建议这样分：

- `runtime_service`
  - 解决“底下是哪个 transport、task-side service 面长什么样”
- `task_runtime_api`
  - 解决“当前任务真正想怎么调用这些 service”

如果一个 worker context 直接持有 `RuntimeTrapServiceFacade`，其实已经能工作；
但如果我们想把这层继续长成 future syscall facade，那么当前建议在它之上再加：

- `TaskSyscallApi`

也就是：

- `docs/system/minimal_kernel_task_syscall_api_contract.md`

## 当前证据路径

当前与这层直接相关的证据路径有四条：

- `Examples/kernel/runtime_task_api_host`
- `Examples/kernel/runtime_minimal_host`
- `Examples/kernel/runtime_trap_armv7a_host`
- `Examples/kernel/runtime_binding_chain_host`

它们当前分别承担：

- `runtime_task_api_host`
  - 独立验证 `TaskRuntimeApi` 的 `valid()`、`bind_services(...)`、task-named entry points 与 view overload
- `runtime_minimal_host`
  - 验证 generic host 的真实 worker context 已切到 `TaskRuntimeApi<RuntimeTrapServiceFacade<...>>`
- `runtime_trap_armv7a_host`
  - 验证 ARMv7-A host trap mapping 证据路径上的 worker context 也已切到这层
- `runtime_binding_chain_host`
  - 验证 `bind_services(...)` 的 unbind / rebind 会向上传播到顶层 `TaskSyscallApi`，而不是停留在中间 facade 内部

这说明 `TaskRuntimeApi` 不是只存在于文档中的 future idea，而是已经进入现有 host 证据链；同时它的 `bind_services(...)` 也已经有穿过更高层 wrapper 后仍可稳定观察的证据。

## 当前非目标

当前这层仍然不处理：

- 完整用户态 syscall ABI
- errno / libc facade
- 阻塞对象的高层同步语义
- 用户态内存校验
- capability namespace 的完整对象模型

这些都应该在“当前任务自服务 API 已经稳定”之后，再往上长。
