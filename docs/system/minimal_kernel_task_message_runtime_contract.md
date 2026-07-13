# Minimal Kernel Task Message Runtime Contract

> `status`: `supporting`

Task-message runtime 是 minimal-kernel 的异步消息与服务实验，不是 Charm Core、稳定 user ABI、通用 RPC
或 App capability table。准确类型与 API 以 [`Modules/system/kernel`](../../Modules/system/kernel/) 中的
`runtime_mailbox` 和 `task_message_*` source 为准。

## 分层

```text
mailbox transport
  -> label/service routing
  -> budgeted server progress
  -> syscall frame/client/pump
  -> runtime/task API
  -> optional session facade
```

- **Transport/routing**：mailbox 拥有 request/reply、wait 与 timeout；route table 只做固定容量 lookup，
  不拥有 transport 或 service discovery。Missing handler 保持未处理结果。
- **Server progress**：loop 处理单个 ready/timeout event，drain 在一次 wake 内受 budget 限制，pump 编排
  bootstrap/wait/drain/rearm。Queue empty、budget reached、timeout 和 stale event 必须可区分；budget 不
  证明跨 task fairness 或实时上界。
- **Syscall bridge**：frame owner/token 只标识 transport slot，不代表 process isolation、permission 或
  stable wire ABI。Client/pump 保持 asynchronous progress，API 不把 wait 伪装成同步 RPC。
- **Session facade**：handle 只在当前 acceptor/slot 中有效。当前没有 naming、authentication、version
  negotiation、cross-process isolation 或 reliable network transport。

## 共同不变量

- `send/receive/reply/dispatch` 立即返回，等待和 completion 由 event/`step()` 推进。
- Queue、frame、table 和 session slot 使用固定容量，满载显式失败。
- Reply 与 timeout 是不同 completion 分支；late/stale event 不得变成成功。
- `valid()` 只检查局部绑定，不证明完整服务链可运行。
- Thin facade 不拥有 transport、scheduler 或 syscall dispatch 语义。
- Typed protocol 必须保留 raw operation/value；unknown operation 不得静默重写。
- Trace/witness 是局部诊断，不构成稳定 ABI。

## 失败保真

当前层次使用 `bool`、局部 result 和 witness 字段，没有统一错误枚举。组合层仍必须保留以下来源：

- invalid/unbound object；
- queue、frame、table 或 channel capacity exhausted；
- missing label、service、session 或 operation；
- send、publish、wait 或 rearm failure；
- timeout；
- stale/mismatched event、token 或 sequence；
- handler/trap result failure；
- completion queue full/drop。

不得把这些失败压成无来源的 `false` 后交给上层猜测。

## Evidence 与非目标

Host fixtures 位于 `Examples/kernel/runtime_task_message_*`，覆盖 transport、routing、server progress、
syscall frame/client/pump、runtime API 和 session roundtrip。通过只证明对应 fixture，不证明 production
scheduling、fairness、concurrency safety 或 cross-core transport。

该 runtime 不定义产品 IPC/RPC、POSIX syscall ABI、App ABI、process/protection domain、address-space
isolation 或 capability security，也不承诺每个 thin facade 永久保留为独立 module。历史取舍见
[`minimal-kernel-task-message-v0`](../archive/minimal-kernel-task-message-v0/README.md)。
