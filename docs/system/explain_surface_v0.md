# Explain Surface / Artifact Report v0

本文不是新的调试 UI，也不是最终冻结的外部协议。  
它用于定义 Charm 在 `system compiler v0` 阶段面向人类与工具的最小输出面：`artifact report` 与 `explain surface`。

它要回答的核心问题不是“系统内部是不是很优雅”，而是：

> **当系统已经被编译、物化、导出之后，人和工具到底该通过什么表面理解它。**

在 Charm 的中长期主线上，`explain surface` 不是额外酷炫功能，  
它更接近 `system compiler` 的自然副产物：

- 如果系统可以被编译
- 如果 bringup 可以被举证
- 如果资源边界可以被审计

那么这些结果就不应只留在源码和人脑里，  
而应成为可以被报告、被查询、被自动化消费的对象。

## 1. 为什么需要单独定义输出面

Charm 现在已经不是只在追求“结构比较整齐”的阶段。  
当前主线已经开始要求：

- 系统秩序可解释
- bringup 过程可举证
- 资源边界可审计

一旦这些东西成立，新的问题就会立刻出现：

- 哪些结果是给人读的
- 哪些结果是给脚本和 CI 读的
- 哪些结果只是样例协议
- 哪些结果已经可以作为稳定桥接面
- runtime 观察与静态导出如何对齐

如果没有单独的输出面定义，  
这些解释物就会再次退化成：

- 零散脚本
- 临时 JSON
- 各种只对当前作者有意义的导出格式

这会直接削弱 system compiler 最关键的价值之一：

> **把系统秩序交给人和工具共同消费。**

## 2. `artifact report` 与 `explain surface` 的区别

这两个词相关，但不应混成同一件事。

### 2.1 `artifact report`

`artifact report` 更偏：

- 批量生成
- 文件工件
- CI / 脚本 / IDE 原型消费
- 构建后或导出后查看

它回答的是：

> “本次系统编译/导出，稳定产出了哪些可审计结果。”

### 2.2 `explain surface`

`explain surface` 更偏：

- 人类可追问
- 工具可查询
- 读路径统一
- 静态结果与运行时观察的桥接面

它回答的是：

> “当我想追问某个系统事实时，应该通过什么统一问题模型拿到答案。”

可以把两者理解为：

- `artifact report`
  是工件面
- `explain surface`
  是问题面

前者更像“系统吐出什么”，  
后者更像“人和工具如何追问系统”。

## 3. v0 的位置与范围

`explain surface v0` 当前不追求做成完整 runtime inspector 平台。  
它在现阶段的职责很收敛：

> **先把现有静态导出、报告工件与少量运行时观察收束成一套统一输出语言。**

这意味着 v0 的重心应是：

- 只读
- 可导出
- 可引用
- 可审计
- 可被脚本消费

其中 runtime 观察这条线当前也应保持同样纪律：

- 不把 `recent transitions` 绑死在某个示例内部日志上
- 优先把它做成独立 sidecar 工件
- 再由 `artifact report` 吸收为 explain surface 可查询的问题面

而不是：

- 新做一个庞大的交互式工具
- 提前冻结所有长期协议
- 一步到位做成“系统全知视角”

## 4. 当前已经存在的胚胎

Charm 这条线并不是从零开始。  
当前仓库已经有几组很强的输出面胚胎。

### 4.1 `materialized_graph` 观察导出链

当前已经存在：

- `docs/system/init_materialized_graph_observe.md`
- `init.observe`
- `DOT / JSON sample` 导出

这条链已经证明：

> **系统装配结果可以稳定投影成只读语义视图。**

### 4.2 机器可读协议分层

当前 `schemas/` 已经明确区分了几层协议：

- `sample/v2`
- `export_bundle/v1`
- `bundle_diff/v1`
- `ci_summary/v1`
- `report_manifest/v1`

对应说明见：

- `schemas/README.md`

这非常重要，因为它说明当前仓库已经不是只有“随手导个 JSON”，  
而是在开始定义：

- 哪些协议偏样例
- 哪些协议偏稳定桥接
- 哪些协议是 CI / report 链消费面

### 4.3 bringup 与资源这两条新子主线

当前已经新增：

