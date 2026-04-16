# Artifact Report v0

本文不是最终冻结的 JSON Schema，也不是新的导出脚本实现说明。  
它用于定义 Charm 在 `system compiler v0` 阶段的最小统一报告对象：`artifact report`。

当前 schema 草案与最小机器可验样例见：

- `schemas/system_compiler.artifact_report.v0.schema.json`
- `schemas/examples/system_compiler.artifact_report.v0.sample.json`
- `schemas/system_compiler.runtime_observe_snapshot.v0.schema.json`
- `schemas/examples/system_compiler.runtime_observe_snapshot.v0.sample.json`

当前可以直接这样校验样例：

```powershell
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_compiler.artifact_report.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_compiler.runtime_observe_snapshot.v0.sample.json
```

当前最小真实生成链脚本为：

- `scripts/export_system_compiler_artifact_report.ps1`
- `scripts/inspect_system_compiler_artifact_report.ps1`

它当前会基于现有 `export_bundle` index、可选 `materialized_graph.sample` 与可选 `runtime_observe` sidecar，
为每个 case 生成一份最小 `artifact report` JSON。
而 `inspect_system_compiler_artifact_report.ps1` 则提供了当前最小只读消费面，
用于把 case 级 `artifact report` 直接展开成人类可读摘要或机器继续消费的 JSON 视图。

当前这条链已经不再要求每个 case 都必须先落成静态 graph。
`export_bundle/v1` 现在可以同时承载两类 case：

- `materialized_graph`
  同时带出 `dot/json` 与可选 `runtime_observe` sidecar
- `runtime_only`
  只带出 `runtime_observe` sidecar，不强行伪造 graph 工件

如果调用方显式传入 `-Profile`、`-Board`、`-Facet`，
当前生成链也会把这些 subject 元数据写入报告对象。
如果 bundle 的 case entry 自带 `subject` 元数据，
当前导出脚本也会在没有显式 override 时自动继承它。
如果 bundle 的 case entry 自带 `declared_facts`，
当前导出脚本也会把它们写入 `structure.declared_facts`，
并与图推导出的 `required_facts` / `provided_facts` 保持分离。
如果 bundle 的 case entry 自带 `declared_contracts`，
当前导出脚本也会把这些输入合同写入 `resource_contract.declared_contract_entries`，
并基于 `declared_facts`、`subject` 派生事实与图提供能力产出最小 `provided / satisfied / violated / unknown` 摘要。
如果 bundle 顶层已经保留了输入 `manifest` provenance，
当前 `artifact report` 也会把它继续写入 `artifacts.input_manifest`。
如果某个 case 还额外声明了 `runtime_observe` sidecar，
当前导出脚本也会把它吸收到 `runtime_observe` 摘要里，
并继续把来源写入 `artifacts.runtime_observe`。
当前这个摘要至少会继续保留：

- `observed_capabilities`
- `publish_state_summary`
- `export_state_summary`
- `recent_transitions`

这条线当前刻意保持两层分离：

- `materialized_graph.sample`
  继续承载静态结构事实
- `runtime_observe` sidecar
  承载独立的动态观察事实

如果当前 case 还没有接入 sidecar，
`artifact report` 仍会保留稳定的 `runtime_observe` 形状，
但内容会诚实地保持为空摘要。

如果当前 case 属于 `runtime_only`，
`artifact report.artifacts.sample_json` 与 `artifact report.artifacts.dot` 会保持为空，
`structure.node_count / edge_count / materialized_order` 也会回落到零值或空数组；
但 `capability_count`、`bringup_evidence` 与 `runtime_observe` 仍会继续从 sidecar 收敛最小结论对象。

当前最小真实链路可以这样跑：

