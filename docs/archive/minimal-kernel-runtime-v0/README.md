# Minimal-kernel Runtime v0 讨论归档

## 归档原因

早期 runtime 文档按 `bridge`、`mailbox`、`runtime_service`、`task_runtime_api` 分开推进，累计了大量重复的 API 枚举、证据清单和未来叙事。现行源码边界已经可以由一份聚合契约准确说明，因此默认入口收敛到：

- [`../../system/minimal_kernel_runtime_bridge_contract.md`](../../system/minimal_kernel_runtime_bridge_contract.md)

独立证据入口仍保留：

- [`../../system/minimal_kernel_host_smoke_bundle_contract.md`](../../system/minimal_kernel_host_smoke_bundle_contract.md)
- [`../../system/minimal_kernel_runtime_evidence_bundle_contract.md`](../../system/minimal_kernel_runtime_evidence_bundle_contract.md)

本目录同时保留 session witness 的早期完整设计：

kernel module 的早期结构、M1 sync、M2 thread 与 M3 observability 阶段材料见
[`kernel-module-milestones/`](kernel-module-milestones/)。这些文件中的 freeze、demo 路径和 CSV
字段不约束当前源码。

- [`kernel_runtime_session_witness_v0.md`](kernel_runtime_session_witness_v0.md)：保留 semantic/machine/runtime
  分层、failure taxonomy、world compare 与 runner 消费链的原始讨论。现行 supporting contract 已收敛到
  [`../../system/kernel_runtime_session_witness_v0.md`](../../system/kernel_runtime_session_witness_v0.md)。
- [`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md)：
  保留 Host/QEMU 总证据链的完整产物树、session 旁路 smoke、inspect 命令矩阵与早期 CI 验收口径。
  现行默认入口仍是
  [`../../system/minimal_kernel_runtime_evidence_bundle_contract.md`](../../system/minimal_kernel_runtime_evidence_bundle_contract.md)。
- [`minimal_kernel_runtime_evidence_matrix.md`](minimal_kernel_runtime_evidence_matrix.md)：保留早期 runtime seam、
  verifier、handoff 与“不证明”边界的逐项映射。该表不再作为新增 seam 的维护门禁。

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

若以后实现其中任一方向，应从具体源码、失败语义和独立证据重新建立契约，不恢复旧文档中的阶段性叙事。