- `docs/system/bringup_evidence_pipeline_v0.md`
- `docs/system/resource_contract_v0.md`

它们已经给出两类新的解释物需求：

- bringup evidence report
- resource contract report

也就是说，`materialized_graph` 不再是唯一输出源，  
system compiler 正在长出多张可被解释的面。

### 4.4 runtime 可观察胚胎

在 runtime 这一侧，当前也已经出现稳定观察缝：

- `PublishState`
- `ExportState`
- `ExportTransition`

它们虽然还不是完整 explain surface，  
但已经证明：

> **运行时变化也可以被提升成稳定观察语言，而不是只靠临时日志。**

## 5. v0 的最小 `artifact report`

当前建议把 system compiler 的最小工件面先收敛成一份统一的 `artifact report` 语义，而不是继续平铺很多彼此独立的小报告。

具体字段分组、最小样例对象与工程边界，
见：`docs/system/artifact_report_v0.md`

v0 阶段建议至少覆盖：

- capabilities
- materialized order
- required facts
- unresolved bindings
- active facets
- bringup evidence summary
- resource contract summary
- supporting artifacts 引用

这里最关键的一点是：

> **报告本身应成为可引用对象，而不是“看完就散”的终端输出。**

这意味着 v0 报告至少需要稳定描述：

- 它是谁生成的
- 它引用了哪些底层工件
- 它覆盖哪些 case / profile / board / facet
- 它的摘要结论是什么

当前仓库里，这层最小只读消费面已经开始有了一个很具体的胚胎：

- `scripts/inspect_system_compiler_artifact_report.ps1`

它当前不是完整 explain shell，
但已经能把 `artifact report` 压成一页稳定可读摘要，
并继续把结构、资源契约、compare 结论与底层工件引用一起带出来。

## 6. v0 的最小 `explain surface`

当前建议先把问题面收敛为五类最小查询，而不是先发明很大的命令系统。

### 6.1 `cap list`

它回答：

- 当前系统有哪些 capability
- 哪些是 materialized 结果
- 哪些已经进入 published 表面
- 哪些名字同时承担 `declared_fact / resource_fact / unresolved_binding` 语义

它的输入可以来自：

- materialized graph 导出
- registry publish 状态
- bringup evidence report

当前仓库里，这个问题面已经有了一个最小真实入口：

- `scripts/inspect_system_compiler_artifact_report.ps1 -CapList`

它当前优先消费：

- `artifact report`

更具体地说，当前真实的 runtime transition 数据优先应通过：

- bundle case 级 `runtime_observe` sidecar
- `artifact report.runtime_observe`
- `scripts/inspect_system_compiler_artifact_report.ps1 -RecentTransitions`

这条链逐层进入 explain surface。
如果当前 case 没有接入 sidecar，
`recent transitions` 仍保留稳定查询形状，但结果为空。
- `artifacts.sample_json`

并先把最小输出收敛为：

- `capability`
- `materialized / observed / published / required`
- `declared_fact / resource_fact / unresolved_binding`
- `provider_nodes / consumer_nodes`

如果当前 report 来自 compare 模式，
单 report 级 `-CapList -AsJson` 现在也会把 capability 级 compare 摘要一起带出来，
至少覆盖：

- `comparison.bringup_changed / comparison.bringup_change_kinds`
- `comparison.resource_changed / comparison.resource_change_kinds`
- `comparison.resource_contracts`

与此同时，
单 report 默认总览里的 `comparison` 现在也会继续附带：

- `comparison.capability_summary.compared_capability_count`
- `comparison.capability_summary.bringup_compare_capability_count / resource_compare_capability_count`
- `comparison.capability_summary.compared_capabilities`

当前实现支持两种读取作用域：

- 单 report 查询
- 全 artifact root 汇总

其中单 report 查询适合回答：

- 这个 case 当前有哪些 capability
- 谁提供它
- 谁消费它

而全 root 汇总适合回答：

- 这个 capability 出现在哪些 case
- 它在哪些 case 被 materialized / published / required

为了避免把“单 case 证据”与“跨 case 聚合”混成一种半语义状态，
当前实现不支持对任意多 case 子集直接执行 `cap list` 汇总。
也就是说，`cap list` 当前只接受：

