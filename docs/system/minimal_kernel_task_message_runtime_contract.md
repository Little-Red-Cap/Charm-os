# Minimal Kernel Task Message Runtime Contract

## 文档状态

- `status`: `supporting`
- `scope`: `kernel.task_message_*` 模块的当前组合边界
- `authority`: 受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
  [`charm_core_contract.md`](../architecture/charm_core_contract.md) 约束

Task-message runtime 是 minimal-kernel 的异步消息与服务实验。它不是 Charm Core、稳定用户态
ABI、通用 RPC 框架或 App capability table。

## 代码层次

### Transport 与 routing

| 模块 | 责任 |
|---|---|
| `kernel.runtime_mailbox` | request/reply mailbox、等待与 timeout 事件 |
| `kernel.task_message_api` | mailbox 的 task-facing send/receive/reply 包装 |
| `kernel.task_message_table` | `label -> handler` 的固定容量路由 |
| `kernel.task_message_dispatch` | 单条 request 的 lookup、handler 调用与 reply |

Table 不拥有 mailbox transport 或 service discovery。Missing/unbound handler 必须保持未处理结果，
不能伪装成功。

### Server progress

| 模块 | 责任 |
|---|---|
| `kernel.task_message_service_loop` | receive-ready/timeout 的单步推进 |
| `kernel.task_message_service_drain` | 一次唤醒内按 budget 处理多条 request |
| `kernel.task_message_service_pump` | bootstrap、wait、drain、rearm 与 budget hold 编排 |

`step()` 只推进当前事件。Drain budget 只限制一次唤醒，不证明跨 task 公平性或实时上界。
Queue empty、budget reached、timeout 和 stale event 应保持可区分。

### Message-backed syscall

| 模块 | 责任 |
|---|---|
| `kernel.task_message_syscall_bridge` | v0 `label/value` 到 syscall request 的窄映射 |
| `kernel.task_message_syscall_frame` | 固定容量 frame store 与多字段 request transport |
| `kernel.task_message_syscall_frame_caller` | client publish/send/wait/reply completion |
| `kernel.task_message_syscall_client` | 单 pending async call 状态 |
| `kernel.task_message_syscall_pump` | queued request/completion 编排 |
| `kernel.task_message_runtime_service` | runtime operation facade |
| `kernel.task_message_runtime_api` | 当前任务语义的薄包装 |
| `kernel.task_message_syscall_api` | `sys_*` 命名面 |

Frame store 中的 owner/token 标识 transport slot，不是进程隔离、权限或稳定 wire ABI。
Client/pump 保持异步 progress；API 不把等待伪装成同步 RPC。

### Session facade

| 模块 | 责任 |
|---|---|
| `kernel.task_message_session_api` | client `open/request/close` 状态机 |
| `kernel.task_message_session_dispatch` | service id 与 session handler 分发 |
| `kernel.task_message_session_acceptor` | 固定容量 channel slot 与 session handle lookup |
| `kernel.task_message_session_endpoint` | server context 和 reply helper |
| `kernel.task_message_session_protocol` | operation lookup 与 raw handler |
| `kernel.task_message_session_protocol_schema` | typed operation 到 raw request 的投影 |
| `kernel.task_message_session_service` | server ownership facade |
| `kernel.task_message_session_roundtrip` | 组合 witness，不是新 runtime layer |
| `kernel.task_message_session_service_loop` | 组合 witness，不是新 protocol |

Session handle 只在当前 acceptor/slot 实现范围内有效。当前代码不提供命名服务、身份认证、
跨进程隔离、版本协商或可靠网络 transport。

## 共同不变量

- 所有操作保持显式异步；调用方通过 event/step 推进 completion。
- Capacity、queue、frame 和 session slot 由具体模板或配置决定，满载必须显式失败。
- Reply 和 timeout 是不同 completion 分支；late/stale event 不能被当作成功。
- `valid()` 只检查当前对象绑定条件，不证明整个服务链可运行。
- Trace/witness 类型用于局部测试和诊断，不构成稳定 ABI。
- Thin facade 不拥有下层 transport、scheduler 或 syscall dispatch 的语义。
- Typed protocol projection 必须保留 raw operation/value，未知 operation 不应被静默重写。

## 失败分类

当前各层会暴露不同结果类型，但评审时至少应保留这些类别：

- invalid/unbound object；
- queue、frame 或 channel capacity exhausted；
- missing label、service、session 或 operation；
- send/publish/wait/rearm failure；
- timeout；
- stale or mismatched event/token/sequence；
- handler/trap result failure；
- completion queue full or dropped。

不要把这些类别压成一个无来源的 `false` 后再由上层猜测。

## 证据入口

Host examples 位于 `Examples/kernel/runtime_task_message_*`，分别覆盖基础 transport、routing、
server loop、syscall frame/client/pump、runtime API 和 session roundtrip。

组合范围最大的当前样例是：

- `Examples/kernel/runtime_task_message_session_roundtrip_host`
- `Examples/kernel/runtime_task_message_session_service_loop_host`

样例通过只证明其 fixture 覆盖的分支，不证明生产调度、公平性、并发安全或跨核 transport。

## 非目标

- 不定义产品级 IPC/RPC、POSIX syscall ABI 或 App ABI。
- 不定义 process、protection domain、用户/内核地址空间或 capability security。
- 不承诺所有 facade 都应长期保留为独立模块。
- 不要求文档与每个薄 C++ module 一比一对应。

原 23 份逐层草案的独立取舍已压缩到
[`../archive/minimal-kernel-task-message-v0/README.md`](../archive/minimal-kernel-task-message-v0/README.md)。
