# System Compiler Vocabulary v0

本文不是新的 DSL，也不是已经冻结的配置协议。
它用于收敛 Charm 在 `system compiler v0` 阶段最核心的一组词汇，
并把这些词和仓库当前已经存在的代码/文档载体做一轮正式映射。

它要回答的核心问题不是“未来最终配置文件长什么样”，而是：

> **当我们讨论 system compiler 时，仓库里哪些词已经可以当正式语言使用，它们当前分别落在什么地方。**

## 1. 为什么现在要先写词汇表

`docs/architecture/system_compiler_roadmap.md` 已经明确：

- v0 优先做“解释系统”
- 先冻结核心术语
- 先建立概念映射表

如果这一步不先做，后面会很容易出现几类漂移：

- 同一个词在不同文档里指不同东西
- 当前临时载体被误认为最终形态
- build 层概念、system 层概念、runtime 层概念互相挤占

词汇表的价值不是把未来一次说死，
而是先把“现在允许怎样说话”讲清楚。

## 2. v0 使用规则

当前建议把这份词汇表当成 **架构语言**，而不是新的配置格式。

v0 阶段的使用规则如下：

- 先固定词义，再决定是否需要新的配置对象或 codegen。
- 当“目标词汇”和“当前载体”不同名时，文档里要同时点名两者。
- 不把单一现有实现误写成最终唯一宇宙中心。
- 输入词汇和输出词汇要分开，不要把 `artifact report` 当成输入配置。

一个安全写法是：

> `BoardPackage`（当前核心载体是 `BoardCaps`）

而不是：

> `BoardCaps` 就已经等于最终 `BoardPackage`

## 3. 核心输入词汇

### 3.1 `SystemSpec`

它回答的是：

> **这个系统想成为什么样。**

它关心的通常不是某个单模块，
而是系统级目标，例如：

- 要暴露哪些 capability / service
- 要走哪条 bringup 路径
- 哪些 facet 应该处于活动状态
- 在哪些 board / profile 组合上应成立

当前仓库里的主要载体是分散的：

- 示例或应用目标的 CMake 组合
- `scripts/materialized_graph.export_case_manifest.v1.json` 这类当前导出链输入清单与 per-case `declared_facts / declared_contracts`
- `init.graph` 的装配链与 case 选择
- 系统设计文档里的目标描述
- `materialized_graph` / `artifact report` 里的 `subject.case` 作为临时投影

当前明确不要把它误解成：

- 单个 `CMakeLists.txt`
- 单个 `init chain`
- 单个导出 case 名字

当前状态：

- `SystemSpec` 已经是路线图正式词汇
- 但还没有收敛成单一源码对象
- `artifact report.system_input.system_spec`
  已开始作为 v0 结果物里的规范化输入投影，
  用来把当前 case 的 system spec 入口正式暴露给人和工具
- compare 模式下的 `artifact report.comparison.system_input`
  也已经开始把“系统如何成立的输入发生了什么漂移”正式拉进结果物，
  让输入语言不只可导出，也可比较

### 3.2 `Profile`

它回答的是：

> **这个系统允许活在哪种资源宇宙里。**

它更偏向资源/功能等级，
而不是某一个具体模块的启用开关。

当前仓库里的主要载体包括：

- `cmake/CharmTargetConfig.cmake` 中的 `charm_apply_target_profile(...)`
- `CMakeLists.txt` 中局部 featureset 语义，例如 `CHARM_VIVID_FEATURESET=FULL|MCU_MIN`
- `artifact report` 的 `subject.profile`

`Profile` 当前明确不等于：

- Debug / Release
- 单个 UI featureset
- 某一个编译器选项集合

更准确地说：

- `target profile`
  是当前 build 层对 `Profile` 的一部分承载
- `featureset`
  是局部子系统对 `Profile` 的一部分投影

当前状态：

- 词已经有现实载体
- 但载体仍然分散，还没统一成单一 profile 语言
- `artifact report.system_input.resolved_input.profile`
  现在已经把“当前 profile 最终取值及来源”正式投影出来

### 3.3 `BoardPackage`

它回答的是：

> **板级已知事实是什么。**

它应承担的是板级事实声明，
而不是偷偷推进生命周期。

一个成熟的 `BoardPackage` 未来通常应能承载：

- 板级资源事实
- live handle / ops / 默认配置
- capability 名称与绑定关系
- bringup 所需的已知事实
- 板级 smoke / evidence 的入口信息

