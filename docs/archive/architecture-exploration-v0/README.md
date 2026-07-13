# Architecture Exploration v0 归档

本目录保留 RTE 和外部项目机制案例中仍有独立价值的设计讨论。方法论宣言、Spine 全文与
RTE-to-H747 五阶段 roadmap 已删除；当前没有足够源码与跨环境证据支撑剩余模型成为 Charm Core
或统一运行时。

默认入口是：

- [`../../architecture/README.md`](../../architecture/README.md)
- [`../../../CONSTITUTION.md`](../../../CONSTITUTION.md)
- [`../../architecture/charm_core_contract.md`](../../architecture/charm_core_contract.md)

归档正文只用于追溯。任何恢复工作都必须重新核对当前代码、CMake、消费方和可重复 smoke，不以历史文档数量作为证据。

保留文件：

- [`rte_capability_composition_contract_v0.md`](rte_capability_composition_contract_v0.md)
- [`tdesktop_mechanism_retained_notes.md`](tdesktop_mechanism_retained_notes.md)：保留 lifetime、execution
  domain、style/schema/platform/storage 与注释边界的外部机制比较。

已删除正文中仍有效的判断只有：平台承载系统而不定义系统；系统秩序应独立于平台细节；边界应
显式、可验证并尽可能机器可读。它们现在由 Constitution、核心契约和现行 architecture rules 约束，
不再保留“术/法/道”、五阶段排期或统一 Spine 链条。

Telegram Desktop 案例中的 lifetime、execution domain、style/schema/storage law 仍可作为设计反例和
候选机制来源，但其中的 P0-P3 排期及 Charm 机制映射从未成为现行 roadmap。

Telegram Desktop 比较全文已压缩；上游仓库导览、伪 API、Player/Vivid 推演、superproject 建议和
优先级路线不再保留。

旧 Display + Player 压力切片、五阶段排期和 host smoke inventory 可从 Git 历史追溯；这些内容
不代表当前 H747 target、Player 结构或 Core 准入状态。
