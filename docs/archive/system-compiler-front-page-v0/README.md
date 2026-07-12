# System Compiler Front Page v0 归档

> **状态：`archived`**

本目录记录早期 system compiler front-page 探索，不是当前架构或实现入口。

## 保留内容

- `opening_judgment_corridor_v0.md`
  仍被 system compiler 词汇材料引用，保留为历史定义来源。
- [`minimal_kernel_runtime_session_witness_inspect_compare_consumer_v0.md`](minimal_kernel_runtime_session_witness_inspect_compare_consumer_v0.md)
  保留 runtime session compare focus 排序、preferred/fallback explain hop 与 reader-first 消费策略的原始讨论；
  现行 supporting contract 仍位于
  [`../../system/minimal_kernel_runtime_session_witness_inspect_compare_consumer_v0.md`](../../system/minimal_kernel_runtime_session_witness_inspect_compare_consumer_v0.md)。

其余按对话步骤拆分的 opening-flow、biography、world、witness 和 compare 微文档已从当前树删除。它们的过程信息可从 Git 历史追溯，不再占用当前文档面。

## 保留的讨论结论

| 历史对象 | 有价值的问题 | 当前裁决 |
|---|---|---|
| `canonical_world` | 一次交付要证明哪些契约、问题和 witness | 可作为证据编排思路，不是 Core 世界模型 |
| `witness_bundle` | 如何汇总证据、状态、缺失点和来源 | 可作为 evidence 工具的局部边界 |
| `world_compare` | baseline 与 candidate 从哪条 witness 开始漂移，影响哪一层 | compare 与最小失败面值得保留 |
| `biography/index` | 如何生成可浏览的机器摘要和目录 | 仅是展示/索引工具，不增加领域语义 |
| `front_page_route/open_event` | 工具应打开什么、为什么打开、依据来自哪里 | 路由来源和可审计解释有价值，不应继续拆分更多 schema 层级 |

### Input language v0

原 `docs/system/system_input_language_v0.md` 已删除。它前半重复词汇表与 artifact report，
后半把未实施方案混入完成清单。仍有价值的内容如下：

- `SystemSpec`、`Profile`、`BoardPackage`、`Binding`、`Facet` 是 system compiler
  exploration 中曾使用的输入视角，当前定义统一回到
  [`system_compiler_vocabulary_v0.md`](../../architecture/system_compiler_vocabulary_v0.md)；
- exporter 当前实际支持 `materialized_graph`、`runtime_only`、`fact_only` 三种 `case_kind`；
- 从 SSU 元数据推导资源契约、Profile 隐式契约、跨 case graph path 聚合均是未实施提案；
- SSU `Meta` 目前只是描述性元数据，不能单独证明资源契约或运行时行为，自动推导前必须先有
  独立验证规则和失败语义。

### Roadmap 与 vocabulary

原 roadmap 的时间表、主线宣言和“传奇路线”已删除，vocabulary 的 schema 字段流水账也已
收敛。保留的讨论判断是：

- 在 codegen 前先验证输入规范化、结果可追溯和失败可解释；
- 静态 graph、runtime observation 与 fact evidence 应保持分离；
- `SystemSpec`、`BoardPackage`、`World`、`Witness` 等词没有自动进入 Core；
- 只有稳定 producer、consumer、反例和 smoke 出现后，才值得增加新 schema 或 IR。

对应 schema 和脚本仍在仓库中，例如：

- [`../../../schemas/system_compiler.canonical_world.v0.schema.json`](../../../schemas/system_compiler.canonical_world.v0.schema.json)
- [`../../../schemas/system_compiler.witness_bundle.v0.schema.json`](../../../schemas/system_compiler.witness_bundle.v0.schema.json)
- [`../../../schemas/system_compiler.world_compare.v0.schema.json`](../../../schemas/system_compiler.world_compare.v0.schema.json)
- [`../../../schemas/system_compiler.front_page_route.v0.schema.json`](../../../schemas/system_compiler.front_page_route.v0.schema.json)
- [`../../../scripts/compare_system_compiler_world.py`](../../../scripts/compare_system_compiler_world.py)

这些材料的价值在证据组织、比较和解释，不在对象数量。后续不得恢复 selector、plan、action、landing、compare 等逐步扩张的平行层级。

## 当前入口

- [`../../architecture/charm_core_contract.md`](../../architecture/charm_core_contract.md)
- [`../../architecture/system_compiler_roadmap.md`](../../architecture/system_compiler_roadmap.md)
- [`../../architecture/system_compiler_vocabulary_v0.md`](../../architecture/system_compiler_vocabulary_v0.md)
- [`../../system/README.md`](../../system/README.md)

本归档不得作为新增 Core 概念、公共 API 或实现路径的依据。
