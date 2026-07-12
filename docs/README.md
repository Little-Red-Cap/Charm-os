# Charm 文档路由

本页只负责区分文档权威和提供最短入口，不再用“当前战线”定义 Charm 的身份。

Charm 的正式定位始终是：

> **Charm 是一个能力导向的嵌入式应用平台。**

## 十分钟阅读路径

1. [`../CONSTITUTION.md`](../CONSTITUTION.md)：Core 准入六问、裁决等级和首批判例。
2. [`../README.md`](../README.md)：Charm 是什么、与普通嵌入式框架的差异、当前证明到哪里。
3. [`architecture/charm_core_contract.md`](architecture/charm_core_contract.md)：最小关系、MVP、OS 和 Project 边界。

读完这三份文档，应能回答：

- Charm 是什么，以及它不是什么；
- 应用为何只依赖 Capability Contract；
- Requirement、Provision 和 Binding 如何构成最小模型；
- 当前技术积累为何还不等于 MVP 已成立。

## 文档状态

| 状态 | 作用 | 权威范围 |
|---|---|---|
| `canonical` | 定义 Charm Core 身份、准入和最小契约 | 全仓；只能有极少数入口 |
| `supporting` | 描述局部实现、现状、证据或已稳定的专题边界 | 仅在自身专题内，且不得与 canonical 冲突 |
| `exploration` | 保存提案、路线、v0 词汇和待审判模型 | 不得作为公共 Core 语义依据 |
| `archive` | 保存历史背景和已退出路线 | 仅供追溯 |

文件名包含 `contract`、`overview` 或 `roadmap` 不自动决定状态。文档顶部状态声明和上位文档
优先；没有状态声明的旧文档默认按 `supporting` 或 `exploration` 使用，不得视为 canonical。

## 权威顺序

涉及 Charm 身份与 Core 语义时：

1. [`../CONSTITUTION.md`](../CONSTITUTION.md)
2. [`architecture/charm_core_contract.md`](architecture/charm_core_contract.md)
3. 根 [`README.md`](../README.md) 与本页
4. 标为 `supporting` 的专题契约，仅在各自范围内有效
5. 标为 `exploration` 的 plan、roadmap、draft、review、v0
6. `reference/*`、`generated/*`、`archive/*`

[`../AGENTS.md`](../AGENTS.md) 负责 Agent 操作规则；它不替代 Constitution 的架构裁决。

## 按任务进入

### 核心语义与能力归属

- [`architecture/charm_core_contract.md`](architecture/charm_core_contract.md)
- [`agent/routes/architecture.md`](agent/routes/architecture.md)
- [`agent/routes/capability.md`](agent/routes/capability.md)

任何新名词先经过 Constitution，不先从既有代码反推为 Core。

### 当前代码与机制盘点

以下材料属于 supporting inventory，不定义 Charm 身份：

- [`overview.md`](overview.md)
- [`architecture_overview.md`](architecture_overview.md)
- [`capability_map.md`](capability_map.md)
- [`architecture/README.md`](architecture/README.md)
- [`system/init_graph_contract.md`](system/init_graph_contract.md)
- [`io/README.md`](io/README.md)
- [`storage/README.md`](storage/README.md)

### 构建与工程协作

- [`project/README.md`](project/README.md)
- [`agent/routes/build.md`](agent/routes/build.md)
- [`documentation_maintenance.md`](documentation_maintenance.md)
- [`agent/routes/README.md`](agent/routes/README.md)

### OS、runtime 与部署机制

这些是 supporting 实现边界或专题探索，不是 Charm Core 身份：

- [`system/README.md`](system/README.md)
- [`architecture/resident_image_platform_v1_contract.md`](architecture/resident_image_platform_v1_contract.md)
- [`system/minimal_kernel_runtime_evidence_bundle_contract.md`](system/minimal_kernel_runtime_evidence_bundle_contract.md)
- [`system/posix_support_overview.md`](system/posix_support_overview.md)

### 停线前的仓库状态

- [`repo_governance.md`](repo_governance.md)
- [`current_tracks_index.md`](current_tracks_index.md)

这两页保留停线前的多战线状态和入口，当前只作为 supporting snapshot，不再承担仓库身份或
默认路线裁决。

### 方法论与未来机制探索

- [`architecture/charm_methodology_charter.md`](architecture/charm_methodology_charter.md)
- [`architecture/charm_spine_v0.md`](architecture/charm_spine_v0.md)
- [`architecture/rte_capability_composition_contract_v0.md`](architecture/rte_capability_composition_contract_v0.md)
- [`architecture/system_compiler_roadmap.md`](architecture/system_compiler_roadmap.md)
- [`architecture/rte_to_h747_platform_roadmap.md`](architecture/rte_to_h747_platform_roadmap.md)

这些材料在停线期冻结为 `exploration`。它们可以提供判例和证据，不能新增 canonical 词汇或
把某条机制升级为默认路线。

## 维护规则

- canonical 文档只保留定位、准入、最小契约和已裁决术语。
- 路线图、产品愿景、实现清单和工程步骤不得塞入 Constitution。
- supporting 文档发生行为变化时仍应同步更新，但不得借此扩大 Core。
- exploration 文档可以保留完整正文，必须明确状态和上位入口。
- 坏链接、循环定义和相互冲突的定位不得留在默认阅读路径。
