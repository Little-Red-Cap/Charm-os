# 最小内核 runtime mailbox 契约（草案）

这份文档用于把“第一个真正像内核对象的上半层 primitive”收成一条稳定边界。

它不试图提前定义完整 IPC 子系统，也不把 `syscall / trap / capability` 一次性揉进来；当前目标只有一个：

- 让 `send -> recv(timeout) -> reply` 先在最小 runtime 闭环里站稳。

## 一句话版本

- `kernel.runtime_bridge` 负责把任务、tick、idle、调度闭环接起来。
- `kernel.runtime_mailbox` 负责在这条闭环之上放进第一个 stateful kernel object。

如果说前一阶段我们证明了“系统会跑”，那这一层证明的是“系统里的任务已经可以开始彼此说话”。

## 模块位置与职责

模块位置：

- `Modules/system/kernel/runtime_mailbox.cppm`

当前核心对象：

- `RuntimeMailbox<Scheduler, RequestCapacity, ReplyCapacity, ReplyWaitCapacity>`
- `RuntimeMailboxRequest`
- `RuntimeMailboxReply`

当前直接承接这层 task-facing 命名面的模块：

- `Modules/system/kernel/task_message_api.cppm`

它当前负责四件事：

1. 保存发往单个 server task 的请求队列。
2. 保存发往调用者 task 的 reply 队列。
3. 把 `recv(timeout)` 的等待态映射为 scheduler timer + event。
4. 把 `reply` 到达时的唤醒，映射为 scheduler post + timeout cancel。

它当前不负责：

- trap / syscall 编号与 frame ingress
- 用户态地址空间边界
- 多 server endpoint 命名空间
- capability routing policy
- 跨 CPU / SMP mailbox 语义

## 当前接口形状

当前 mailbox 保持在 very thin 的 runtime object 形状：

- `send(...)`
- `send_current(...)`
- `receive(...)`
- `receive_current(...)`
- `wait_receive_until(...)`
- `wait_receive_current_until(...)`
- `consume_receive_timeout(...)`
- `reply(...)`
- `reply_current(...)`
- `receive_reply(...)`
- `receive_reply_current(...)`
- `wait_reply_until(...)`
- `wait_reply_current_until(...)`
- `consume_reply_timeout(...)`

配套事件 helper：

- `make_runtime_mailbox_receive_event()`
- `make_runtime_mailbox_receive_timeout_event()`
- `make_runtime_mailbox_reply_event()`
- `make_runtime_mailbox_reply_timeout_event()`

当前这层的 current-task message naming 已经单独收成：

- `kernel.task_message_api`

这里有两个刻意收口的点：

1. mailbox 只依赖现有 scheduler/runtime 闭环，不要求 trap surface 先稳定。
2. mailbox 提供 current-task 便捷入口，但不强迫 future syscall 命名现在就定案。

## 当前语义边界

### 1) `send(...)` 只是投递请求，不是同步 RPC

当前 `send(...)` 的语义是：

- 把 request 放进 mailbox 的 request queue
- 如 server 正在 `recv(timeout)` 等待，则取消那次等待的 timeout
- 给 server 投递 receive-ready event

它当前不保证：

- 调用者在 `send(...)` 返回时已经拿到 reply
- request 一定立刻被 server 消费
- 多调用者之间的公平调度策略已经冻结

### 2) `wait_receive_until(...)` 是 runtime 等待，不是线程阻塞原语大全

当前 `wait_receive_until(due)` 的语义是：

- 给 mailbox 记下一次“server 正在等消息”的等待态
- 用 scheduler timer 在 `due` 时投递 receive-timeout event

它当前不试图替代：

- 通用 wait set
- 条件变量
- select/poll 风格多路等待

### 3) `reply(...)` 只证明 request/reply 因果链

当前 `reply(...)` 的语义是：

- 把 reply 放进 reply queue
- 如 client 正在等 reply，则取消对应 timeout
- 给 client 投递 reply-ready event

它当前只证明“被请求方可以把结果送回调用者”，还没有上升到：

- 完整消息协议
- 结构化 capability object
- 用户态 syscall ABI

## 与现有上半层模块的关系

推荐把当前上半层理解成：

1. `kernel.runtime_glue`
   - 无状态调度原语
2. `kernel.runtime_bridge`
   - stateful runtime loop / idle / tick bridge
3. `kernel.runtime_mailbox`
   - 第一个运行在这条 bridge 之上的 stateful kernel object
4. `kernel.task_message_api`
   - `RuntimeMailbox` 之上的 current-task message surface
5. `kernel.runtime_service`
   - task-side trap/service facade
6. `kernel.task_runtime_api`
   - current-task runtime 命名面
7. `kernel.task_syscall_api`
   - future syscall-facing 命名面

也就是说，`runtime_mailbox` 不是 `runtime_service` 的替代品。

两条线现在分别在证明：

- `runtime_mailbox`：任务之间的 runtime object 语义
- `task_message_api`：站在 mailbox 之上的 current-task message 命名与重绑定语义
- `runtime_service/task_runtime_api/task_syscall_api`：任务自身向内核发起最小 service/trap 请求的语义

这两条线未来会汇合，但当前不必强行揉成一层。

## 当前证据路径

当前与这层直接相关的证据路径是：

- `Examples/kernel/runtime_task_message_host`
- `Examples/kernel/runtime_mailbox_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

- `runtime_task_message_host` 当前专门证明：
  - `TaskMessageApi` 的默认未绑定占位
  - task-named `send / receive / reply / wait_*` 入口
  - `bind_mailbox(...)` / `unbind_mailbox()` 的重绑定可见性
- `runtime_mailbox_host` 当前专门证明：

- server 先进入 `recv(timeout)` 等待
- 首次等待可以被 timer 超时唤醒
- client 后续 `send(...)` 会取消 server 的下一次等待超时
- server 成功 `receive(...)` request 并 `reply(...)`
- client 的 `wait_reply_until(...)` 会在 reply 到达时被取消 timeout
- 整条链在 `RuntimeBridge` 驱动的 `server / client / idle / tick` host 闭环里可观察

它当前不证明：

- trap/frame ingress
- ARMv7-A leaf 进入 mailbox
- 多 client / 多 server 的路由策略

## 当前非目标

当前这层仍然不处理：

- 命名端口与 service discovery
- 多生产者公平性 / 优先级继承
- 零拷贝消息缓冲
- 同步 RPC、channel、stream 的完整对象模型
- user/kernel ABI 暴露

当前最重要的不是把 IPC 一次做完，而是先把“第一个能说话的内核对象”做成稳定证据。