当前仓库里的核心载体是：

- `platform::board::BoardCaps`
- 各板级 `make_board_caps()`

当前还可能伴随：

- 板级 CMake/target 配置
- bringup 相关脚本或示例入口

这里要明确：

> **`BoardCaps` 是当前 `BoardPackage` 的核心事实载体，但不是全部。**

当前不要把 `BoardPackage` 误写成：

- 隐式初始化函数
- 偷偷注册全局状态的 BSP 钩子
- 运行期 probe / match 容器

当前状态：

- `BoardCaps` 已经很稳定
- `BoardPackage` 仍是更上位的汇总词
- `artifact report.system_input.resolved_input.board`
  现在已经把 board 取值及来源正式投影出来，但还不是完整 `BoardPackage`

### 3.4 `Binding`

它回答的是：

> **这些事实如何连接到 capability、服务与最终系统结果。**

`Binding` 在 Charm 里不应被缩成单个类名，
它更像一门桥接语言。

当前它主要有两条承载平面：

- 静态 capability 平面
- 动态 discovery 平面

静态平面当前的主要载体包括：

- `hal::*Binding`
- `driver::*::ChannelBinding`
- `io::ChannelAliasBinding`
- `block::*Binding`
- `CoreSystemChain` / `UsartInitChain` 这类装配组合器

动态平面当前的主要载体包括：

- `device::Bus`
- `device::Driver`
- `device::Registry`
- capability export / stable slot export

这里最重要的边界是：

> **`Binding` 不等于单个 `device::Driver`，也不只等于某个 `*Binding` 类。**

在静态 capability 主轴下，
当前更安全的理解是：

```text
BoardCaps
  -> ControllerBinding
  -> ServiceAdapter
  -> capability export / registry
```

在动态 discovery 平面下，
当前更安全的理解是：

```text
RuntimeBus
  -> RuntimeDriver
  -> capability export
```

当前状态：

- 词义已经比较清楚
- 但它仍由多组现有实现共同承载

### 3.5 `Facet`

它回答的是：

> **同一套架构在当前实例里，到底启用了哪些面。**

`Facet` 不是 profile，
也不是简单的“有没有编某个模块”。

它更像系统 embodiment 的切面语言。

当前仓库里至少已经有两类载体：

- build 侧 facet
- report / system 侧 facet

build 侧的现实载体包括：

- `CMakeLists.txt` 里的 `charm_add_runtime_facet(...)`
- `Charm::core` / `Charm::io` / `Charm::platform` / `Charm::system`

report / system 侧的现实载体包括：

- `artifact report` 的 `subject.active_facets`
- 各类文档里描述的 `runtime` / `input` / `ui` 等语义面

这里要特别避免一个误解：

> **当前 build facet 和 report facet 还不是完全同一套命名体系。**

因此 v0 阶段建议这样写：

- `build facet`
  指构建组织层的 facet target
- `system facet`
  指 system compiler / report 语义里的活动面

当前状态：

- 词已经有现实入口
- 但命名和粒度仍在收敛
- `artifact report.system_input.resolved_input.active_facets`
  现在已经把活动 facets 及其解析来源正式投影出来

### 3.6 `Case`

它回答的是：

> **当前被导出、比较、报告的这个具体场景是谁。**

`Case` 是当前工具链里已经很稳定的工程词，
但它不应被误认成完整 `SystemSpec`。

当前载体包括：

- `scripts/materialized_graph.export_case_manifest.v1.json` 中的 case 条目
- `materialized_graph` 导出 case
- `bundle` / `bundle_diff` / `ci_summary` 中的 case 条目
- `artifact report` 的 `subject.case`

更准确的理解是：

> **`Case` 是当前 system compiler v0 对某个场景的命名投影。**

当前状态：

- 工具链已稳定使用
- 但它仍是 `SystemSpec` 的临时投影，不是最终同义词

## 4. 核心输出词汇

### 4.1 `Capability`

它回答的是：

> **系统最终对外提供、并允许其它部分消费的能力是什么。**

当前对应载体包括：

- `init.graph` 中的 capability provider / consumer
- `io.registry` / block registry 中的稳定入口
- 动态 discovery 平面导出的 slot / endpoint / registry entry

它是 Charm 最终统一语言之一。

### 4.2 `Fact`

它回答的是：

> **为了让某条绑定或某条资源法律成立，系统当前已知哪些事实。**

当前对应载体包括：