- 精确单 report
- 或整个 artifact root

如果调用方需要稳定机器消费，
当前 `-AsJson` 会返回上述最小字段，
而 root 汇总模式还会额外带出 `cases` 与 `*_cases` 数组，
用于表达 capability 在不同 case 中的出现分布。

如果选择的是整组 compare report，
artifact_root 级 `-CapList -AsJson` 现在也会继续暴露：

- capability 级 `compare_cases / bringup_compare_cases / resource_compare_cases`
- capability 级 `bringup_change_kinds / resource_change_kinds`
- query 级 `comparison.compared_capability_count`
- query 级 `comparison.bringup_compare_capability_count / resource_compare_capability_count`

这意味着 `cap list` 现在已经不只会回答“这个 capability 出现在哪些 case 里”，
还可以直接回答：

- 哪些 capability 本身已经进入 compare drift 热点
- 这些热点更偏 bringup 漂移还是资源法律漂移
- 某个 capability 的 compare 漂移究竟覆盖了哪些 case

与此同时，
如果调用方直接读取 artifact_root 默认总览，
当前 `comparison` 摘要也会继续附带一个最小 `capability_summary`，
至少包括：

- `capability_summary.compared_capability_count`
- `capability_summary.bringup_compare_capability_count / resource_compare_capability_count`
- `capability_summary.compared_capabilities`

这意味着默认 explain 面现在已经可以先回答两层问题：

- 有多少 case 发生了 compare drift
- compare drift 主要集中在哪些 capability 上

围绕同一批 capability，
当前仓库里还新增了一个更直接的单 report 入口：

- `scripts/inspect_system_compiler_artifact_report.ps1 -BringupEvidence`

它当前会把 `artifact report.bringup_evidence.evidence_entries` 展开成 capability 级证据矩阵，
至少带出：

- `declared / materialized / published / observed / blocked / failed`
- `publish_state / export_state`
- `provider_nodes / consumer_nodes`
- capability 级 `blocked_reasons / failed_reasons`

这意味着当前 `cap list`、`why unavailable` 与 `bringup evidence` 三个问题面，
已经开始共享同一批 capability 级证据来源，
而不是各自再维护一套互相漂移的判断。

与此同时，`-ArtifactRoot ... -BringupEvidence` 现在也已经支持直接返回 artifact_root 级聚合结果。
它会把多份 report 收束成：

- per-case bringup 摘要
- capability matrix
- blocked reason matrix
- failed reason matrix

如果这些 report 来自 compare 模式，
artifact_root 级 `-BringupEvidence -AsJson` 现在也会继续暴露
`query.comparison.bringup_evidence`，至少带出：

- `compared_case_count / changed_case_count / unchanged_case_count`
- `changed_cases / unchanged_cases`
- `summary_change_matrix`
- `capability_change_matrix`

这意味着 explain surface 已经不只会回答“哪个 capability 在哪些 case 中存在”，
还可以横向回答：

- 哪些 case 的 bringup 证据相对 baseline 发生漂移
- 漂移集中在哪些 summary change
- 某个 capability 的 compare 漂移究竟出现在多少 case 里

这样 bringup 证据不再只能按单 case 追问，
还可以直接横向查看：

- 哪个 capability 在哪些 case 中只停留在 declared 态
- 哪些 capability 已经跨 case materialized / observed
- `publish_state / export_state` 在不同 case 之间如何分布
- 哪些 blocked / failed reason 在多个 case 之间重复出现

如果当前 report 来自 compare 模式，
`-BringupEvidence -AsJson` 还会额外暴露 `query.comparison.bringup_evidence`，
至少带出：

- `changed`
- `left / right`
- `summary_changes`
- `published_capability_changes`
- `capability_changes`

这让 explain surface 可以在结构 diff 仍保持 `unchanged` 时，
继续回答“bringup 证据相对 baseline 漂移了什么”，
尤其是哪些 capability 的 `publish_state / export_state` 已经发生切换。

### 6.2 `why unavailable`

它回答：

> “为什么某个 capability / 绑定 / 服务当前不可用。”

这条查询是 explain surface 里最有价值的能力之一。  
它应优先能区分：

