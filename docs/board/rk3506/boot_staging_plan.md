# RK3506 启动阶段边界

> status: exploration

本文只界定 RK3506 物理启动阶段与 Charm 公开运行模型的边界。完整的 SDK
线索、历史方案和阶段排期已移入
[`../../archive/architecture-inventory-v0/rk3506_boot_staging_plan.md`](../../archive/architecture-inventory-v0/rk3506_boot_staging_plan.md)。

## 阶段模型

RK3506 的实际启动链可能包含：

```text
BootROM / boot media ingress
    -> Stage A: SRAM early stage
    -> Stage B: DDR init / relocation stage
    -> Stage C: post-DDR normal image
```

- **Stage A**：在片内 SRAM/IRAM 中建立最小栈、介质入口和调试入口，并进入
  DDR 初始化或下一阶段 loader。
- **Stage B**：完成 DDR 初始化、最小内存验证和镜像搬运，建立较完整的栈、
  BSS 与运行地址。
- **Stage C**：在 DDR 已可用的前提下承载向量、异常、GIC、generic timer、
  MMU/cache/TLB 和正常裸机 runtime。

这些是物理阶段，不意味着 Charm 必须公开一套通用 `stage1/stage2/stage3`
框架。

## 当前仓库边界

当前 RK3506 裸机 target 应按 **Stage C** 理解。它可以复用 QEMU ARMv7-A
路径验证过的异常、向量、中断和内存管理语义，但必须由 RK3506 叶子映射真实
地址、时钟、复位和外设。

Stage A/B 若进入仓库，应留在 RK3506 私有 target 内：

- 不让 vendor DDR binary、BootROM 细节或 SRAM 地址污染公共 boot 模型；
- 不让早期阶段的内存限制反向塑形普通 runtime；
- 不把高层镜像策略、slot、verify 或 rollback 塞进 DDR bring-up；
- 不把 QEMU 的 Stage C 语义证据误写成真实板早期启动证据。

前级跳入 Stage C 前必须满足的条件由
[`post_ddr_handoff_contract.md`](post_ddr_handoff_contract.md) 约束。

## 当前依据

现有 SDK 显示 Rockchip 主路径包含独立 DDR 数据和 SPL/loader：

- `RK3506MINIALL.ini` 分别声明 `FlashData` 与 `FlashBoot`；
- U-Boot 配置给出了独立的 SPL text、stack、BSS 与 size 边界；
- 开源 TPL 路径没有提供可直接替代 vendor DDR 初始化的完整实现。

这些信息足以支持“物理上存在早期阶段”的判断，但不足以证明 Charm 已经实现
Stage A/B，也不足以把当前 SDK 选择提升为 SoC 永久契约。

## 未确认事实

以下内容仍需 TRM、原理图、vendor 资料或实板证据：

- 片内 SRAM/OCRAM 的准确容量和可用布局；
- DDRC/DDRPHY 的寄存器、训练流程和最小初始化边界；
- BootROM 对各启动介质、下载模式和镜像格式的精确行为；
- secure/OTP/reset gating 等必须保留的前置状态；
- SMP/PSCI 由哪个阶段或上游固件提供；
- 替换 vendor loader 时，自研 Stage A/B 的最小可行范围。

在这些事实确认前，当前工作应继续把 Stage C 与 post-DDR handoff 打稳，不猜测
补全早期启动实现。