- `export case manifest` 里的 per-case `declared_facts`
- `export case manifest` 里的 per-case `declared_contracts.requires`
- `required_facts`
- `provided_facts`
- `artifact report.fact_resolution.fact_inventory`
- board 已知资源与环境条件

这里需要特别注意：

> **同一个名字在不同视角下，可能既是 capability，也可能是 fact。**

例如：

- `system.clock`
  既可以是系统对外可消费的 capability
  也可以是资源契约或绑定检查中的 provided fact

两者差异不在名字，而在语义位置。

### 4.3 `SystemInput`

它回答的是：

> **当前系统是以什么规范化输入被解释/编译出来的，以及这些输入在多 case 之间如何收口或漂移。**

它当前更偏“输入侧结果物”，
而不是单个 subject 字段或 metadata diff 本身。

当前对应载体包括：

- `artifact report` 中的 `system_input`
- `artifact report.comparison.system_input`
- artifact_root 默认总览中的 `system_input_summary`
- artifact_root 默认总览中的 `comparison.system_input_summary`
- 默认总览里的 `InpCmp` 与 compare 摘要里的 `input_changed_case_count`

它最适合回答这类问题：

- 某个 case 当前到底属于哪种 `case_kind`
- declared input 与 resolved input 分别是什么
- 一组 case 整体有哪些 resolved profile / resolved board / active facet
- 哪些 declared fact / declared contract / subject fact 在多 case 之间收口
- compare 模式下输入漂移发生在 `system_spec`、`declared_input` 还是 `resolved_input`

### 4.4 `BindingResult`

它回答的是：

> **当前这组 required binding 里，哪些已经成立，哪些还没成立。**

它当前更偏“成立性结果物”，
而不是图本身或 explain query 本身。

当前对应载体包括：

- `artifact report` 中的 `binding_result`
- artifact_root 默认总览中的 `binding_result_summary`
- artifact_root 默认总览中的 `comparison.binding_result_summary`
- `required_facts / unresolved_bindings`
- capability provider / consumer 的最小汇总结论

它最适合回答这类问题：

- 哪些 binding 已经 resolved
- 哪些 binding 还 unresolved
- 某个 required capability 由谁提供、被谁消费
- compare 模式下哪些 capability 的 binding state 已经发生漂移

### 4.5 `FactResolution`

它回答的是：

> **当前有哪些 facts 已经进入可用库存，哪些只是被要求存在，以及每条输入侧资源法律为什么成立或不成立。**

它当前更偏“输入与成立结果之间的事实收口语言”，
而不是 explain query 本身。

当前对应载体包括：

- `artifact report` 中的 `fact_resolution`
- `artifact report.comparison.fact_resolution`
- artifact_root 默认总览中的 `fact_resolution_summary`
- artifact_root 默认总览中的 `comparison.fact_resolution_summary`
- report 级 `resource summary` explain 结果

它最适合回答这类问题：

- 当前有哪些 `declared / subject / required / graph_provided / audit_provided` facts
- 哪条合同当前是 `satisfied / violated / unknown`
- 某条合同成立时，证据来自 declared、subject 还是 graph/audit fact
- compare 模式下哪些 facts 或合同成立性已经发生漂移

### 4.6 `BringupOrder`

它回答的是：

> **当前系统实际按什么顺序被物化/bring up，以及每个节点依赖谁。**

当前对应载体包括：

- `materialized_graph.sample/v2` 里的节点顺序、phase、requires/provides
- `artifact report` 中的 `bringup_order`
- artifact_root 默认总览中的 `bringup_order_summary`
- artifact_root 默认总览中的 `comparison.bringup_order_summary`

它和 `Materialized Graph` 的区别是：

- `Materialized Graph`
  更偏结构事实
- `BringupOrder`
  更偏“成立过程的结果语言”

它最适合回答这类问题：

- 谁先被 bring up
- 某个节点依赖谁
- 哪些 require 已满足
- 哪些 require 仍缺失，因此当前只能标成 blocked
- compare 模式下哪些节点的 bringup order / blocked 状态已经发生漂移

### 4.7 `SystemFormation`

它回答的是：

> **当前这组 binding 与 bringup 结果最终是否已经形成一个可成立的系统，以及阻塞点是什么。**

当前对应载体包括：

- `artifact report` 中的 `system_formation`
- `artifact report.comparison.system_formation`
- artifact_root 默认总览中的 `system_formation_summary`
- artifact_root 默认总览中的 `comparison.system_formation_summary`
- 默认总览里的 `Formation / FormCmp`

