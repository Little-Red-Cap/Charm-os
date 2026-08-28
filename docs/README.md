# Charm 文档入口

## 文档状态

- `status`: `supporting`
- `scope`: `docs/` 总路由
- `authority`: [`../CONSTITUTION.md`](../CONSTITUTION.md)

本页只提供权威顺序和最短入口，不定义新的架构语义。

Charm 是一个能力导向的嵌入式应用平台。

## 首读

1. [`CONSTITUTION.md`](../CONSTITUTION.md)：Charm 身份与 Core 准入。
2. [根 README](../README.md)：仓库定位与当前实现入口。
3. [`charm_core_contract.md`](architecture/charm_core_contract.md)：最小 Core 关系与边界。
4. [`only_core_distillation_sop.md`](project/only_core_distillation_sop.md)：OnlyCore 提纯流程和证据要求。

## 状态与权威

| 状态 | 用途 |
|---|---|
| `canonical` | 定义 Charm Core；全仓仅保留极少数入口 |
| `supporting` | 描述局部契约、实现或证据，仅在专题内有效 |
| `exploration` | 保存未冻结方案，不作为实现或 Core 依据 |
| `archived` | 历史追溯，不进入推荐路径 |

涉及 Charm 身份与 Core 语义时，权威顺序为：Constitution、canonical 核心契约、根 README、
专题 supporting 文档、exploration、Git 历史。文件名中的 `contract`、`overview`
或 `roadmap` 不授予权威。

[`AGENTS.md`](../AGENTS.md) 负责操作与协作规则，不替代架构裁决。

## 按任务进入

| 任务 | 当前入口 |
|---|---|
| Core 语义与能力归属 | [`architecture/README.md`](architecture/README.md)、[`architecture route`](agent/routes/architecture.md) |
| 当前源码与证据 | [`overview.md`](overview.md)、[`architecture_overview.md`](architecture_overview.md)、[`Examples/README.md`](../Examples/README.md) |
| OnlyCore 提纯与协作 | [`project/only_core_distillation_sop.md`](project/only_core_distillation_sop.md)、[`project/README.md`](project/README.md) |
| 构建 | [`build route`](agent/routes/build.md) |
| 文档维护 | [`documentation_maintenance.md`](documentation_maintenance.md)、[`docs route`](agent/routes/docs.md) |

Resident image、minimal-kernel、RTE、Spine、IO、UI 和 System Compiler 材料已从 OnlyCore 活动树移除；
不要从 Git 历史中的文档数量、schema 或旧 smoke 名称反推当前实现或 Core 身份。
