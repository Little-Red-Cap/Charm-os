# Phase 2 Gap 摘要

> **文档状态：`archive summary`**

完整阶段记录见 [`../archive/device-readiness-and-phase2-v0/phase2_gap_map.md`](../archive/device-readiness-and-phase2-v0/phase2_gap_map.md)。当前 SSU 边界见 [`ssu_contract.md`](ssu_contract.md)，文档治理见
[`../documentation_maintenance.md`](../documentation_maintenance.md)。

## 仍可复用的未决项

- 新执行路径仍需明确 owner、context、blocking 和 failure 语义；
- Host/QEMU/real-board 样本不能互相替代；
- 工具链和 C++ Modules 问题必须以可复现构建记录描述；
- app/system/platform 边界需由真实 consumer 验证；
- 旧旁路、临时构建目录和重复文档应持续清理。

## 已失效的用法

`Phase 2` 不再是当前仓库排期或统一进度刻度。旧文档中的“下一步”“主线感”“大手术”属于阶段叙事，不能作为实现优先级或完成证明。