它最适合回答这类问题：

- 当前系统整体是 `formed` 还是 `blocked`
- unresolved capability 与 blocked node 是否已经收敛成正式 blocker 列表
- 一组 case 整体有多少已经 `formed` / `blocked`
- compare 模式下哪些 case 发生了 `formed -> blocked` 一类 formation 漂移
- compare 漂移有没有已经进入成立性结果面

### 4.8 `Materialized Graph`

它回答的是：

> **系统装配后，真正形成的图是什么。**

当前对应载体包括：

- `init.materialize`
- `materialized_graph.sample/v2`
- DOT / JSON sample 导出
- export bundle

它是当前 system compiler 最成熟的结果物之一。

### 4.9 `Bringup Evidence`

它回答的是：

> **这条 bringup 路径被推进到什么状态，并且有哪些证据。**

当前对应载体包括：

- `docs/system/bringup_evidence_pipeline_v0.md`
- `artifact report` 中的 `bringup_evidence` 摘要

### 4.10 `Resource Contract`

它回答的是：

> **这些系统部分在当前资源/执行宇宙里是否合法。**

当前对应载体包括：

- `docs/system/resource_contract_v0.md`
- `artifact report` 中的 `resource_contract` 摘要

### 4.11 `Artifact Report`

它回答的是：

> **本次 system compiler/export/compare 到底稳定产出了哪些结论对象。**

当前对应载体包括：

- `docs/system/artifact_report_v0.md`
- `schemas/system_compiler.artifact_report.v0.schema.json`
- `scripts/export_system_compiler_artifact_report.ps1`
- artifact_root 默认总览中的 `system_compiler_summary`
- artifact_root 默认总览中的 `comparison.system_compiler_summary`

### 4.12 `Explain Surface`

它回答的是：

> **当人和工具想继续追问系统事实时，应通过什么统一问题面来追问。**

当前对应载体包括：

- `docs/system/explain_surface_v0.md`
- runtime observe / publish / export 状态
- `artifact report` 作为 explain 输入工件

## 5. 最小概念映射表

| 目标词汇 | 当前主要载体 | 当前状态 | 当前不要误写成 |
| --- | --- | --- | --- |
| `SystemSpec` | 应用/示例目标、export case manifest、init case、设计文档、`artifact report.system_input.system_spec` | 词已确立，开始进入结果物投影 | 单个 case / 单个 CMakeLists |
| `Profile` | target profile、局部 featureset、report profile 字段、`system_input.resolved_input.profile` | 已有碎片化载体，开始显式投影解析来源 | Debug/Release、单个 UI 配置 |
| `BoardPackage` | `BoardCaps` + 板级 target/config + `system_input.resolved_input.board` | 事实载体已存在，开始显式投影 board 取值来源 | 隐式 init / BSP 黑盒 |
| `Binding` | `*Binding`、init chain、runtime driver/export | 双平面都已存在 | 只等于 `device::Driver` |
| `Facet` | facet target、`active_facets`、语义面文档、`system_input.resolved_input.active_facets` | 词已出现，开始显式投影解析来源 | profile / target / component |
| `Case` | export case manifest、export bundle / CI / report 的 case 名 | 工具链已稳定使用 | 完整 `SystemSpec` |
| `Capability` | `init.graph`、registry、slot export | 最稳定的统一语言之一 | 任意板级细节或内部 handle |
| `Fact` | `export case manifest.declared_facts` / `declared_contracts.requires` / `required_facts` / `provided_facts` | 已有输入侧与报告侧载体 | 单纯等于 capability 名字 |
| `SystemCompilerSummary` | `system_compiler_summary`、`comparison.system_compiler_summary`、artifact_root 默认总览、[`../../schemas/system_compiler_summary.v0.schema.json`](../../schemas/system_compiler_summary.v0.schema.json)、`cases[*].formation_basis / binding_summary / bringup_summary`、`blocker_reason_matrix / blocker_missing_requires_matrix / blocker_depends_on_matrix`、`binding_reason_matrix / bringup_phase_matrix / bringup_dependency_matrix`、`formation_basis / binding_basis / bringup_basis / formation_drift / binding_drift / bringup_drift / result_map` | root 级跨阶段总结果物已经出现，并开始携带单 case 成立 basis、formation/binding/bringup 三段 stage block，以及这些 block 与分阶段 summary 的 machine-readable 对应关系；现在对象本身也会显式带出 `kind = system_compiler_summary/v0` 与 `mode = summary | comparison` 两个自描述字段。其中 `result_map.stage_blocks[*].root_fields` 用来标出 `system_compiler_summary` 根上的 stage 归属字段，`result_map.*.field_relations[*]` 继续把 root field 到 block 字段、summary 字段之间的 `same_field / field_alias / none` 关系正式导出，而 `result_map.case_projection_field_relations.<stage>[*]` 则把单 case projection 字段到 stage case summary 字段之间的 `same_field / field_alias` 关系与 fallback source 一并导出 | 单个阶段摘要或单条 compare 统计 |
| `SystemInput` | `artifact report.system_input`、`comparison.system_input`、`system_input_summary`、`comparison.system_input_summary`、默认总览 `InpCmp` | 已有正式输入侧结果物载体，并开始进入 root 级聚合摘要 | 单个 subject 字段或 metadata diff |
| `BindingResult` | `artifact report.binding_result`、`binding_result_summary`、`comparison.binding_result_summary`、`required_facts / unresolved_bindings` | 已有正式结果物载体 | 图本身或单条 explain query |
| `FactResolution` | `artifact report.fact_resolution`、`comparison.fact_resolution`、`fact_resolution_summary`、`resource summary` | 已有正式结果物载体 | 只等于 `resource_contract` 审计层 |
| `BringupOrder` | `artifact report.bringup_order`、`bringup_order_summary`、`comparison.bringup_order_summary`、materialized graph 节点顺序 | 已有正式结果物载体 | 仅仅等于 DOT 展示顺序 |
| `SystemFormation` | `artifact report.system_formation`、`comparison.system_formation`、默认总览 `Formation / FormCmp` | 已有正式结果物载体 | 单纯等于 `binding_result` 或 `bringup_order` |
| `Artifact Report` | schema + export script + CI 输出 | 已有真实最小生成链 | explain surface 本身 |

