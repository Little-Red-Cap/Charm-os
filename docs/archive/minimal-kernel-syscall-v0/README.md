# Minimal Kernel Syscall v0 历史摘要

## 文档状态

- `status`: `archived`
- `scope`: task syscall catalog/dispatch/API/frame 分层讨论
- `current entries`:
  [`task syscall`](../../system/minimal_kernel_task_syscall_table_contract.md)、
  [`runtime trap`](../../system/minimal_kernel_trap_syscall_contract.md)、
  [`trap ingress`](../../system/minimal_kernel_trap_ingress_contract.md)

原文档按每个薄模块分别解释命名、catalog、dispatch 和 frame，内容高度重复。当前实现仍保留
这些 module，但文档合并为一条 syscall/trap 链。

## 保留的取舍

- Task-facing API、编号 catalog、request dispatch、handler table 和 frame translation 是不同责任。
- Syscall id 当前复用 trap service 数值，但不应提前承诺永久 ABI。
- Frame adapter 必须分开 capture 与 apply-result，才能区分 decode 和 writeback 失败。
- Static table 的 missing slot 与 unbound handler 是不同错误。
- Host caller/frame fixture 只验证映射，不证明真实 privilege transition。
- Trace/witness 是局部诊断，不是稳定协议。

历史 frame 文档存在编码损坏；当前语义以源码和上列现行契约为准。