```powershell
./scripts/export_materialized_graph.ps1 -Case materialize-observe-demo -OutputRoot out/artifact-report-demo-bundle
./scripts/export_system_compiler_artifact_report.ps1 -BundleRoot out/artifact-report-demo-bundle -Case materialize-observe-demo -OutputRoot out/system-compiler-artifact-report-demo
python ./scripts/validate_materialized_graph_artifacts.py ./out/system-compiler-artifact-report-demo/materialize-observe-demo.artifact_report.json
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo -Case materialize-observe-demo -CapList
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo -CapList -AsJson
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo -Case materialize-observe-demo -GraphPath io.uart1
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo -Case materialize-observe-demo -RecentTransitions
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo -Case materialize-observe-demo -ResourceSummary
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo -Case materialize-observe-demo -BringupEvidence
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo -Case materialize-observe-demo -ShowArtifacts
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo -Case materialize-observe-demo -WhyCapability io.uart1
```

当前 runtime-only case 也已经可以走同一条正式链路，例如：

```powershell
./scripts/export_materialized_graph.ps1 -Case usb-host-runtime-multi-smoke -OutputRoot out/runtime-only-bundle
./scripts/export_system_compiler_artifact_report.ps1 -BundleRoot out/runtime-only-bundle -Case usb-host-runtime-multi-smoke -OutputRoot out/runtime-only-artifact-report
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/runtime-only-artifact-report -Case usb-host-runtime-multi-smoke -RecentTransitions
```

当前 inspector 至少会直接带出：

- case / mode / profile / board / facets
- `node_count / edge_count / unresolved_bindings`
- `declared_contract_entries / provided_facts / satisfied / violated / unknown`
- 最小 `cap list` 查询结果
- 最小 `graph path` 查询结果
- 最小 `recent transitions` 查询结果
- 最小 `resource summary` 查询结果
- 最小 `bringup evidence` 查询结果
- compare 模式下的 `summary_changes / metadata_changes / comparison.bringup_evidence / comparison.resource_contract`
- artifact_root 默认总览里的 compare 摘要
- 最小 `why unavailable` 查询结果
- 按需显示底层工件引用

其中 `cap list` 当前已经能在两种作用域上工作：

- 单 report 查询
- 全 artifact root 汇总

它当前最小稳定输出会围绕以下字段组织：

- `capability`
- `materialized / observed / published / required`
- `declared_fact / resource_fact / unresolved_binding`
- `provider_nodes / consumer_nodes`

如果当前 report 来自 compare 模式，
单 report 级 `cap list` 现在还会继续为每个 capability 带出最小 compare 摘要，
至少包括：

- `comparison.bringup_changed / comparison.bringup_change_kinds`
- `comparison.resource_changed / comparison.resource_change_kinds`
- `comparison.resource_contracts`

与此同时，
单 report 默认总览里的 `comparison` 现在也会继续带出一份最小 `capability_summary`，
至少包括：

- `comparison.capability_summary.compared_capability_count`
- `comparison.capability_summary.bringup_compare_capability_count / resource_compare_capability_count`
- `comparison.capability_summary.compared_capabilities`

如果选择的是整组 compare report，
artifact_root 级 `cap list` 现在也会继续带出：

- capability 级 `compare_cases / bringup_compare_cases / resource_compare_cases`
- capability 级 `bringup_change_kinds / resource_change_kinds`
- query 级 compare 摘要计数

这意味着当前 inspector 已经可以把“capability 分布”与“compare drift 分布”放进同一张表面。

当调用方显式选择多个 case 子集时，
当前 inspector 不支持直接对这类“部分 root”做 `cap list` 汇总，
以避免把单 case 证据和跨 case 聚合语义混在一起。

与此同时，
如果调用方直接读取 artifact_root 默认总览，
而所选报告又来自 compare 模式，
当前 JSON 总览也会额外带出一份最小 `comparison` 摘要，
至少回答：

- `compared_case_count`
- `metadata_changed_case_count`
- `bringup_changed_case_count`
- `resource_changed_case_count`
- `capability_summary.compared_capability_count`
- `capability_summary.bringup_compare_capability_count / resource_compare_capability_count`
- `capability_summary.compared_capabilities`

这意味着默认总览已经能直接回答：

> **这一组 compare report 里，到底有多少 case 真正在 compare 维度上发生了漂移。**

它现在还会继续给出一份最小 capability 热点入口，
用来先回答：