## 6. 一个最小 worked example

以 `bringup-minimal-observe-demo` 这类最小导出场景为例，
可以把词汇映射成下面这条链：

```text
SystemSpec
  -> 我想得到一个最小可观察系统，并暴露 console / input 等能力

Profile
  -> MCU_MIN（资源/功能等级的当前投影）

BoardPackage
  -> stm32_stub 的 BoardCaps、handle、默认绑定关系

Binding
  -> hal::UartBinding
  -> driver::usart::ChannelBinding
  -> io::ChannelAliasBinding

Facet
  -> runtime / input（当前 report 语义里的 active facets）

Outputs
  -> materialized graph
  -> bringup evidence summary
  -> resource contract summary
  -> artifact report
```

这条例子最重要的价值不是展示某个具体 demo，
而是说明：

> **system compiler 词汇不是悬在空中的术语，它们已经可以映到当前仓库里的真实链路。**

## 7. v0 写作建议

如果后续文档要使用这组词，当前建议遵守下面几条写法：

- 先写目标词，再点名当前载体。
- 当 build 侧与 system/report 侧同名但不同义时，加前缀说明。
- 不把 `Case`、`Profile`、`Facet` 三者写成同一个东西。
- 不把 `BoardCaps`、`device::Registry` 这类现有实现提升成唯一世界模型。

一个推荐句式是：

> 当前 `BoardPackage` 的核心事实载体是 `BoardCaps`，  
> 当前 `Binding` 主要由 `*Binding + init chain + capability export` 共同承载。

## 8. 与其它文档的关系

- `docs/architecture/system_compiler_roadmap.md`
  负责给出主轴、维度、边界与时间线
- `docs/architecture/system_compiler_vocabulary_v0.md`
  负责给出核心词义与当前仓库映射
- `docs/system/artifact_report_v0.md`
  负责给出最小结论对象
- `docs/system/explain_surface_v0.md`
  负责给出人和工具继续追问系统的表面
- `docs/system/bringup_evidence_pipeline_v0.md`
  负责给出 bringup 证据语言
- `docs/system/resource_contract_v0.md`
  负责给出资源法律语言

## 9. 当前结论

`System Compiler Vocabulary v0` 当前最重要的作用不是制造新对象，
而是先把 Charm 已经长出来的那几根主词压稳：

- `SystemSpec`
- `Profile`
- `BoardPackage`
- `Binding`
- `Facet`

只要这些词义稳定，
后续无论是 `artifact report`、bringup evidence、resource contract，
还是更远一点的 managed time / guest world，
都能在同一套语言里继续生长。
