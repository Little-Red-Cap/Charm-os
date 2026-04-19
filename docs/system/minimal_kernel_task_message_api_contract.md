# 最小内核 task message API 契约（草案）

这份文档用于把 `RuntimeMailbox` 之上的 current-task message surface 收成一条独立边界。

它对应当前新增的：

- `Modules/system/kernel/task_message_api.cppm`

目标不是提前定义完整 IPC 子系统，也不是把 `RuntimeMailbox` 再包成一层更重的 facade，而是先给 task/worker context 一个稳定、薄、可重绑定的消息命名面。

## 一句话版本

- `kernel.runtime_mailbox` 负责承载任务之间的 stateful request/reply 语义。
- `kernel.task_message_api` 负责把这条语义收成“当前任务看到的消息入口”。

如果说前一层证明的是“任务之间已经能彼此说话”，那么这一层证明的是“任务已经可以用 task-facing 的名字去说话，而不是直接摸 mailbox 的底层 current 接口”。

## 为什么单独收这一层

当前仓库里已经有：

- `kernel.runtime_mailbox`
- `kernel.task_runtime_api`
- `kernel.task_syscall_api`

它们分别站在三个不同位置：

- `RuntimeMailbox` 表达任务之间的 stateful kernel object 语义。
- `TaskMessageApi` 表达当前任务对这类消息对象的最小 task-facing 入口。
- `TaskRuntimeApi` / `TaskSyscallApi` 表达当前任务向内核自身发起 runtime/service/trap 请求的入口。

把这几层拆开之后，我们可以同时保留：

- 任务之间的 mailbox object 语义
- 当前任务的 message-facing 命名面
- 当前任务的 runtime/syscall-facing 命名面

这样未来无论我们继续长 mailbox-based IPC，还是往 trap/syscall surface 合并，都不需要反过来扭曲已有命名。

## 模块位置与导出

模块位置：

- `Modules/system/kernel/task_message_api.cppm`

当前它直接转导出：

- `kernel.runtime_mailbox`
- `kernel.eda`
- `kernel.evt`

这样 task/worker context 只 import `kernel.task_message_api`，就能同时拿到：

- `TaskMessageApi<...>`
- `TaskMessageSendView`
- `TaskMessageReplyView`
- `RuntimeMailboxRequest`
- `RuntimeMailboxReply`
- mailbox timeout event helper

## 当前核心类型

- `TaskMessageApi<Mailbox>`
- `TaskMessageSendView`
- `TaskMessageReplyView`
- `make_task_message_api(mailbox)`

其中 `Mailbox` 当前默认对应 `RuntimeMailbox<...>`，但这一层刻意只依赖 mailbox 所暴露的 task-facing current methods，不提前把自己钉死在某个更大的 IPC 框架上。

## 当前 API 形状

`TaskMessageApi<Mailbox>` 当前对 task/worker context 暴露：

- `valid()`
- `mailbox()`
- `server()`
- `bind_mailbox(...)`
- `unbind_mailbox()`
- `send(label, value, sequence=0)`
- `send(TaskMessageSendView)`
- `receive(request)`
- `wait_receive_until(due)`
- `consume_receive_timeout(event)`
- `reply(to, sequence, value)`
- `reply(TaskMessageReplyView)`
- `reply(request, value)`
- `wait_reply_until(due)`
- `receive_reply(reply)`
- `consume_reply_timeout(event)`

这层最关键的作用不是增加新能力，而是把 mailbox 的 current-task 入口正式收成 task-facing 命名：

- `send(...)`
- `receive(...)`
- `wait_receive_until(...)`
- `reply(...)`
- `wait_reply_until(...)`
- `receive_reply(...)`

这样 task/worker context 看到的是“我当前在发消息、等消息、回消息”，而不是“我在直接调用某个 stateful object 的 current_* 内部接口”。

## 当前语义边界

### 1) 这层不重写 `RuntimeMailbox` 的状态机

`TaskMessageApi` 当前只是对 mailbox current methods 的薄包装。

这意味着：

- request/reply queue 的行为仍由 `RuntimeMailbox` 定义
- timeout 的 arm / cancel / consume 仍由 `RuntimeMailbox` 定义
- server/client 路由与 wakeup 也仍由 `RuntimeMailbox` 定义

这层当前不重新定义：

- mailbox 内部缓冲策略
- 多 endpoint 路由
- 更高层 IPC 协议

### 2) 默认构造允许未绑定占位

`TaskMessageApi` 当前允许默认构造，并在未绑定时保持 `valid() == false`。

这条语义是刻意保留的，因为它方便：

- host verifier 先构造 task-facing surface，再显式绑定真实 mailbox
- runtime bootstrap 期间做占位、延后接线
- 后续验证 unbind / rebind 的传播可见性

它当前证明的是“未绑定状态可被安全观察”，不是“默认构造就自动接入某个全局 mailbox”。

### 3) `bind_mailbox(...)` 收的是 task-facing 重绑定，不是 service discovery

`bind_mailbox(...)` / `unbind_mailbox()` 当前表达的是：

- 当前任务手上的 message surface 可以切换到底层 mailbox 对象
- 切换后，顶层 task-facing 入口会直接观察到新的状态与 server

它当前不表达：

- 命名服务
- endpoint lookup
- capability-based mailbox discovery

### 4) 这层不抢占 `TaskRuntimeApi` / `TaskSyscallApi` 的语义位置

`TaskMessageApi` 处理的是“任务和任务之间通过 mailbox 说话”。

`TaskRuntimeApi` / `TaskSyscallApi` 处理的是“任务向内核自身发起 runtime / trap / syscall 请求”。

这两条线未来可能汇合，但当前保持分层更健康：

- `RuntimeMailbox` / `TaskMessageApi`：task-to-task object semantics
- `RuntimeTrapServiceFacade` / `TaskRuntimeApi` / `TaskSyscallApi`：task-to-kernel self-service semantics

如果下一步要继续长 server-side message routing，当前更推荐在这层之上新增独立的：

- `kernel.task_message_table`

而不是反过来让 `TaskMessageApi` 自己变成 label dispatcher。

## 当前证据路径

当前与这层直接相关的证据路径有三条：

- `Examples/kernel/runtime_task_message_host`
- `Examples/kernel/runtime_mailbox_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

它们当前分别承担：

- `runtime_task_message_host`
  - 独立验证 `TaskMessageApi` 的默认未绑定占位、task-named entry points、view overload 与 bind/unbind 行为
- `runtime_mailbox_host`
  - 验证真实 `RuntimeBridge + RuntimeMailbox` host 闭环现在也已通过 `TaskMessageApi` 驱动 `server / client / idle / tick`
- `minimal_kernel_runtime_host_smoke.ps1`
  - 把独立 verifier 与 runtime mailbox demo 一起纳入批量 smoke，确保这层不会脱离现有 runtime 证据网

这说明 `TaskMessageApi` 不是只存在于文档中的 future idea，而是已经成为 `RuntimeMailbox` 之上的正式 task-facing surface，并且它既有独立 host verifier，也有落在真实 runtime loop 上的闭环证据。

## 当前非目标

当前这层仍然不处理：

- 完整 IPC protocol/catalog
- syscall number / trap ingress
- user/kernel ABI 暴露
- 多 server 命名与发现
- channel / stream / rendezvous 等更高层对象模型

当前最重要的不是把消息系统一次做满，而是先把“第一个 task-facing message surface”收成稳定契约，并让它与现有 runtime mailbox 证据链对齐。
