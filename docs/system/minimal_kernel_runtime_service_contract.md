# 最小内核 task-side runtime service 契约（草案）

这份文档用于把“任务侧到底应该怎么看待 `yield / sleep / debug_write / capability_call` 这组最小 syscall/trap 入口”单独收口。

它对应当前上半层新增的：

- `Modules/system/kernel/runtime_service.cppm`

目标不是把 `kernel.runtime_trap` 再包装成一层大而全的 API，而是给 task/worker context 一个更稳定、也更接近未来 syscall facade 的最小入口面。

## 一句话版本

- `kernel.runtime_trap` 负责定义 trap/service 语义
- `kernel.runtime_service` 负责把这些语义变成 task-side 可直接持有的最小服务面

只要这两层不混，未来无论底下接的是 `RuntimeTrapPort`、`RuntimeTrapIngressCaller`，还是再往后真正的 arch/user boundary，任务侧代码都不需要跟着重写 transport 形状。

## 为什么单独需要这层

当前上半层已经有两类 trap transport：

- `RuntimeTrapPort<Tick>`
- `RuntimeTrapIngressCaller<Frame, Tick>`

它们都能表达：

- `yield_current`
- `sleep_current_until`
- `debug_write`
- `capability_call`

但它们本身更像“transport 落点”，而不是 task 眼里的服务面。

任务代码如果直接持有它们，会同时暴露出一些此阶段不必要的细节：

- transport 来自 runtime bridge 直连还是 ingress caller
- host/stub 证据路径上的 frame builder 形状
- 哪些 helper 是为了 caller/adapter 存在，而不是为了 task 本身存在

因此当前建议新增一层很薄的 task-side facade，把 transport 细节挡在 worker context 外面。

## 模块位置与职责

模块位置：

- `Modules/system/kernel/runtime_service.cppm`

模块当前直接复用并转导出 `kernel.runtime_trap` 的最小 view/result 词汇，因此 task-side 只 import `kernel.runtime_service` 也能拿到这层 facade 所需的 trap 语义类型。

当前核心类型：

- `RuntimeTrapServiceFacade<Transport>`
- `make_runtime_trap_service_facade(transport)`

它的职责只做三件事：

1. 持有一个可替换的 trap transport。
2. 给任务侧暴露统一命名的最小 service 入口。
3. 保持 transport 原有的 `TrapResult` / 错误语义，不额外发明第二套返回协议。

它当前不负责：

- 解释 trap frame
- 暴露 `TrapRequest`
- 决定 scheduler event policy
- 处理 arch origin / return frame / writeback

这些职责仍然留在 `kernel.runtime_trap` 与 `kernel.runtime_trap_ingress`。

如果我们想把这层再往“当前任务真正会直接拿到的 API 名字”继续收口，当前建议继续往上接：

- `docs/system/minimal_kernel_task_runtime_api_contract.md`
- `docs/system/minimal_kernel_task_syscall_api_contract.md`

## 当前 transport 约定

`RuntimeTrapServiceFacade<Transport>` 当前假设底下 transport 至少提供：

- `using tick_type = ...`
- `valid()`
- `yield_current(TrapYieldCurrentView)`
- `sleep_current_until(TrapSleepUntilView<tick_type>)`
- `debug_write(TrapDebugWriteView)`
- `capability_call(TrapCapabilityCallView)`

也就是说，这层 facade 并不要求 transport 的具体来源一致，只要求它们对最小 service view 的理解一致。

## 当前 task-side 入口面

当前 facade 对任务侧暴露的入口是：

- `valid()`
- `transport()`
- `bind_transport(...)`
- `yield_current()`
- `yield_current(TrapYieldCurrentView)`
- `sleep_current_until(due)`
- `sleep_current_until(TrapSleepUntilView<Tick>)`
- `debug_write(value)`
- `debug_write(TrapDebugWriteView)`
- `capability_call(capability_id, operation, payload=0)`
- `capability_call(TrapCapabilityCallView)`

其中标量重载的意义不是引入新语义，而是把常见 task-side 调用写法收成更自然的形状：

- `yield_current()` 而不是手写空 view
- `sleep_current_until(due)` 而不是每次显式构造 `TrapSleepUntilView`
- `debug_write(value)` 而不是每次手写 `TrapDebugWriteView`
- `capability_call(id, op, payload)` 而不是每次手写 `TrapCapabilityCallView`

