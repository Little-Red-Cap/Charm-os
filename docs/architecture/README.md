# Charm 架构文档入口

本目录同时包含 canonical 核心契约、supporting 专题契约和 exploration 材料。它们不再因为
位于同一目录而拥有相同权威。

Charm 的正式定位是：

> **Charm 是一个能力导向的嵌入式应用平台。**

## 第一入口

1. [`../../CONSTITUTION.md`](../../CONSTITUTION.md)
2. [`charm_core_contract.md`](charm_core_contract.md)
3. [`../../README.md`](../../README.md)
4. [`../README.md`](../README.md)

Constitution 负责 Core 准入，核心契约负责最小关系和 MVP。其它架构文档只能在这两者给出的
边界内解释局部问题。

## Canonical

- [`charm_core_contract.md`](charm_core_contract.md)

本目录当前只有这一份 canonical 核心契约。新增 canonical 文档或术语前，必须通过 Constitution
的六问审查；已有文件名和历史使用量不构成准入证据。

## Supporting

### Core 治理与语义审计

- [`charm_core_semantic_audit.md`](charm_core_semantic_audit.md)
- [`../../scripts/check_charm_core_governance.ps1`](../../scripts/check_charm_core_governance.ps1)
- [`../../Examples/system/charm_capability_mvp/README.md`](../../Examples/system/charm_capability_mvp/README.md)

审计页把 Constitution 裁决映射到真实源码、CMake 和 smoke；检查脚本只防止 canonical
定位、裁决表、文档状态和链接漂移，不替代行为测试或 Core 准入审判。MVP 示例当前已完成
Host 与真实 QEMU 两域，仍不能作为真实板证据。

### 依赖、入口与局部公共边界

- [`dependency_contract.md`](dependency_contract.md)
- [`entry_surface_contract.md`](entry_surface_contract.md)
- [`stable_entry_aggregate_contract.md`](stable_entry_aggregate_contract.md)
- [`dependency_whitelist.md`](dependency_whitelist.md)
- [`legacy_runtime_facade_retirement_contract.md`](legacy_runtime_facade_retirement_contract.md)

这些文件描述当前代码的依赖和入口约束，不定义 Charm Core 身份。

### Driver、device 与 interface 证据

- [`driver_model.md`](driver_model.md)
- [`interface_admission_policy.md`](interface_admission_policy.md)
- [`i2c_device_contract_v0.md`](i2c_device_contract_v0.md)
- [`spi_device_contract_v0.md`](spi_device_contract_v0.md)
- [`gpio_device_contract_v0.md`](gpio_device_contract_v0.md)
- [`block_device_contract_v0.md`](block_device_contract_v0.md)
- [`stream_io_device_contract_v0.md`](stream_io_device_contract_v0.md)
- [`timebase_device_contract_v0.md`](timebase_device_contract_v0.md)

Driver 和 Backend 已被裁决为 `Implementation / Tool`。这些文档可约束其实现范围，但不能把
Driver、Provider 基类或 device graph 提升为 Core 原语。SPI、GPIO、Block、Stream IO 和 Timebase
目前都是 proposed implementation interfaces；它们的完整早期讨论保留在
[`../archive/device-interface-drafts-v0/`](../archive/device-interface-drafts-v0/)，不作为默认契约入口。

### Signal、state 与能力回收

- [`signal_state_contract_v0.md`](signal_state_contract_v0.md)
- [`capability_recovery_rules.md`](capability_recovery_rules.md)
- [`capability_recovery_matrix.md`](capability_recovery_matrix.md)
- [`real_board_landing_gap_audit_v0.md`](real_board_landing_gap_audit_v0.md)

这些是局部语义和现有实现证据。其名词仍需按 Constitution 逐项审判。Signal / State 的
完整阶段设计稿位于 [`../archive/signal-state-v0/`](../archive/signal-state-v0/README.md)，
不作为默认契约入口。

### Resident image 与部署

- [`resident_image_platform_v1_contract.md`](resident_image_platform_v1_contract.md)

Resident ELF、ModuleX、Image Store 和 Loader 是可选部署机制。它们可以复用同一应用和能力契约，
但不是 Charm MVP 成立条件，也不定义第二套 App model。

## Exploration

以下材料在停线阶段保留摘要并冻结扩面；完整讨论位于对应归档目录：

- [`charm_methodology_charter.md`](charm_methodology_charter.md)
- [`charm_spine_v0.md`](charm_spine_v0.md)
- [`rte_capability_composition_contract_v0.md`](rte_capability_composition_contract_v0.md)
- [`system_compiler_roadmap.md`](system_compiler_roadmap.md)
- [`system_compiler_vocabulary_v0.md`](system_compiler_vocabulary_v0.md)
- [`rte_to_h747_platform_roadmap.md`](rte_to_h747_platform_roadmap.md)

方法论、Spine 和 RTE 的完整正文见
[`../archive/architecture-exploration-v0/README.md`](../archive/architecture-exploration-v0/README.md)。

System Compiler、IR、Graph 和 RTE 当前均不能作为 canonical Core 身份。它们可以作为工具、
派生表示或待审判模型继续提供证据，但不再承担默认路线。

## 按问题进入

| 问题 | 先读 |
|---|---|
| 一个概念能否进入 Core | [`../../CONSTITUTION.md`](../../CONSTITUTION.md) |
| Requirement / Provision / Binding 如何成立 | [`charm_core_contract.md`](charm_core_contract.md) |
| 当前代码与首批裁决有哪些冲突 | [`charm_core_semantic_audit.md`](charm_core_semantic_audit.md) |
| 当前模块入口与依赖是否合法 | [`entry_surface_contract.md`](entry_surface_contract.md)、[`dependency_contract.md`](dependency_contract.md) |
| Driver 或 device 局部实现如何组织 | [`driver_model.md`](driver_model.md) |
| 同域 signal / state 如何表达 | [`signal_state_contract_v0.md`](signal_state_contract_v0.md) |
| Resident ELF / ModuleX 如何进入 AppRuntime | [`resident_image_platform_v1_contract.md`](resident_image_platform_v1_contract.md) |
| 旧 System Compiler / RTE 讨论依据是什么 | 本页 Exploration 列表 |

## 使用规则

- 不从目录规模、类名或现有调用量反推 Core 身份。
- 不把 Interface 当作 Capability Contract，也不把 Provider 角色实体化为公共基类。
- 不把 init DAG、runtime topology、ownership 和 hot-plug state 合成一张权威 Graph。
- 不用 roadmap、v0 或产品压力线覆盖 canonical 裁决。
- 专题文档与 Constitution 或核心契约冲突时，先降级专题结论，再决定是否发起重新审判。
