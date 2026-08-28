# Charm 架构文档入口

## 文档状态

- `status`: `supporting`
- `scope`: architecture 文档路由
- `authority`: [`../../CONSTITUTION.md`](../../CONSTITUTION.md)

目录位置不授予文档相同权威。先读 Constitution，再读唯一 canonical 核心契约；其它文件只在其
声明的局部范围内有效。

Charm 是一个能力导向的嵌入式应用平台。

## Canonical

- [`charm_core_contract.md`](charm_core_contract.md)

本目录当前只有这一份 canonical 核心契约。Core 准入必须遵守 Constitution 六问。

## Supporting

| 主题 | 入口 |
|---|---|
| Core 冲突审计 | [`charm_core_semantic_audit.md`](charm_core_semantic_audit.md) |
| Capability relation v1 准入 | [`charm_capability_relations_v1_admission.md`](charm_capability_relations_v1_admission.md) |
| OnlyCore 当前源码 | [`../../Modules/core/capability/relations.hpp`](../../Modules/core/capability/relations.hpp) |
| OnlyCore Host 证据 | [`../../Examples/system/charm_capability_relations/README.md`](../../Examples/system/charm_capability_relations/README.md) |

其它 device、IO、runtime、板级和 deployment 材料已从 OnlyCore 活动文档区移除，只从快照分支或
Git 历史追溯。RTE/Spine 源码原型已经退役；System Compiler、
IR、Graph 和 RTE 不因出现在历史材料中而成为 Core 身份。

## 使用规则

- 不从目录、类名、调用量或文档数量反推 Core 身份。
- 不把 Interface、Provider、init DAG、runtime topology 或 hot-plug state 直接升级为 Core 语义。
- 专题文档与 Constitution/核心契约冲突时，先以高权威文档为准并降级专题结论。