> **这些漂移主要集中在哪些 capability 上。**

而 `why unavailable` 当前则支持两种读取作用域：

- 单 report 查询
- 全 artifact root 汇总

如果当前 report 来自 compare 模式，
单 report 级该查询现在也会继续为目标 capability 带出最小 `comparison` 证据块，
至少包括：

- `comparison.changed`
- `comparison.bringup_changed / comparison.bringup_change_kinds`
- `comparison.resource_changed / comparison.resource_change_kinds`
- `comparison.resource_contracts`

如果选择的是整组 compare report，
artifact_root 级该查询现在也会继续带出：

- `state_counts`
- `compared_case_count / bringup_compare_case_count / resource_compare_case_count`
- `compared_cases / bringup_compare_cases / resource_compare_cases`
- `resource_contracts`

而 `graph path` 当前则明确只支持单 report 查询。
它当前最小稳定输出会围绕以下字段组织：

- `capability`
- `state / availability_state`
- `comparison`
- `direct_edges`
- `provider_paths`
- `consumer_paths`

如果当前 report 来自 compare 模式，
`graph path` 现在也会继续为目标 capability 带出最小 `comparison` 证据块，
至少包括：

- `comparison.changed`
- `comparison.bringup_changed / comparison.bringup_change_kinds`
- `comparison.resource_changed / comparison.resource_change_kinds`
- `comparison.resource_contracts`

其中：

- `state`
  当前优先表达图查询自身的语义状态，例如 `edge_paths / provider_terminal / required_without_provider / undeclared`
- `availability_state`
  则继续保留来自 `why unavailable` 的可用性判断，
  方便 explain 面与可用性面保持同一套语言

而 `resource summary` 当前同样明确只支持单 report 查询。
它当前最小稳定输出会围绕以下字段组织：

- `declared_contracts / audited_count / satisfied_count / violated_count / unknown_count`
- `fact_inventory`
- `contracts`
- `resource_hotspots`

其中：

- `fact_inventory`
  当前至少区分 `declared_facts / subject_facts / graph_provided_facts / audit_provided_facts`
- `contracts`
  则把每条输入侧合同压成稳定查询结果，
  至少带出 `state / requires / present_facts / missing_facts / fact_sources`

而 `recent transitions` 当前也明确只支持单 report 查询。
它当前最小稳定输出会围绕以下字段组织：

- `observed_capabilities`
- `publish_state_summary`
- `export_state_summary`
- `transition_count`
- `transition_capabilities`
- `action_counts`
- `transitions`

如果当前 case 已接入 `runtime_observe` sidecar，
这里会返回真实的 runtime export 观察摘要；
如果尚未接入 sidecar，
则继续返回稳定形状，但结果为空。

如果当前 report 来自 compare 模式，
`recent transitions` 现在也会继续为“出现在 transition 列表里的 capability”
带出最小 compare 摘要。

其中 `query.result.comparison` 当前至少包括：

- `compared_transition_count / bringup_compare_transition_count / resource_compare_transition_count`
- `compared_capability_count / bringup_compare_capability_count / resource_compare_capability_count`
- `compared_capabilities`
- `bringup_change_kinds / resource_change_kinds`
- `resource_contracts`

与此同时，`query.result.transitions[*]` 当前至少继续带出：

- `comparison.bringup_changed / comparison.bringup_change_kinds`
- `comparison.resource_changed / comparison.resource_change_kinds`
- `comparison.resource_contracts`

这意味着 compare 模式下，
`recent transitions` 现在可以直接回答：

> **最近这批 runtime export 切换里，哪些 capability 既发生了切换，也在 compare 维度上发生了漂移。**

但它仍保持一个边界：

- 不做 artifact_root 聚合
- 不把未出现在 `recent_transitions` 里的 compare capability 强行混入 runtime 视图

