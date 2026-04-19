# Kernel 示例入口

本目录收纳内核、运行时、QEMU、host runtime 和早期平台验证相关示例。

这些示例默认不承担“全仓总入口”职责。  
如果你在看现行系统契约、最小内核边界或平台装配规则，优先先读：

- [`../../docs/system/README.md`](../../docs/system/README.md)
- [`../../docs/architecture_overview.md`](../../docs/architecture_overview.md)

## 按验证路径进入

### 我想看 ARMv7-A / QEMU bare-metal

先读：

- [`armv7a/qemu/README.md`](armv7a/qemu/README.md)

这是当前 Cortex-A/QEMU 路径最完整、最适合作为入口的一支。

### 我想看 AArch64 / QEMU

当前这支还比较轻，入口主要在：

- `aarch64/qemu/CMakeLists.txt`
- `aarch64/qemu/CMakePresets.json`

适合在你已经知道自己要看什么时直接进入。

### 我想看 POSIX / QEMU

先读：

- [`posix/qemu/README.md`](posix/qemu/README.md)

### 我想看 RTOS v0 路径

先读：

- [`rtos/README.md`](rtos/README.md)
- [`rtos/qemu/README.md`](rtos/qemu/README.md)

### 我想看 host runtime 最小样例

这一簇示例主要按“某个 runtime / syscall / trap 契约的最小宿主验证”组织：

- `runtime_minimal_host`
- `runtime_service_host`
- `runtime_task_api_host`
- `runtime_task_syscall_host`
- `runtime_task_syscall_catalog_host`
- `runtime_task_syscall_dispatch_host`
- `runtime_task_syscall_table_host`
- `runtime_task_syscall_frame_host`
- `runtime_task_syscall_frame_caller_host`
- `runtime_task_syscall_frame_armv7a_host`
- `runtime_thread_port_host`
- `runtime_trap_armv7a_host`
- `runtime_isr_defer_host`

这批目录当前大多没有自己的 README，适合配合下面这些系统文档一起看：

- [`../../docs/system/minimal_kernel_runtime_bridge_contract.md`](../../docs/system/minimal_kernel_runtime_bridge_contract.md)
- [`../../docs/system/minimal_kernel_runtime_service_contract.md`](../../docs/system/minimal_kernel_runtime_service_contract.md)
- [`../../docs/system/minimal_kernel_task_runtime_api_contract.md`](../../docs/system/minimal_kernel_task_runtime_api_contract.md)
- [`../../docs/system/minimal_kernel_task_syscall_api_contract.md`](../../docs/system/minimal_kernel_task_syscall_api_contract.md)
- [`../../docs/system/minimal_kernel_task_syscall_catalog_contract.md`](../../docs/system/minimal_kernel_task_syscall_catalog_contract.md)
- [`../../docs/system/minimal_kernel_task_syscall_dispatch_contract.md`](../../docs/system/minimal_kernel_task_syscall_dispatch_contract.md)
- [`../../docs/system/minimal_kernel_task_syscall_table_contract.md`](../../docs/system/minimal_kernel_task_syscall_table_contract.md)
- [`../../docs/system/minimal_kernel_task_syscall_frame_contract.md`](../../docs/system/minimal_kernel_task_syscall_frame_contract.md)
- [`../../docs/system/minimal_kernel_trap_syscall_contract.md`](../../docs/system/minimal_kernel_trap_syscall_contract.md)

### 我想看 Windows 历史验证路径

目录：

- `windows/`

这条线更偏早期宿主验证和阶段性样例，不是当前 QEMU / ARMv7-A 主入口。

## 当前建议阅读顺序

- ARMv7-A / QEMU：
  `armv7a/qemu/README.md`
- POSIX / QEMU：
  `posix/qemu/README.md`
- RTOS：
  `rtos/README.md`
- host runtime：
  先回 `docs/system/*` 对应契约，再进具体 `runtime_*_host`

## 使用提醒

- 这些目录里存在阶段性验证、历史路径和现行主线并存的情况，不要默认把所有示例都当成当前主路径。
- 如果你需要“现在仓库推荐从哪条内核线进去”，优先还是 `armv7a/qemu` 与 `posix/qemu`。
