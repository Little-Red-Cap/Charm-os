# Minimal Kernel Task Syscall Contract

## 文档状态

- `status`: `supporting`
- `scope`: task syscall 编号、request、dispatch、静态 table 与 frame adapter
- `authority`: 受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
  [`charm_core_contract.md`](../architecture/charm_core_contract.md) 约束

Task syscall 是 minimal-kernel 内部实验接口，不是产品用户态 ABI、POSIX syscall 表或
Charm App API。

## 请求与映射

Task-facing wrapper 只把 typed operation 转成 syscall id 和四个参数，不拥有调度策略或架构
frame。Catalog 负责 syscall id、trap service 与语义投影之间的映射；未知 id 必须保持
invalid/unsupported，不得回退到其它 handler。

当前 syscall id 复用 `TrapService` 数值只是实现事实，不构成永久 wire ABI。具体编号和参数名
以 `kernel.task_syscall_catalog` 源码为准，不在本文复制。

## Static table

Table 持有固定数量的 handler entry，并保持以下分支可区分：

- lookup 返回 entry、slot 和 matched；
- matched 且 handler valid 时调用 handler；
- matched 但 unbound 返回 `TrapError::unbound_adapter`；
- 未匹配返回 `TrapError::unsupported_service`；
- dispatch 保留 handler 返回的 disposition/error/value。

Handler 的 disposition、error 与 value 必须原样进入统一 `TrapResult`，table 不把拒绝或失败
包装成成功。Table 不提供动态注册、权限检查、进程隔离或 ABI 版本协商。

## Frame pipeline

处理顺序固定为：

```text
capture/decode -> TaskSyscallRequest -> table dispatch -> apply_result
```

公共层只看 syscall id 和四个参数。平台 adapter 拥有真实 frame layout，并提供 capture 与
result writeback；frame bridge 不得引入平台寄存器布局。

Adapter 缺失报告 `unbound_adapter`，capture/decode 失败报告 `decode_failed`，result writeback
失败报告 `writeback_failed`。这些错误不得合并或改写为 handler 失败。

## 结果与观测

所有分支使用 [`TrapResult`](minimal_kernel_trap_syscall_contract.md)。Catalog、dispatch、table
和 frame 可以产生局部 trace/witness，但这些观测不构成 syscall ABI 或系统级证据。

## 验证入口

Host 与 QEMU 入口由 [`Examples/kernel/README.md`](../../Examples/kernel/README.md) 路由；
ARMv7-A frame ownership 与证据边界见
[`armv7a_runtime_trap_mapping_contract.md`](armv7a_runtime_trap_mapping_contract.md)。验证通过只证明
对应 fixture，不证明产品用户态隔离或稳定 syscall ABI。

## 非目标

- 不定义真实 SVC register ABI 或 exception return。
- 不定义用户地址检查、权限、copy-in/out 或进程模型。
- 不保证编号长期稳定。
- 不把 task-facing `sys_*` 名称解释为同步系统调用。

旧 catalog、dispatch、API 和 frame 草案的取舍见
[`../archive/minimal-kernel-syscall-v0/README.md`](../archive/minimal-kernel-syscall-v0/README.md)。