当前仓库里，`Examples/usb/usb_host_runtime_multi_smoke` 已经作为第一条正式
`runtime_only` case 接入 `export_case_manifest -> export_bundle -> artifact_report`，
并稳定产出一份真实 `system_compiler.runtime_observe_snapshot/v0` sidecar。
这个示例会在场景末尾执行 `remove + forget + unexport`，
因此导出结果里的 `published_capabilities` 为空、
`publish_state_summary` 与 `export_state_summary` 都会收敛到 `missing` 末态。
这里要把它理解为“终态摘要 + 最近历史”：

- `publish_state_summary / export_state_summary`
  反映导出时刻的最终状态
- `recent_transitions`
  保留场景中最近的真实 attach/detach/unexport 历史

另外，`Examples/usb/usb_msc_block_demo` 现在已经成为第一条正式接入
`export_case_manifest -> export_bundle -> artifact_report` 的 graph case。
它导出的 sidecar 反映的是 bringup 完成后的真实末态：

- `published_capabilities = ["block.sd0"]`
- `publish_state_summary.published = 1`
- `recent_transitions = []`

这意味着当前仓库里已经同时存在两类真实 producer：

- `usb_host_runtime_multi_smoke`
  更适合演示 transition-rich `runtime_only` case
- `usb_msc_block_demo`
  更适合演示 graph case + runtime sidecar 一起进入 bundle / artifact report 的真实末态摘要

也就是说，它当前回答的不是“完整运行时事件历史”，
而是：

> **当前 artifact report 已经稳定保留了哪些最近状态切换，以及这些切换在 publish/export 语义里呈现成什么摘要。**

当前 `scripts/ci_materialized_graph_bundle.ps1` 也已经能在生成 `summary.json` 时同步产出 candidate 侧的 `artifact report`，并把这些报告路径写回 CI 摘要。
当 CI 调用方提供 `-Profile`、`-Board`、`-Facet` 时，
这些默认 subject 元数据也会继续透传到 case 级 `artifact report`。
如果调用方没有显式提供这些参数，
当前 CI 摘要也会在所选 case 的 bundle `subject` 一致时自动提炼出 `subject_defaults`。

它要回答的核心问题不是“有哪些零散导出文件”，而是：

> **当一次系统编译、bringup 举证与资源审计完成后，Charm 应该把哪些核心事实收束成一个可引用对象。**

在当前主线上，`artifact report` 的定位很明确：

- 它不是 explain surface 的替代品
- 它不是底层 bundle / diff / manifest 的替代品
- 它是把这些结果汇总成“系统本次产出了什么结论”的统一工件

## 1. 为什么需要单独定义 `artifact report`

当前仓库已经有很多很强的输出面胚胎：

- `materialized_graph.sample`
- `export_bundle`
- `bundle_diff`
- `ci_summary`
- `report_manifest`
- bringup evidence / resource contract 这两条新文档主线

但它们目前更多回答的是：

- 某个局部工件长什么样
- 某条导出链怎么消费
- 某次 diff 和 CI 摘要如何组织

它们还没有单独回答下面这个更上位的问题：

> **如果把这次系统结果当成一个完整对象，它的最小摘要应该长什么样。**

这正是 `artifact report` 的职责。

没有这一层时，系统结果很容易再次退化成：

- 一堆散文件
- 一堆分散脚本输出
- 一堆只有作者自己知道怎么串起来的观察结果

而有了这一层，Charm 才更像一个真正的 system compiler：

> **它不仅能产工件，还能产“关于这些工件的统一结论对象”。**

## 2. `artifact report` 在输出面里的位置

当前可以把输出面理解为四层：

### 2.1 样例层

- `sample`

用途：

- 字段勘探
- 原型接入
- 快速结构观察

### 2.2 工件组织层

- `bundle`
- `report manifest`

用途：

- 工件归档
- 多文件组织
- 报告文件发现

### 2.3 比较与 CI 层

- `bundle diff`
- `ci summary`

用途：

- 增量变化分析
- CI 摘要
- 自动化状态汇总

### 2.4 汇总结论层

- `artifact report`

用途：

- 把静态结构、bringup 证据、资源审计与支持工件收束成统一摘要对象

因此 `artifact report` 更像：

> **report-of-reports**

它消费已有工件，但不试图取代这些工件。