view 重载则继续保留，方便 host verifier、future syscall facade 或更复杂的调用方保持显式语义。

## 语义边界

### 1) facade 不重新定义返回值语义

`RuntimeTrapServiceFacade` 当前直接转发底层 transport 返回的 `TrapResult`。

这意味着：

- `ok()/disposition/error/value` 的解释仍然由 trap/runtime 层定义
- facade 本身不做 errno 包装
- facade 本身不做额外错误翻译

换句话说，这层是“task-side 入口面”，不是“第二套 trap protocol”。

### 2) facade 不暴露 origin/frame 细节

当前 task-side facade 有意不让 worker 直接接触：

- `TrapOrigin`
- `TrapRequest`
- `TrapFrameView`
- caller-side frame adapter

这是因为这些信息更接近：

- arch/service 边界
- host/stub verifier
- ingress adapter

而不是当前 task 需要直接关心的内容。

### 3) `bind_transport(...)` 是 transport 重定向，不是重建语义

`bind_transport(...)` 当前只是把 facade 持有的底层 transport 换成新的实例。

它的意义主要是：

- host verifier 可以先构造未绑定 facade，再后续接上 transport
- task context 可以保持稳定字段布局，而不把 transport 初始化顺序泄漏出去
- 后续如果 transport 来源变化，task 侧入口面不需要跟着变化

### 4) `valid()` 是 task-side 的最小就绪探针

`valid()` 当前只是转发 transport 的可用性判断。

它的作用不是替代完整生命周期管理，而是给 task/worker/host verifier 一个最小、统一的“当前是否已接线”探针。

## 当前建议调用形状

当前建议任务侧优先持有：

- `RuntimeTrapServiceFacade<Transport>`

而不是直接持有：

- `RuntimeTrapPort<Tick>`
- `RuntimeTrapIngressCaller<Frame, Tick>`

建议的形状是：

1. 下半层或 host verifier 负责构造 transport。
2. task/worker context 只持有 facade。
3. worker step 只调用 facade 上的最小 service 面。

这让任务代码可以稳定写成：

- `runtime.yield_current()`
- `runtime.sleep_current_until(due)`
- `runtime.debug_write(value)`
- `runtime.capability_call(id, op, payload)`

而不需要知道 transport 背后是 runtime bridge 直连还是 ingress caller。

## 当前证据路径

当前与这层 facade 直接相关的证据路径有三条：

- `Examples/kernel/runtime_minimal_host`
- `Examples/kernel/runtime_trap_armv7a_host`
- `Examples/kernel/runtime_service_host`

它们当前分别承担：

- `runtime_minimal_host`
  - 证明 facade 可以挂在 generic host 的 ingress caller 之上，并参与最小 runtime/trap 闭环
- `runtime_trap_armv7a_host`
  - 证明 facade 可以挂在 ARMv7-A host trap mapping 之上，而不暴露 arch frame 细节给 worker
- `runtime_service_host`
  - 证明 facade 自身的 `valid()`、`bind_transport(...)`、标量/视图双重重载，以及 default payload 等 task-side 语义

这三条证据合起来说明：

- facade 不是只存在于文档里的包装层
- 它已经同时有“接在真实 trap transport 之上”的证据
- 也有“脱离 scheduler/ingress 细节独立验证自身语义”的证据

## 与其它上半层模块的关系

推荐把这几层理解成：

1. `kernel.runtime_bridge`
   - 运行时调度胶水与 stateful bridge
2. `kernel.runtime_trap`
   - trap/service 语义与最小结果协议
3. `kernel.runtime_trap_ingress`
   - frame capture / writeback / caller-side frame builder
4. `kernel.runtime_service`
   - task-side facade
5. `kernel.task_runtime_api`
   - current-task 视角的最小 runtime API
6. `kernel.task_syscall_api`
   - future syscall-facing 命名面

也就是说，`kernel.runtime_service` 不是替代前面三层，而是把“任务真正想拿到的那一面”从 transport 细节里摘出来；`kernel.task_runtime_api` 继续把这层名字收成 current-task runtime API，而 `kernel.task_syscall_api` 则把它再往 future syscall surface 推进一步。

## 当前非目标

当前这层仍然不处理：

- 完整 syscall ABI
- 用户态地址空间切换
- errno / libc 风格包装
- capability namespace 的完整对象模型
- 阻塞原语的高层 facade

这些都应该建立在“task-side 最小 service 面已经稳定”之后，再继续往上长。
