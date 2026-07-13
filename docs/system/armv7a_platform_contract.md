# ARMv7-A 平台契约

> status: `supporting`
>
> 本文约束 ARMv7-A 公共机器语义与 leaf target 的所有权，不定义 Boot policy、镜像格式或板级常量。

## 责任边界

公共 ARMv7-A 层可以提供可复用的异常、寄存器、barrier、MMU/cache/TLB 和 trap frame 语义；
它不能知道具体 SoC 的 MMIO、clock、pinmux、reset 或启动介质。

代码所有权保持三层：leaf target 持有 QEMU/SoC/board 私有常量和启动细节，
`targets/armv7a/common` 只持有可复用机器语义，`Modules/system/kernel` 持有平台无关策略。

Leaf target 负责：

- reset 入口、CPU mode、栈和异常向量所有权；
- IRQ/FIQ 屏蔽以及 GIC 或其它中断控制器接线；
- early console、generic timer 和板级 MMIO；
- MMU、cache、TLB、branch predictor 与 barrier 的机器状态切换；
- 跳入下一阶段或 runtime 前的最终机器状态。

## Handoff 状态

每个 leaf 必须明确入口与出口状态，至少包括：

- CPU、world、privilege mode、endianness 和当前核心；
- 可读写执行内存、镜像地址、栈与 BSS；
- MMU/cache/TLB/branch prediction 状态；
- VBAR/向量、IRQ/FIQ、GIC active state 和 timer/watchdog 状态；
- cacheable 路径搬运镜像后的 clean/invalidate 与 barrier 责任。

前级状态不满足 leaf 契约时必须在平台边界修正或拒绝，不能把不确定状态泄漏给公共 runtime。

## 不属于本契约

- image header、签名、slot、rollback 和 pending/active policy；
- UART/USB 下载协议、Flash 分区和文件系统；
- BootROM、DDR training、secure boot、PSCI 和 secondary-core policy；
- 某块板的 UART/GIC/GRF/CRU 地址或初始化顺序。

这些内容由 boot、board、loader 或产品契约分别负责。

## 证据域

QEMU 用于验证可仿真的异常、timer、IRQ、trap 和 MMU/cache 操作顺序；它不证明真实 SoC 的
clock、总线、内存时序或外设状态。真实板必须单独提供对应运行证据。

当前入口：

- QEMU trap 映射：[`armv7a_runtime_trap_mapping_contract.md`](armv7a_runtime_trap_mapping_contract.md)
- RK3506 handoff：[`../board/rk3506/post_ddr_handoff_contract.md`](../board/rk3506/post_ddr_handoff_contract.md)
- QEMU minimal-kernel 证据：
  [`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md)
- 历史 staging 取舍：
  [`../archive/system-evidence-and-staging-v0/armv7a_staging_retained_notes.md`](../archive/system-evidence-and-staging-v0/armv7a_staging_retained_notes.md)
