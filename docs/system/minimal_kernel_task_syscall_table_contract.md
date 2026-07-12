# Minimal Kernel Task Syscall Contract

## 文档状态

- `status`: `supporting`
- `scope`: task syscall 编号、request、dispatch、静态 table 与 frame adapter
- `authority`: 受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
  [`charm_core_contract.md`](../architecture/charm_core_contract.md) 约束

Task syscall 是 minimal-kernel 内部实验接口，不是产品用户态 ABI、POSIX syscall 表或
Charm App API。

## 模块边界

| 模块 | 责任 |
|---|---|
| `kernel.task_syscall_api` | `sys_*` 的 task-facing 薄包装 |
| `kernel.task_syscall_catalog` | syscall id 与 trap service 的静态映射 |
| `kernel.task_syscall_dispatch` | `TaskSyscallRequest` 到一个 dispatch surface |
| `kernel.task_syscall_table` | syscall id 到固定 handler entry 的 lookup/dispatch |
| `kernel.task_syscall_frame` | frame capture、decode、table dispatch 与 result writeback |

这些层可以在代码中分开维护，但共享本契约，不再为每个薄 facade 建立独立文档。

## 当前编号

| `TaskSyscallId` | 数值 | 对应 `TrapService` | 参数 |
|---|---:|---|---|
| `invalid` | 0 | `invalid` | 无 |
| `yield` | 1 | `yield_current` | 无 |
| `sleep_until` | 2 | `sleep_until` | `arg0=due` |
| `debug_write` | 3 | `debug_write` | `arg0=value` |
| `capability_call` | 4 | `capability_call` | `arg0=id, arg1=operation, arg2=payload` |

编号当前直接复用 `TrapService` 数值。这是实现事实，不承诺永久 wire ABI。

## Request 与 catalog

`TaskSyscallRequest` 固定包含：

```text
syscall / arg0 / arg1 / arg2 / arg3
```

Catalog 描述 name、trap service、view kind、参数名称、结果名称和 supported 标志。
未知 id 映射为 invalid/unsupported，不得回退到其它 handler。

## Static table

`TaskSyscallTable` 持有固定数量的 `TaskSyscallHandlerEntry`：

- lookup 返回 entry、slot 和 matched；
- matched 且 handler valid 时调用 handler；
- matched 但 unbound 返回 `TrapError::unbound_adapter`；
- 未匹配返回 `TrapError::unsupported_service`；
- dispatch 保留 handler 返回的 disposition/error/value。

Table 不提供动态注册、权限检查、进程隔离或 ABI 版本协商。

## Frame pipeline

`kernel.task_syscall_frame` 提供架构无关的五字段视图：

```text
syscall / arg0 / arg1 / arg2 / arg3
```

处理顺序固定为：

```text
capture/decode -> TaskSyscallRequest -> table dispatch -> apply_result
```

`TaskSyscallFrameAdapter<Frame>` 由平台提供 `capture` 和 `apply_result`。Adapter 缺失、decode
失败和 writeback 失败必须分别报告；frame bridge 不拥有真实架构 frame layout。

## 结果与观测

所有分支使用 [`TrapResult`](minimal_kernel_trap_syscall_contract.md)：

```text
disposition / error / value
```

Catalog、dispatch、table 和 frame 各自提供 trace/witness 类型。Witness 是局部测试结果，
不构成 syscall ABI 或系统级证据。

## 证据

- `Examples/kernel/runtime_task_syscall_host`
- `Examples/kernel/runtime_task_syscall_catalog_host`
- `Examples/kernel/runtime_task_syscall_dispatch_host`
- `Examples/kernel/runtime_task_syscall_table_host`
- `Examples/kernel/runtime_task_syscall_frame_host`
- `Examples/kernel/runtime_task_syscall_frame_caller_host`
- `Examples/kernel/runtime_task_syscall_frame_armv7a_host`

ARMv7-A 映射边界见
[`armv7a_runtime_trap_mapping_contract.md`](armv7a_runtime_trap_mapping_contract.md)。

## 非目标

- 不定义真实 SVC register ABI 或 exception return。
- 不定义用户地址检查、权限、copy-in/out 或进程模型。
- 不保证编号长期稳定。
- 不把 task-facing `sys_*` 名称解释为同步系统调用。

旧 catalog、dispatch、API 和 frame 草案的取舍见
[`../archive/minimal-kernel-syscall-v0/README.md`](../archive/minimal-kernel-syscall-v0/README.md)。
