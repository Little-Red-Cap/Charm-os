# ARMv7-A QEMU Bare-metal Leaf

## 文档状态

- `status`: `supporting`
- `scope`: QEMU `virt` / Cortex-A7 的 ARMv7-A bare-metal machine evidence
- `source`: `CMakeLists.txt`、`CMakePresets.json` 与本目录 runner

该 leaf 不是 Linux image，也不证明 RK3506 或其它 SoC 的 clock、DDR、GIC、cache timing 与外设。
startup、linker、vector、mode stack、PL011、GIC/timer、MMU/cache 和 runner 均由本目录拥有；可复用
frame/handoff/trap adapter 位于 [`targets/armv7a/common`](../../../../targets/armv7a/common/)。平台责任见
[`armv7a_platform_contract.md`](../../../../docs/system/armv7a_platform_contract.md)。

## 入口

| 任务 | 入口 |
|---|---|
| configure、串行 build、QEMU 与 token gate | [`run_qemu_ci.ps1`](run_qemu_ci.ps1) |
| 运行已有 ELF | [`run_qemu.ps1`](run_qemu.ps1) |
| GDB wait | `run_qemu.ps1 -WaitForGdb -GdbPort <port>` |

`run_qemu_ci.ps1` 复用 `debug` preset 和 `out/build/debug`。`run_qemu.ps1` 使用 `-device loader` 装载
bare-metal ELF；具体 QEMU 参数、默认 ELF、timeout 和 token 只由脚本维护。

## 定向证据

| 范围 | Runner family | 上位边界 |
|---|---|---|
| runtime / syscall / lower-half | `run_qemu_runtime_*`、`run_qemu_task_syscall_ci.ps1`、`run_qemu_lower_half_ci.ps1` | [`trap mapping`](../../../../docs/system/armv7a_runtime_trap_mapping_contract.md) |
| abort / exception / interrupt | `run_qemu_abort_*`、`run_qemu_exception_*`、`run_qemu_interrupt_*` | `CMakeLists.txt` cache enum 与 preset |
| handoff | `run_qemu_handoff_*` | prepare/entry/transfer/launch leaf semantics |

Handoff evidence 不定义产品 image、slot、signature 或真实板 jump policy。专项 runner 不能代替默认
runtime evidence bundle。

## 代码 ownership

| 区域 | 职责 |
|---|---|
| `startup.S` / `vectors.S` / `linker.ld` | reset、mode stacks、vector 与内存布局 |
| `armv7a_cpu/mmu/cache/page_table*` | CP15、translation、cache/TLB probe |
| `armv7a_gic/arch_timer/interrupt*` | GIC、timer、IRQ/FIQ 与 edge cases |
| `armv7a_runtime_*` / `armv7a_task_syscall_*` | runtime glue、SVC frame、dispatch 与 failure |
| `armv7a_handoff_*` | prepare、entry、transfer、launch |

这些文件是 QEMU leaf glue，不进入跨架构 runtime module。只有
`targets/armv7a/common` 或 `docs/system` 明确的语义可以跨 target 复用。

## 证据边界

- Host verifier 只证明字段映射，不证明 exception entry。
- QEMU 只证明可仿真的 CPU/firmware seam，不证明真实 SoC。
- preset、script 或 token 存在不等于当前绿色；以当次退出码和日志为准。