## 3. v0 明确不做什么

当前版本明确不做：

- 不试图一份报告塞下所有细节
- 不复制所有底层工件全文
- 不承诺当前字段已经是长期冻结协议
- 不要求所有子系统一次性接入
- 不跳过既有 `bundle / diff / manifest` 体系重做一套新管线

当前更重要的是先把这件事做对：

> **定义出一个足够小、足够稳、足够可引用的最小统一报告对象。**

## 4. v0 的最小对象边界

当前建议把 `artifact report v0` 理解为一个只读汇总对象。  
它应满足下面几个特征：

- 以“这次系统结果”为中心，而不是以单个工件为中心
- 只保留最关键的摘要字段
- 引用底层工件，而不是吞掉底层工件
- 同时覆盖静态装配、bringup 证据与资源审计三张面
- 让上层 explain surface 有稳定输入锚点

换句话说，它不应是“大而全数据库”，  
而应是：

> **最小系统结论页。**

## 5. v0 建议字段分组

当前建议把 `artifact report` 分成八组字段。

### 5.1 报告身份

这一组回答：

> “这份报告是谁、什么时候、按什么模式生成的。”

建议至少包含：

- `schema`
- `generated_at_utc`
- `generator`
- `report_kind`
- `mode`

其中：

- `report_kind`
  当前可先固定为 `system_compiler.artifact_report`
- `mode`
  可用于区分：
  - `export_only`
  - `compare`
  - 其它未来模式

### 5.2 系统上下文

这一组回答：

> “这份报告针对的是哪个系统实例。”

建议至少包含：

- `case`
- `profile`
- `board`
- `active_facets`

这里不要求所有字段立即 100% 完备，  
但 v0 应先把这些对象正式列为报告语言的一部分。

### 5.3 静态结构摘要

这一组回答：

> “系统静态装配之后，长成了什么样。”

建议至少包含：

- `capability_count`
- `node_count`
- `edge_count`
- `materialized_order`
- `declared_facts`
- `required_facts`
- `unresolved_bindings`

其中：

- `materialized_order`
  可先只放节点顺序摘要或引用外部工件
- `declared_facts`
  应保留来自输入 `manifest` 的显式声明事实，
  不与图推导得到的 `required_facts` / `provided_facts` 混写
- `required_facts`
  应是从当前 `materialized_graph` 结构推导出的需求事实
- `unresolved_bindings`
  则是最关键的未完成结构结论之一

### 5.4 bringup 证据摘要

这一组回答：

> “bringup 过程在证据语言里当前是什么结论。”

建议至少包含：

- `declared_count`
- `materialized_count`
- `published_count`
- `observed_count`
- `blocked_count`
- `failed_count`

以及按需包含：

- `published_capabilities`
- `blocked_reasons`
- `failed_reasons`
- `evidence_entries`

这里不要求 v0 就把所有节点细节内嵌进报告，  
但应让顶层摘要一眼能看出：

> 当前 bringup 问题到底是“没成立”、还是“成立了但没发布”、还是“已经失败”。 

当 `evidence_entries` 存在时，
它应至少能稳定回答每个 capability 当前是否已经：

- `declared`
- `materialized`
- `published`
- `observed`
- `blocked`
- `failed`

并保留：

- `publish_state / export_state`
- `provider_nodes / consumer_nodes`
- capability 级 `blocked_reasons / failed_reasons`

这里要特别注意，`observed_count` 当前表达的是 capability 级证据矩阵里的 `observed` 结论数，
而不只是 `runtime_observe.observed_capabilities` 的条目数。
对于 graph case，它现在允许把 `materialized_graph` 的稳定观察结果一并算入 `observed`。

### 5.5 资源契约摘要

这一组回答：

> “系统在当前资源宇宙里，合法性结论如何。”

建议至少包含：

- `declared_contracts`
- `declared_contract_entries`
- `provided_facts`
- `audited_count`
- `satisfied_count`
- `violated_count`
- `unknown_count`

以及按需包含：

- `satisfied_contracts`
- `violations`
- `unknown_contracts`
- `resource_hotspots`

