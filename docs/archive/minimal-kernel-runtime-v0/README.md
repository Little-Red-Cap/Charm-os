# Minimal-kernel Runtime v0 讨论归档

> `status`: `archived`

## 归档原因

早期 runtime 文档按 `bridge`、`mailbox`、`runtime_service`、`task_runtime_api` 分开推进，累计了大量重复的 API 枚举、证据清单和未来叙事。现行源码边界已经可以由一份聚合契约准确说明，因此默认入口收敛到：

- [`../../system/minimal_kernel_runtime_bridge_contract.md`](../../system/minimal_kernel_runtime_bridge_contract.md)

独立证据入口仍保留：

- [`../../system/minimal_kernel_host_smoke_bundle_contract.md`](../../system/minimal_kernel_host_smoke_bundle_contract.md)
- [`../../system/minimal_kernel_runtime_evidence_bundle_contract.md`](../../system/minimal_kernel_runtime_evidence_bundle_contract.md)

kernel module 早期阶段中仍有独立价值的 sync 竞争规则与 thread 职责取舍见
[`kernel-module-milestones/`](kernel-module-milestones/)。Session witness 的 semantic/machine/runtime 分层
见 [`kernel_runtime_session_witness_v0.md`](../../system/kernel_runtime_session_witness_v0.md)；字段、failure
code 与派生规则以 schema/exporter 为准。

## 保留的设计判断

- bridge 与 arch ingress 必须分工：bridge 表达 scheduler/runtime 语义，ingress 负责真实异常帧、寄存器和平台入口。
- loop port 与 thread port 应分离，避免 lower-half 和 task-side 权限面混在一起。
- mailbox 是异步 `send/receive/reply` 机制，不应因 request/reply 形状被称为同步 RPC。
- mailbox 的固定容量、显式 timeout 和失败返回是当前真实约束，不应被愿景语言掩盖。
- runtime service facade 不重写 `TrapResult`，只隐藏 transport 的调用形状。
- task runtime API 的主语是 current task self-service，不是 scheduler management API。

## 尚未实施的讨论

以下方向有讨论价值，但当前没有资格进入现行契约：

- 跨核或跨地址空间 mailbox/RPC；
- 动态队列、backpressure、取消传播和 exactly-once；
- capability namespace 与对象生命周期；
- 用户指针校验、errno/libc facade 与完整用户态 ABI；
- 优先级继承、死锁检测和多核调度一致性。

若以后实现其中任一方向，应从具体源码、失败语义和独立证据重新建立契约。

## 历史故障

- 2026-07-13：`runtime_task_syscall_frame_armv7a_host` 通过，但
  `run_qemu_task_syscall_ci.ps1` 被 GCC 17 modules/libstdc++ 重复定义错误阻断，未进入 QEMU。
