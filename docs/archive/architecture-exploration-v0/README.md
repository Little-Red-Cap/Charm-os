# Architecture Exploration v0 归档

本目录保留方法论、Spine、RTE 和外部项目机制案例讨论。它们有术语来源、设计取舍和未决问题，不能直接删除；但当前没有足够源码与跨环境证据支撑其作为 Charm Core 或统一运行时模型。

默认入口是：

- [`../../architecture/README.md`](../../architecture/README.md)
- [`../../../CONSTITUTION.md`](../../../CONSTITUTION.md)
- [`../../architecture/charm_core_contract.md`](../../architecture/charm_core_contract.md)

归档正文只用于追溯。任何恢复工作都必须重新核对当前代码、CMake、消费方和可重复 smoke，不以历史文档数量作为证据。

保留文件：

- [`charm_methodology_charter.md`](charm_methodology_charter.md)
- [`charm_spine_v0.md`](charm_spine_v0.md)
- [`rte_capability_composition_contract_v0.md`](rte_capability_composition_contract_v0.md)
- [`rte_to_h747_platform_roadmap.md`](rte_to_h747_platform_roadmap.md)
- [`tdesktop_mechanism_lessons_for_charm.md`](tdesktop_mechanism_lessons_for_charm.md)

Telegram Desktop 案例中的 lifetime、execution domain、style/schema/storage law 仍可作为设计反例和
候选机制来源，但其中的 P0-P3 排期及 Charm 机制映射从未成为现行 roadmap。

RTE/H747 路线归档保留旧 Display + Player 压力切片、五阶段排期和 host smoke
inventory；这些内容不代表当前 H747 target、Player 结构或 Core 准入状态。
