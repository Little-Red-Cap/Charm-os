# Bring-up Evidence Pipeline v0

> **文档状态：`supporting`（报告原型）**

本文记录现有 bring-up evidence 报告边界。完整阶段讨论见
[`../archive/system-evidence-and-staging-v0/bringup_evidence_pipeline_v0.md`](../archive/system-evidence-and-staging-v0/bringup_evidence_pipeline_v0.md)。

## 当前实现

`export_system_compiler_artifact_report.ps1` 和相关 inspect/compare 脚本能报告：

- `declared`；
- `materialized`；
- `published`；
- `observed`；
- `blocked`；
- `failed`。

materialized graph 示例和历史 I2C fixture 可作为输入，但 producer 名称、fact 名称或 `board_real` 标签不等于真实板证据。

## 语义边界

- `published` 只表示入口已发布，不表示设备可用；
- `observed` 只表示观察路径产生结果，不表示结果正确；
- `failed/blocked` 必须保留原因和 producer；
- Host fixture、QEMU 和 real board 必须明确区分；
- evidence report 是只读投影，不参与 init、binding 或运行时判决。

## 未证明

该原型没有证明自动 bring-up、硬件健康、统一 System Compiler 或产品级证据数据库。真实板结论必须附带 board、固件、命令、原始输出和失败路径。
