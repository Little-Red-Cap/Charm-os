# Minimal Kernel Task Message v0 历史摘要

## 文档状态

- `status`: `archived`
- `scope`: task-message runtime 分层演进中的设计取舍
- `current contract`:
  [`../../system/minimal_kernel_task_message_runtime_contract.md`](../../system/minimal_kernel_task_message_runtime_contract.md)

Task-message 实现曾按每个新增 facade 拆出一份契约草案，形成 23 份重复文档。代码模块仍可按
责任细分，但长期文档只保留组合边界。

## 保留的取舍

- Mailbox transport、label routing、server progress 和 syscall/session facade 不应互相拥有。
- Server `loop -> drain -> pump` 的切分用于区分单步事件、单次唤醒 budget 和 task body 编排。
- v0 `label/value` syscall bridge 很窄；frame transport 用于承载更完整 request，不代表稳定 ABI。
- Client 先保持单 pending，再由 pump 提供有界 queue；这不是同步 future/promise。
- Session `open/request/close` 建立最小状态机，acceptor 用固定 slot 管理 handle。
- Endpoint/protocol/schema 是 server-side 投影层，不应隐藏 raw operation 或下层错误。
- Roundtrip/service-loop 文件是组合证据，不应被误读为新协议层。
- Budget 只证明单次 drain 有界，不证明全局公平、deadline 或实时性。

## 未证明

- 跨核、跨进程或网络 transport；
- memory protection、身份认证、授权或取消传播；
- 多 client 并发公平；
- service discovery、版本协商和 schema compatibility；
- product-stable syscall/session ABI。

具体类型和错误分支应以当前 module source 与 host example 为准。
