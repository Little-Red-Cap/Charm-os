# Charm 架构文档入口

## 文档状态

- `status`: `supporting`
- `scope`: architecture 文档路由
- `authority`: [`../../CONSTITUTION.md`](../../CONSTITUTION.md)

目录位置不授予文档相同权威。先读 Constitution，再读唯一 canonical 核心契约；其它文件只在其
声明的局部范围内有效。

Charm 的正式定位是：

> **Charm 是一个能力导向的嵌入式应用平台。**

## 第一入口

1. [`../../CONSTITUTION.md`](../../CONSTITUTION.md)
2. [`charm_core_contract.md`](charm_core_contract.md)
3. [`../../README.md`](../../README.md)
4. [`../README.md`](../README.md)

## Canonical

- [`charm_core_contract.md`](charm_core_contract.md)

本目录当前只有这一份 canonical 核心契约。Core 准入必须遵守 Constitution 六问。

## Supporting

| 主题 | 入口 |
|---|---|
| Core 冲突审计 | [`charm_core_semantic_audit.md`](charm_core_semantic_audit.md) |
| 依赖与入口 | [`dependency_contract.md`](dependency_contract.md)、[`entry_surface_contract.md`](entry_surface_contract.md)、[`dependency_whitelist.md`](dependency_whitelist.md) |
| Driver/device/interface | [`driver_model.md`](driver_model.md)、[`interface_admission_policy.md`](interface_admission_policy.md) |
| device 局部接口 | [`i2c_device_contract_v0.md`](i2c_device_contract_v0.md)、[`spi_device_contract_v0.md`](spi_device_contract_v0.md)、[`gpio_device_contract_v0.md`](gpio_device_contract_v0.md)、[`block_device_contract_v0.md`](block_device_contract_v0.md)、[`stream_io_device_contract_v0.md`](stream_io_device_contract_v0.md)、[`timebase_device_contract_v0.md`](timebase_device_contract_v0.md) |
| signal/state 与真实板缺口 | [`signal_state_contract_v0.md`](signal_state_contract_v0.md)、[`real_board_landing_gap_audit_v0.md`](real_board_landing_gap_audit_v0.md) |
| resident image/deployment | [`resident_image_platform_v1_contract.md`](resident_image_platform_v1_contract.md) |

device 早期讨论见 [`../archive/device-interface-drafts-v0/`](../archive/device-interface-drafts-v0/)，
Signal/State 与能力回收历史见对应 archive；归档材料不覆盖现行契约。

## Exploration

- [`charm_methodology_charter.md`](charm_methodology_charter.md)
- [`charm_spine_v0.md`](charm_spine_v0.md)
- [`rte_capability_composition_contract_v0.md`](rte_capability_composition_contract_v0.md)
- [`system_compiler_roadmap.md`](system_compiler_roadmap.md)
- [`system_compiler_vocabulary_v0.md`](system_compiler_vocabulary_v0.md)
- [`rte_to_h747_platform_roadmap.md`](rte_to_h747_platform_roadmap.md)

完整历史讨论见 [`../archive/architecture-exploration-v0/README.md`](../archive/architecture-exploration-v0/README.md)。
System Compiler、IR、Graph 和 RTE 不因出现在本列表而成为 Core 身份。

## 按问题进入

| 问题 | 先读 |
|---|---|
| 概念能否进入 Core | [`../../CONSTITUTION.md`](../../CONSTITUTION.md) |
| Requirement / Provision / Binding | [`charm_core_contract.md`](charm_core_contract.md) |
| 源码与裁决的冲突 | [`charm_core_semantic_audit.md`](charm_core_semantic_audit.md) |
| 模块入口与依赖 | [`entry_surface_contract.md`](entry_surface_contract.md)、[`dependency_contract.md`](dependency_contract.md) |
| Driver/device 组织 | [`driver_model.md`](driver_model.md) |
| signal/state | [`signal_state_contract_v0.md`](signal_state_contract_v0.md) |
| Resident ELF/ModuleX | [`resident_image_platform_v1_contract.md`](resident_image_platform_v1_contract.md) |

## 使用规则

- 不从目录、类名、调用量或文档数量反推 Core 身份。
- 不把 Interface、Provider、init DAG、runtime topology 或 hot-plug state 直接升级为 Core 语义。
- 专题文档与 Constitution/核心契约冲突时，先以高权威文档为准并降级专题结论。
