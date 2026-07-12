# ARMv7-A Minimal Kernel Staging 摘要

> **文档状态：`exploration`（历史路线，已被实现推进超越）**

完整分阶段计划见
[`../archive/system-evidence-and-staging-v0/armv7a_minimal_kernel_staging_plan.md`](../archive/system-evidence-and-staging-v0/armv7a_minimal_kernel_staging_plan.md)。当前入口见
[`armv7a_platform_contract.md`](armv7a_platform_contract.md) 和
[`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md)。

## 仍有效的边界

- leaf target 持有 QEMU/SoC/board 私有常量与启动细节；
- `targets/armv7a/common` 只保留可复用 ARMv7-A 机器语义；
- `Modules/system/kernel` 持有平台无关策略；
- Host、QEMU 和真实板证据必须分别记录。

## 当前代码事实

QEMU target 已包含 exception/IRQ/timer、thread/context switch、scheduler dispatch、runtime trap、task syscall 和 runtime handoff 等实现与脚本。旧文档中的“下一阶段”顺序不能再作为当前排期。

## 使用限制

本摘要不宣称 minimal-kernel 全链已绿色。具体可运行范围和已知工具链问题以相关 contract、CMake target 和当次 smoke 结果为准；不要从历史计划推断真实板、隔离、SMP、完整用户态或 POSIX 已完成。