- 未声明
- 未 materialized
- 依赖未满足
- 未 published
- 被 blocked
- 已 failed
- 资源契约 violated

也就是说，它不只是回答“没有”，  
而要回答：

> **到底卡在哪一层。**

当前仓库里，这个问题面已经有了一个很小但真实的入口：

- `scripts/inspect_system_compiler_artifact_report.ps1 -WhyCapability <name>`

它当前支持两种读取作用域：

- 单 report 查询
- 全 artifact root 汇总

它当前优先消费：

- `artifact report`
- `artifacts.sample_json`

并先覆盖几类最小结论：

- `available`
- `materialized_not_published`
- `runtime_observed_not_published`
- `runtime_observed_not_materialized`
- `unresolved_binding`
- `required_without_provider`
- `undeclared`

也就是说，v0 当前先不追求“解释整个运行时宇宙”，
而是先让系统能基于稳定工件回答：

> **这个名字为什么现在没有站到可用面上。**

如果当前 report 来自 compare 模式，
`-WhyCapability -AsJson` 现在也会继续为目标 capability 带出最小 `comparison` 证据块，
至少包括：

- `comparison.changed`
- `comparison.bringup_changed / comparison.bringup_change_kinds`
- `comparison.resource_changed / comparison.resource_change_kinds`
- `comparison.resource_contracts`

如果选择的是整组 compare report，
artifact_root 级 `-WhyCapability -AsJson` 现在也会继续带出：

- `state_counts`
- `compared_case_count / bringup_compare_case_count / resource_compare_case_count`
- `compared_cases / bringup_compare_cases / resource_compare_cases`
- `resource_contracts`

这意味着调用方现在不只可以追问：

- 它为什么当前不可用

还可以继续追问：

- 它相对 baseline 为什么漂了
- 漂移更偏 bringup 证据还是资源法律
- 漂移具体落到了哪条 contract change 或 publish/export state 切换上
- 这个 capability 在哪些 case 里共同卡在同一种状态

### 6.3 `graph path`

它回答：

- 某个 consumer 依赖链是如何成立的
- 某条 bringup 路径经过哪些关键节点
- 某个 capability 的 provider 链接在哪里

当前最自然的静态输入来源仍然是：

- `materialized_graph`
- 依赖边导出
- bringup evidence summary

当前仓库里，这个问题面也已经有了一个最小真实入口：

- `scripts/inspect_system_compiler_artifact_report.ps1 -GraphPath <capability>`

它当前优先消费：

- `artifact report`
- `artifacts.sample_json`

如果当前 report 对应的是 `runtime_only` case，
也就是 `artifacts.sample_json` 为空，
当前查询会稳定返回 `graph_unavailable`，
而不是假装替调用方拼出一张并不存在的静态图。

当前 v0 的实现刻意收敛到“单 report + capability 维度”：

- 只接受精确单 report
- 不做跨 case 聚合
- 不把节点名查询、任意图遍历和多 report 比较混进同一个接口

它当前最小稳定输出会围绕以下字段组织：

- `capability`
- `state / availability_state`
- `direct_edges`
- `provider_paths`
- `consumer_paths`

其中：

- `state`
  当前优先表达图查询自身的语义状态，例如：
  `edge_paths / provider_terminal / required_without_provider / undeclared`
- `availability_state`
  则继续复用 `why unavailable` 的状态语言，
  让“图里怎么连”和“为什么不可用”保持可对齐

当前 `graph path` 的最小解释方式是：

- 如果该 capability 在图里存在 direct edge，
  就给出 direct edge 以及经过这条 edge 的 consumer 路径
- 如果它当前只是终端 provider，
  就给出通向该 provider 节点的最小依赖路径
- 如果它只出现在 consumer 需求里但没有 provider，
  就给出通向 consumer 的依赖路径

也就是说，v0 当前还不是“图查询语言”，
而是一个面向 explain surface 的最小稳定问题面：

> **围绕一个 capability，把它在当前 materialized graph 里到底接到了哪里、又是通过哪些依赖链接上的，稳定吐出来。**

### 6.4 `recent transitions`

它回答：

- 最近有哪些运行时状态变化发生
- 哪些 published / attached 状态发生过切换
- 哪些 runtime export 事件值得被观察