这里建议特别优先覆盖：

- `may_block`
- `needs_heap`
- `needs_reactor`
- `needs_monotonic_clock`
- `irq_safe`

当前 v0 里，`declared_contract_entries` 更适合作为输入侧法律文本的直接投影，
而 `provided_facts / satisfied / violated / unknown` 则是最小审计层。

### 5.6 运行时观察摘要

这一组回答：

> “当前有哪些最小 runtime 事实已经进入稳定观察面。”

建议至少包含：

- `publish_state_summary`
- `export_state_summary`
- `recent_transitions`

这组字段当前不宜做得过大，  
因为 v0 还不是完整 runtime inspector。  
但它至少应该让报告能引用：

- `PublishState`
- `ExportState`
- `ExportTransition`

这些已经存在的观察语言。

### 5.7 比较摘要（compare 模式可选）

这一组回答：

> “如果当前报告来自 compare 模式，这个 case 相对 baseline 到底发生了什么。”

建议至少包含：

- `status`
- `summary_changes`
- `metadata_changes`
- `node_changes`
- `edge_changes`
- `bringup_evidence`
- `resource_contract`

这组字段不应取代底层 `bundle_diff`，
但它应该把 case 级最重要的比较结论直接拉到报告顶层。
对于 metadata-only diff，当前 `status` 仍可保持 `unchanged`，
但 `metadata_changes` 不应被吞掉。
而 `comparison.bringup_evidence` 则用于承载“结构未变但 bringup 证据发生漂移”的比较面，
避免把 sidecar / published 状态变化误塞回结构 diff 语义。

`comparison.bringup_evidence` 当前建议至少包含：

- `changed`
- `left / right`
- `summary_changes`
- `capability_changes`
- `published_capability_changes`

这意味着 compare 模式下即使顶层 `status = unchanged`，
报告仍然可以明确回答 baseline/candidate 的 bringup 证据是否发生漂移，
尤其是哪些 capability 在 `missing / published` 与 `missing / detached / attached` 之间发生切换。

而 `comparison.resource_contract` 则用于承载“结构未变但资源法律发生漂移”的比较面，
避免把资源契约变更误塞回结构 diff 语义。

`comparison.resource_contract` 当前建议至少包含：

- `changed`
- `left / right`
- `summary_changes`
- `contract_changes`
- `provided_fact_changes`
- `hotspot_changes`

这意味着 compare 模式下即使顶层 `status = unchanged`，
报告仍然可以明确回答 baseline/candidate 的资源契约是否发生漂移，
以及是哪条合同从 `absent / satisfied / violated / unknown` 之间切换了状态。

### 5.8 支持工件引用

这一组回答：

> “如果我要继续深挖，应该去看哪些底层工件。”

建议至少包含：

- `bundle`
- `input_manifest`
- `dot`
- `sample_json`
- `runtime_observe`
- `diff`
- `ci_summary`
- `report_manifest`
- `report_markdown`
- `report_html`

其中每个引用建议都以：

- 相对路径
- 或可解析引用键

形式出现，而不是把大块内容直接内嵌进顶层报告。

## 6. v0 的最小样例形状

当前更适合先用一个“语义样例”来固定对象边界，而不是急着冻结正式 schema。

一个收敛后的最小样例可以长这样：

