# ARMv7-A QEMU bare-metal leaf

## 边界

该 target 在 QEMU `virt` / Cortex-A7 上验证 ARMv7-A bare-metal 机器语义。它不是 Linux image，
也不证明 RK3506 或其它真实 SoC 的 clock、DDR、GIC、cache timing 和外设状态。

Leaf 自己拥有 startup、linker、vector、mode stack、PL011、GIC/generic timer、MMU/cache 操作和
QEMU runner。可复用 frame、handoff 与 trap adapter 语义位于
[`targets/armv7a/common`](../../../../targets/armv7a/common/)。平台责任见
[`armv7a_platform_contract.md`](../../../../docs/system/armv7a_platform_contract.md)。

## 默认运行

在本目录执行：

```powershell
./run_qemu_ci.ps1
```

该 runner 配置并串行构建 `debug` preset，启动 `qemu-system-arm`，再按脚本中的 expected token
验证输出。README 不复制 token；pass/fail 以当次 runner 日志为准。

只启动已有 ELF：

```powershell
./run_qemu.ps1
./run_qemu.ps1 -ElfPath <path-to-elf>
```

`run_qemu.ps1` 使用：

```text
-machine virt -cpu cortex-a7 -nographic -monitor none -device loader,file=<elf>,cpu-num=0
```

使用 `-device loader` 是因为产物是 bare-metal ELF，不是 Linux kernel image。

## 定向 runner

### Runtime / syscall

- `run_qemu_phase_ledger_ci.ps1`
- `run_qemu_runtime_live_ci.ps1`
- `run_qemu_runtime_leaf_ports_ci.ps1`
- `run_qemu_runtime_thread_ci.ps1`
- `run_qemu_runtime_binding_bundle_ci.ps1`
- `run_qemu_runtime_leaf_bundle_ci.ps1`
- `run_qemu_runtime_package_ci.ps1`
- `run_qemu_runtime_trap_ci.ps1`
- `run_qemu_task_syscall_ci.ps1`
- `run_qemu_arch_ingress_seam_ci.ps1`
- `run_qemu_lower_half_ci.ps1`

Trap 与 syscall 映射边界见
[`armv7a_runtime_trap_mapping_contract.md`](../../../../docs/system/armv7a_runtime_trap_mapping_contract.md)。

### Exception / interrupt

- `run_qemu_abort_ci.ps1 -Kind <kind>`
- `run_qemu_exception_ci.ps1 -Kind undefined`
- `run_qemu_interrupt_special_ci.ps1`
- `run_qemu_interrupt_sgi_timeout_ci.ps1`
- `run_qemu_interrupt_unexpected_ci.ps1`
- `run_qemu_interrupt_sgi_fiq_timeout_ci.ps1`

支持的 abort/interrupt kind 以 `CMakeLists.txt` cache enum 和 `CMakePresets.json` 为准，不在本文
维护第二份列表。

### Handoff

- `run_qemu_handoff_ci.ps1`
- `run_qemu_handoff_live_ci.ps1`

Handoff runner 验证 prepare/entry/transfer/launch 的 QEMU leaf 行为，不定义产品 image、slot、签名
或真实板跳转策略。

## GDB

```powershell
./run_qemu.ps1 -WaitForGdb -GdbPort 1234
```

然后由 ARM GDB 连接 `target remote :1234` 并加载相同 ELF 的 symbols。`-WaitForGdb` 使用 QEMU
`-S -gdb tcp::<port>`，CPU 在 reset 后暂停。

## 代码区域

| 区域 | 职责 |
|---|---|
| `startup.S` / `vectors.S` / `linker.ld` | reset、mode stacks、vector 与内存布局 |
| `armv7a_cpu/mmu/cache/page_table*` | CP15、translation、cache/TLB probe |
| `armv7a_gic/arch_timer/interrupt*` | GIC、timer、IRQ/FIQ 与 edge cases |
| `armv7a_runtime_*` | current/trap/thread/loop/live/bundle/package glue |
| `armv7a_task_syscall_*` | task-side SVC frame、dispatch、roundtrip 与 failure |
| `armv7a_handoff_*` | prepare、entry、transfer、launch 与 live smoke |

这些文件是 QEMU leaf glue，不应被搬入跨架构 runtime module。公共语义只有在
`targets/armv7a/common` 或 `docs/system` 契约中明确时才可复用。

## 证据规则

- Host verifier 证明字段映射，不能证明真实 exception entry。
- QEMU runner 证明可仿真的 CPU/firmware seam，不能证明真实 SoC。
- preset、script 或 token 存在不等于当前绿色；必须保留当次命令、退出码和日志。
- 专项 runner 的结果不能代替默认 runtime evidence bundle。