这条查询当前更适合先挂在：

- `PublishState`
- `ExportState`
- `ExportTransition`

之上，而不急着追求全系统统一事件总线。

当前仓库里，这个问题面现在也已经有了一个最小真实入口：

- `scripts/inspect_system_compiler_artifact_report.ps1 -RecentTransitions`

它当前优先消费：

- `artifact report`

当前 v0 的实现同样保持很克制：

- 只接受精确单 report
- 不做跨 case 聚合
- 不把 runtime transition 查询扩展成完整事件总线或 tracing 平台

它当前最小稳定输出会围绕以下字段组织：

- `observed_capabilities`
- `publish_state_summary`
- `export_state_summary`
- `transition_count`
- `transition_capabilities`
- `action_counts`
- `transitions`

其中 `transitions` 当前至少保留：

- `order`
- `capability`
- `action`
- `before`
- `after`

这意味着 `recent transitions` 现在先回答的是：

- 最近到底有没有状态切换发生
- 切换集中在哪些 capability
- 切换动作是 `attach`、`ensure_exported` 还是其它 runtime export 事件
- 当前 publish/export 统计摘要是什么

也就是说，v0 当前不是在承诺“完整运行时历史”，
而是在把 runtime observe 面先压成一个最小稳定查询：

> **围绕 artifact report 当前保留下来的最近切换，稳定回答“发生了什么、发生在谁身上、在 publish/export 语义里当前是什么样”。**

当前仓库里已经有一条真实 runtime-only producer：

- `Examples/usb/usb_host_runtime_multi_smoke`

它现在已经作为正式 `runtime_only` case 接入
`export_case_manifest -> export_bundle -> artifact_report`，
并可以稳定导出非空 `recent_transitions` 的 `runtime_observe` sidecar。
同时要注意，这个示例在场景末尾会主动执行 `remove / forget / unexport`，
因此 sidecar 顶层摘要反映的是“最终已清理”的末态：

- `published_capabilities` 为空
- `publish_state_summary` 会落在 `missing`
- `export_state_summary` 会落在 `missing`

这不是导出失败，而是 explain surface 当前对 runtime 侧刻意保持的语义：

> **摘要回答“现在是什么状态”，`recent_transitions` 回答“刚才发生了什么”。**

另外，当前仓库里已经有第一条正式接入 bundle/report 链的 graph case：

- `Examples/usb/usb_msc_block_demo`

它会在 `export_case_manifest -> export_bundle -> artifact_report` 路径里
产出同 case 的真实 `runtime_observe` sidecar。
这条 sidecar 当前不强调 transition history，而强调 bringup 完成后的真实末态：

- `published_capabilities = ["block.sd0"]`
- `publish_state_summary.published = 1`
- `recent_transitions = []`

也就是说，当前 explain surface 已经开始同时覆盖两种真实来源：

- transition-rich runtime-only producer
- graph-integrated final-state producer

### 6.5 `resource summary`

它回答：

- 当前系统声明了哪些资源/行为要求
- 哪些条件由 profile / board / runtime 提供
- 哪些要求已 satisfied
- 哪些 violated
- 哪些仍 unknown

这条查询正是：

- `resource contract v0`
- `explain surface v0`

之间最自然的连接点。

当前仓库里，这个问题面现在也已经有了一个最小真实入口：

- `scripts/inspect_system_compiler_artifact_report.ps1 -ResourceSummary`

它当前优先消费：

- `artifact report`
- `artifacts.sample_json`

当前 v0 的实现同样保持很克制：

- 单 report 查询仍是资源解释的主入口
- `artifact_root` 聚合查询只负责矩阵与横向摘要
- 不把资源 summary、capability explain、图路径查询揉成一个混合接口

它当前最小稳定输出会围绕以下字段组织：

- `declared_contracts / audited_count / satisfied_count / violated_count / unknown_count`
- `fact_inventory`
- `contracts`
- `resource_hotspots`

其中：

- `fact_inventory`
  当前至少区分：
  `declared_facts / subject_facts / graph_provided_facts / audit_provided_facts / all_available_facts`
- `contracts`
  则把每条输入侧合同压成一条稳定 explain 记录，
  至少带出：
  `contract / state / requires / present_facts / missing_facts / fact_sources`