```json
{
  "schema": "system_compiler.artifact_report/v0",
  "generated_at_utc": "2026-04-15T10:30:00Z",
  "generator": "charm.system_compiler",
  "report_kind": "system_compiler.artifact_report",
  "mode": "compare",
  "subject": {
    "case": "bringup-minimal-observe-demo",
    "profile": "MCU_MIN",
    "board": "stm32_stub",
    "active_facets": ["runtime", "input"]
  },
  "structure": {
    "capability_count": 12,
    "node_count": 9,
    "edge_count": 8,
    "declared_facts": ["board.stm32_stub"],
    "required_facts": ["platform.irq", "system.clock"],
    "unresolved_bindings": []
  },
  "bringup_evidence": {
    "declared_count": 12,
    "materialized_count": 12,
    "published_count": 3,
    "observed_count": 12,
    "blocked_count": 0,
    "failed_count": 0,
    "evidence_entries": [
      {
        "capability": "io.uart1",
        "declared": true,
        "materialized": true,
        "published": true,
        "observed": true,
        "blocked": false,
        "failed": false,
        "publish_state": "published",
        "export_state": "attached",
        "provider_nodes": ["io.uart1"],
        "consumer_nodes": [],
        "blocked_reasons": [],
        "failed_reasons": []
      }
    ]
  },
  "resource_contract": {
    "declared_contracts": 4,
    "declared_contract_entries": [
      {"contract": "needs_monotonic_clock", "requires": ["system.clock"]},
      {"contract": "needs_reactor", "requires": ["io.reactor"]},
      {"contract": "may_block", "requires": ["execution.may_block"]},
      {"contract": "irq_safe", "requires": []}
    ],
    "provided_facts": ["system.clock", "io.reactor", "execution.may_block"],
    "audited_count": 4,
    "satisfied_count": 3,
    "violated_count": 0,
    "unknown_count": 1,
    "satisfied_contracts": [
      "needs_monotonic_clock requires [system.clock]",
      "needs_reactor requires [io.reactor]",
      "may_block requires [execution.may_block]"
    ],
    "unknown_contracts": ["irq_safe requires []"],
    "resource_hotspots": ["irq_safe requires []"]
  },
  "runtime_observe": {
    "publish_state_summary": {"published": 3, "missing": 0},
    "export_state_summary": {"attached": 1, "detached": 2, "missing": 0},
    "recent_transitions": [
      {
        "capability": "io.uart1",
        "action": "attach",
        "before": "detached",
        "after": "attached"
      }
    ]
  },
  "comparison": {
    "status": "unchanged",
    "summary_changes": [],
    "metadata_changes": [
      "declared_contracts:[needs_monotonic_clock requires [system.clock]]->[needs_heap requires [system.heap], needs_monotonic_clock requires [system.clock]]"
    ],
    "node_changes": {"added": 0, "removed": 0, "changed": 0},
    "edge_changes": {"added": 0, "removed": 0},
    "bringup_evidence": {
      "changed": true,
      "left": {
        "declared_count": 12,
        "materialized_count": 12,
        "published_count": 0,
        "observed_count": 12,
        "blocked_count": 0,
        "failed_count": 0,
        "published_capabilities": [],
        "observed_capabilities": []
      },
      "right": {
        "declared_count": 12,
        "materialized_count": 12,
        "published_count": 1,
        "observed_count": 12,
        "blocked_count": 0,
        "failed_count": 0,
        "published_capabilities": ["io.uart1"],
        "observed_capabilities": ["io.uart1"]
      },
      "summary_changes": [
        "published_count:0->1"
      ],
      "published_capability_changes": {
        "added": ["io.uart1"],
        "removed": []
      },
      "capability_changes": [
        {
          "capability": "io.uart1",
          "change_kind": "changed",
          "left_declared": true,
          "right_declared": true,
          "left_materialized": true,
          "right_materialized": true,
          "left_observed": true,
          "right_observed": true,
          "left_published": false,
          "right_published": true,
          "left_blocked": false,
          "right_blocked": false,
          "left_failed": false,
          "right_failed": false,
          "left_publish_state": "missing",
          "right_publish_state": "published",
          "left_export_state": null,
          "right_export_state": "attached"
        }
      ]
    },
    "resource_contract": {
      "changed": true,
      "left": {
        "declared_contracts": 1,
        "audited_count": 1,
        "satisfied_count": 1,
        "violated_count": 0,
        "unknown_count": 0,
        "provided_facts": ["system.clock"],
        "resource_hotspots": []
      },
      "right": {
        "declared_contracts": 2,
        "audited_count": 2,
        "satisfied_count": 1,
        "violated_count": 1,
        "unknown_count": 0,
        "provided_facts": ["system.clock"],
        "resource_hotspots": ["needs_heap missing [system.heap] requires [system.heap]"]
      },
      "summary_changes": [
        "declared_contracts:1->2",
        "violated_count:0->1"
      ],
      "contract_changes": [
        {
          "contract": "needs_heap",
          "change_kind": "added",
          "left_state": "absent",
          "right_state": "violated",
          "left_requires": [],
          "right_requires": ["system.heap"],
          "left_status_text": null,
          "right_status_text": "needs_heap missing [system.heap] requires [system.heap]"
        }
      ],
      "provided_fact_changes": {"added": [], "removed": []},
      "hotspot_changes": {
        "added": ["needs_heap missing [system.heap] requires [system.heap]"],
        "removed": []
      }
    }
  },
  "artifacts": {
    "bundle": "out/materialized-graph-bundle/index.json",
    "input_manifest": "scripts/materialized_graph.export_case_manifest.v1.json",
    "dot": "out/materialized-graph-bundle/case/materialized_graph.dot",
    "sample_json": "out/materialized-graph-bundle/case/materialized_graph.sample.json",
    "runtime_observe": "out/materialized-graph-bundle/case/runtime_observe.snapshot.json",
    "diff": "out/report/materialized_graph_bundle.diff.json",
    "ci_summary": null,
    "report_manifest": "out/report/materialized_graph_bundle_diff_report.manifest.json",
    "report_markdown": "out/report/materialized_graph_bundle_diff_report.md",
    "report_html": "out/report/materialized_graph_bundle_diff_report.html"
  }
}
```

这个样例的重点不是字段名已经最终拍板，
而是先把对象轮廓固定住：

- 顶层是统一报告对象
- 中层是若干摘要分组
- 底层通过 `artifacts` 继续引用原始工件

在 `export_only` 模式下，`comparison` 可以省略；
在 `compare` 模式下，建议把它视为 case 级比较结论的第一摘要面。

## 7. 与其它文档的关系

### 7.1 与 `explain_surface_v0`

`artifact report` 是 explain surface 的重要输入之一，
但两者不等价。

- `artifact report`
  解决“系统当前产出了什么结论对象”
- `explain surface`
  解决“人和工具如何继续追问这些结论”

### 7.2 与 `bringup_evidence_pipeline_v0`

bringup 文档负责定义：

- 状态语言
- 胚胎映射
- 证据边界

而 `artifact report` 负责把这些状态压成：

- 顶层摘要字段
- 可引用结论对象

### 7.3 与 `resource_contract_v0`

资源契约文档负责定义：

- 法律文本
- 审计语言
- 合法性边界

而 `artifact report` 负责把这些审计结果压成：

- `satisfied / violated / unknown` 摘要
- 可进一步追问的热点入口

### 7.4 与现有 `bundle / diff / report manifest`

`artifact report` 应消费这些工件，  
而不应跳过它们另起一套闭环。

这条边界必须守住，因为它能保证：

- 现有工具链继续有价值
- 报告体系逐步长成，而不是重来
- explain surface 有稳定工件锚点

## 8. v0 的工程边界

当前最健康的推进方式是：

1. 先冻结对象边界
2. 先冻结最小摘要字段
3. 先用文档和样例固定语义
4. 再决定哪些字段值得进入真实 schema
5. 再决定哪些摘要值得接进 CI / 工具链

近期不建议：

- 一上来把它做成大而全总报告
- 一上来要求所有底层模块都直接生成这份报告
- 过早承诺所有字段长期稳定
- 把它做成对现有脚本链不兼容的新世界

v0 更合理的判断标准是：

> **当我们拿到一次系统导出结果时，是否能通过一份统一对象先看懂“这次结果最重要的事实是什么”，然后再顺着引用继续深挖。**

## 9. 当前结论

`Artifact Report v0` 当前应被理解成 system compiler 的最小“结论对象”，而不是新的大平台。

它的近期使命很克制：

- 不替代底层工件
- 不替代 explain surface
- 不替代 bringup 与资源契约文档

它只做一件很关键的事：

> **把分散的结构、证据、合法性与观察结果，先收成一个稳定可引用的系统结论页。**