如果当前 report 来自 compare 模式，
`-ResourceSummary -AsJson` 还会额外暴露 `query.comparison.resource_contract`，
至少带出：

- `changed`
- `left / right`
- `summary_changes`
- `contract_changes`
- `provided_fact_changes`
- `hotspot_changes`

这让 explain surface 可以在不伪造结构变化的前提下，
直接回答“资源契约相对 baseline 漂移了什么”。

如果选择的是整组 compare report，
artifact_root 级 `-ResourceSummary -AsJson` 现在也会继续暴露
`query.comparison.resource_contract`，至少带出：

- `compared_case_count / changed_case_count / unchanged_case_count`
- `changed_cases / unchanged_cases`
- `summary_change_matrix`
- `contract_change_matrix`

这让资源解释面不只会横向看“哪些合同在哪些 case 中成立”，
还可以横向看：

- 哪些 case 的资源法律相对 baseline 发生漂移
- 哪条合同在多少 case 中发生 compare change
- summary drift 是否集中在少数几个 contract law 变化上

当前 `resource summary` 的最小解释方式是：

- 先把输入侧 `declared_contract_entries` 逐条展开
- 再把 `declared_facts`、`subject` 派生事实、图里实际提供的 fact，以及审计阶段命中的 fact 收束成事实库存
- 最后给出每条合同当前是 `satisfied / violated / unknown`，并带出对应热点

也就是说，v0 当前不是在做“完整资源证明”，
而是在做 explain surface 所需要的最小合法性面：

> **围绕当前系统，稳定回答“哪些资源合同被声明了、它们为什么成立或不成立、证据又来自哪里”。**

## 7. 当前推荐的协议分层

当前最健康的做法，不是再发明一套完全脱离现有工件链的新协议，  
而是沿着现有分层继续扩展。

建议先维持这样一组层次：

### 7.1 样例层

- `sample`

用途：

- 字段勘探
- 原型接入
- 快速结构观察

### 7.2 工件组织层

- `bundle`
- `report manifest`

用途：

- 批量导出结果组织
- 报告工件发现
- 上层工具稳定引用

### 7.3 比较与 CI 层

- `bundle diff`
- `ci summary`

用途：

- 增量变化分析
- 自动化审阅
- CI 状态汇总

### 7.4 解释层

- `artifact report`
- `explain surface`

用途：

- 把多种底层工件收束成统一的人类/工具问题面

这里最关键的边界是：

> **explain surface 应优先消费已经存在的稳定工件，而不是绕过工件层直接依赖内部实现。**

## 8. v0 的工程边界

当前最健康的推进方式是：

1. 先定义最小问题面
2. 先把现有报告工件收束成统一语义
3. 先让静态导出与少量 runtime 观察可以互相引用
4. 先服务报告、CI、脚本与审阅
5. 再逐步增强实时查询能力

这里说的“互相引用”，当前更推荐理解为：

- `materialized_graph` 保持静态结构事实
- runtime observe 通过 sidecar 保持动态观察事实
- `artifact report` 在统一对象里把两者收束起来

近期不建议：

- 直接承诺一个全局、实时、完整的运行中系统检查器
- 过早把所有查询格式冻结成终局协议
- 为了 explain 而给运行时塞入过重负担
- 跳过现有 `bundle / diff / report manifest` 体系另起炉灶

v0 更合理的判断标准是：

> **当人或工具追问“这个系统为什么长成这样、为什么现在不可用、为什么这里违法”时，Charm 能否比过去更稳定地给出结构化答案。**

## 9. 当前结论

`Explain Surface / Artifact Report v0` 当前不应理解成又一个平行子系统。  
它更像是 system compiler 把自己“说给外部世界听”的第一版语言：

- `artifact report` 让系统结果变成可引用工件
- `explain surface` 让这些工件变成可追问问题面
- `materialized_graph` 提供静态结构胚胎
- bringup evidence 与 resource contract 提供新的解释维度
- runtime transition 观察提供少量动态事实

因此这条线的近期目标可以收束成一句话：

> **先让 Charm 能把系统事实稳定吐出来、稳定被追问，再逐步把这套输出面做成更强的工具与运行时桥接层。**
