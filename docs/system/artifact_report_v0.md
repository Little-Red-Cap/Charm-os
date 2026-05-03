# Artifact Report v0

本文不是最终冻结的 JSON Schema，也不是新的导出脚本实现说明。  
它用于定义 Charm 在 `system compiler v0` 阶段的最小统一报告对象：`artifact report`。

当前 schema 草案与最小机器可验样例见：

- `schemas/system_compiler.artifact_report.v0.schema.json`
- `schemas/system_compiler.artifact_report_index.v0.schema.json`
- `schemas/examples/system_compiler.artifact_report.v0.sample.json`
- `schemas/examples/system_compiler.artifact_report_index.v0.sample.json`
- `schemas/examples/system_compiler.artifact_report.v0.i2c_facts.sample.json`
- `schemas/system_compiler.fact_evidence.v0.schema.json`
- `schemas/examples/system_compiler.fact_evidence.v0.i2c_facts.sample.json`
- `schemas/examples/system_compiler.fact_evidence.v0.board_facts.sample.json`
- `schemas/examples/system_compiler.fact_evidence.v0.board_i2c_composition.sample.json`
- `schemas/system_compiler_summary.v0.schema.json`
- `schemas/examples/system_compiler_summary.summary.v0.sample.json`
- `schemas/examples/system_compiler_summary.comparison.v0.sample.json`
- `schemas/system_input_summary.v0.schema.json`
- `schemas/examples/system_input_summary.summary.v0.sample.json`
- `schemas/examples/system_input_summary.comparison.v0.sample.json`
- `schemas/binding_result_summary.v0.schema.json`
- `schemas/examples/binding_result_summary.summary.v0.sample.json`
- `schemas/examples/binding_result_summary.comparison.v0.sample.json`
- `schemas/bringup_order_summary.v0.schema.json`
- `schemas/examples/bringup_order_summary.summary.v0.sample.json`
- `schemas/examples/bringup_order_summary.comparison.v0.sample.json`
- `schemas/system_formation_summary.v0.schema.json`
- `schemas/examples/system_formation_summary.summary.v0.sample.json`
- `schemas/examples/system_formation_summary.comparison.v0.sample.json`
- `schemas/fact_resolution_summary.v0.schema.json`
- `schemas/examples/fact_resolution_summary.summary.v0.sample.json`
- `schemas/examples/fact_resolution_summary.comparison.v0.sample.json`
- `schemas/system_compiler.runtime_observe_snapshot.v0.schema.json`
- `schemas/examples/system_compiler.runtime_observe_snapshot.v0.sample.json`

当前可以直接这样校验样例：

```powershell
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_compiler.artifact_report.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_compiler.artifact_report_index.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_compiler.artifact_report.v0.i2c_facts.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_compiler.fact_evidence.v0.i2c_facts.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_compiler.fact_evidence.v0.board_facts.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_compiler.fact_evidence.v0.board_i2c_composition.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_compiler_summary.summary.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_compiler_summary.comparison.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_input_summary.summary.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_input_summary.comparison.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/binding_result_summary.summary.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/binding_result_summary.comparison.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/bringup_order_summary.summary.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/bringup_order_summary.comparison.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_formation_summary.summary.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_formation_summary.comparison.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/fact_resolution_summary.summary.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/fact_resolution_summary.comparison.v0.sample.json
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_compiler.runtime_observe_snapshot.v0.sample.json
```

当前最小真实生成链脚本为：

- `scripts/export_system_compiler_artifact_report.ps1`
- `scripts/inspect_system_compiler_artifact_report.ps1`
- `scripts/materialized_graph_required_fact_resolution_matrix_smoke.ps1`
- `scripts/materialized_graph_required_fact_resolution_compare_smoke.ps1`

它当前会基于现有 `export_bundle` index、可选 `materialized_graph.sample`、可选 `runtime_observe` sidecar
与可选 `fact_evidence` sidecar，
为每个 case 生成一份最小 `artifact report` JSON。
当调用方通过 `-OutputRoot` 导出一组 report 时，
脚本还会在 root 下生成一份轻量入口：

- `index.json`
- `schema = system_compiler.artifact_report_index/v0`

这份 index 不是新的大总报告。
它只把“第一眼判断”和“继续深挖的路径”放在 root 入口，
让 CI、IDE 原型或外部脚本不用先打开所有 case 级 full report，
也能快速知道当前 formation 状态、compare drift 维度、阻塞热点与 report 文件位置。
如果调用方使用 `-OutputPath` 只导出单个 report，
当前不会额外生成 root index。
而 `inspect_system_compiler_artifact_report.ps1` 则提供了当前最小只读消费面，
用于把 case 级 `artifact report` 直接展开成人类可读摘要或机器继续消费的 JSON 视图。
`materialized_graph_required_fact_resolution_matrix_smoke.ps1` 则用于钉住 artifact-root 级
`required_fact_resolution_matrix` 的横向聚合语义。
`materialized_graph_required_fact_resolution_compare_smoke.ps1` 则用于钉住 compare 模式下
`required_fact_resolution_changes` 与 `required_fact_resolution_change_matrix` 的漂移语义。

当前这条链已经不再要求每个 case 都必须先落成静态 graph。
`export_bundle/v1` 现在可以同时承载三类 case：

- `materialized_graph`
  同时带出 `dot/json` 与可选 `runtime_observe` sidecar
- `runtime_only`
  只带出 `runtime_observe` sidecar，不强行伪造 graph 工件
- `fact_only`
  可以只携带事实输入 / 审计事实 / `fact_evidence` sidecar，不强行伪造 graph 或 runtime sidecar

如果调用方显式传入 `-Profile`、`-Board`、`-Facet`，
当前生成链也会把这些 subject 元数据写入报告对象。
如果 bundle 的 case entry 自带 `subject` 元数据，
当前导出脚本也会在没有显式 override 时自动继承它。
如果 bundle 的 case entry 自带 `declared_facts`，
当前导出脚本也会把它们写入 `structure.declared_facts`，
并与图推导出的 `required_facts` / `provided_facts` 保持分离。
如果 bundle 的 case entry 自带 `required_facts` 与 `audit_provided_facts`，
当前导出脚本会把它们并入 `structure.required_facts`、
`resource_contract.provided_facts` 与 `fact_resolution.fact_inventory`；
其中 required 但未 available 的事实会自动进入 `resource_hotspots`，
但仍只作为报告结论，不阻断构建。
如果 bundle 的 case entry 自带 `fact_evidence` sidecar，
当前报告脚本会先校验 sidecar schema，
再把来源写入 `artifacts.fact_evidence`；
其中 facts 投影仍通过 bundle case entry 上的
`declared_facts / required_facts / audit_provided_facts` 进入现有报告字段。
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
- `fact_evidence` sidecar
  承载 contract-local 或 board/package-local 的事实证据投影

如果当前 case 还没有接入 sidecar，
`artifact report` 仍会保留稳定的 `runtime_observe` 形状，
但内容会诚实地保持为空摘要。

如果当前 case 属于 `runtime_only`，
`artifact report.artifacts.sample_json` 与 `artifact report.artifacts.dot` 会保持为空，
`structure.node_count / edge_count / materialized_order` 也会回落到零值或空数组；
但 `capability_count`、`bringup_evidence` 与 `runtime_observe` 仍会继续从 sidecar 收敛最小结论对象。
如果当前 case 属于 `fact_only`，
`artifact report.artifacts.sample_json`、`artifact report.artifacts.dot`
与 `artifact report.artifacts.runtime_observe` 都会保持为空；
但 `declared / required / audit_provided` facts 仍会进入事实库存与资源热点计算。
如果它来自 `fact_evidence` sidecar，
则 `artifacts.fact_evidence` 会保留对应证据文件路径。

当前最小真实链路可以这样跑：

```powershell
./scripts/export_materialized_graph.ps1 -Case materialize-observe-demo -OutputRoot out/artifact-report-demo-bundle
./scripts/export_system_compiler_artifact_report.ps1 -BundleRoot out/artifact-report-demo-bundle -Case materialize-observe-demo -OutputRoot out/system-compiler-artifact-report-demo
python ./scripts/validate_materialized_graph_artifacts.py ./out/system-compiler-artifact-report-demo/materialize-observe-demo.artifact_report.json
Get-Content -Raw -Encoding UTF8 ./out/system-compiler-artifact-report-demo/index.json | ConvertFrom-Json
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo -Case materialize-observe-demo -CapList
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo -CapList -AsJson
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo -Case materialize-observe-demo -GraphPath io.uart1
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo -Case materialize-observe-demo -RecentTransitions
./scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot out/system-compiler-artifact-report-demo -Case materialize-observe-demo -ShowTransitions
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

当前 board/package fact-only case 也已经可以走同一条正式链路，例如：

```powershell
./scripts/export_materialized_graph.ps1 -Case board-package-facts-smoke -OutputRoot out/board-facts-bundle
./scripts/export_system_compiler_artifact_report.ps1 -BundleRoot out/board-facts-bundle -Case board-package-facts-smoke -OutputRoot out/board-facts-artifact-report
python ./scripts/validate_materialized_graph_artifacts.py --bundle-root ./out/board-facts-bundle ./out/board-facts-artifact-report/board-package-facts-smoke.artifact_report.json
```

当前多来源 fact composition 也已经可以走同一条正式链路，例如：

```powershell
./scripts/export_materialized_graph.ps1 -Case board-i2c-fact-composition-smoke -OutputRoot out/board-i2c-composition-bundle
./scripts/export_system_compiler_artifact_report.ps1 -BundleRoot out/board-i2c-composition-bundle -Case board-i2c-fact-composition-smoke -OutputRoot out/board-i2c-composition-artifact-report
python ./scripts/validate_materialized_graph_artifacts.py --bundle-root ./out/board-i2c-composition-bundle ./out/board-i2c-composition-artifact-report/board-i2c-fact-composition-smoke.artifact_report.json
```

当前 inspector 至少会直接带出：

- case / mode / profile / board / facets
- `system_input`
- `system_formation`
- `node_count / edge_count / unresolved_bindings`
- `binding_result / bringup_order`
- `declared_contract_entries / provided_facts / satisfied / violated / unknown`
- `fact_resolution`
- 最小 `cap list` 查询结果
- 最小 `graph path` 查询结果
- 最小 `recent transitions` 查询结果
- 最小 `resource summary` 查询结果
- 最小 `bringup evidence` 查询结果
- compare 模式下的 `summary_changes / metadata_changes / comparison.system_input / comparison.system_formation / comparison.binding_result / comparison.bringup_order / comparison.bringup_evidence / comparison.resource_contract / comparison.fact_resolution`
- artifact_root 默认总览里的 `system_compiler_summary / system_input_summary / binding_result_summary / bringup_order_summary / system_formation_summary / fact_resolution_summary`
- artifact_root 默认总览里的 `comparison.system_compiler_summary / comparison.system_input_summary / comparison.binding_result_summary / comparison.bringup_order_summary / comparison.system_formation_summary / comparison.fact_resolution_summary`
- artifact_root 默认总览里的 compare 摘要
- 最小 `why unavailable` 查询结果
- 按需显示底层工件引用

当前也已经有一份 I2C device contract facts 的 artifact report 样例：

- `schemas/examples/system_compiler.artifact_report.v0.i2c_facts.sample.json`
- `schemas/examples/system_compiler.fact_evidence.v0.i2c_facts.sample.json`

这份样例现在与 `i2c-device-contract-facts-smoke` 这条 `fact_only`
导出链保持同一种投影语义，并通过 `fact_evidence` sidecar
把 `io.device_i2c_facts` 的 contract-local facts 带入 bundle / artifact report。
它的作用是把 `io.device_i2c_facts` 如何进入现有
`fact_resolution.fact_inventory`、`structure.required_facts`
与 `resource_contract.provided_facts` 的形状钉住。
其中 `pinmux:pb8/pb9.af4` 故意保留为 required 但未 available，
用于展示 contract-local fact 缺口如何被 artifact report 表达。

当前也已经有一份 board/package facts 的 sidecar 样例：

- `schemas/examples/system_compiler.fact_evidence.v0.board_facts.sample.json`
- `schemas/examples/system_compiler.fact_evidence.v0.board_i2c_composition.sample.json`

这份样例与 `board-package-facts-smoke` 这条 `fact_only` 导出链保持同一种投影语义，
但来源从 contract-local I2C facts 换成 `platform.board_facts` 对 `BoardCaps`
当前事实载体的只读投影。
它的作用是证明 `fact_evidence` 是 system compiler 的通用事实证据入口，
而不是 I2C contract 的专用旁路。

其中 `board_i2c_composition` 样例进一步把 `io.device_i2c_facts`
的 contract-required facts 与 `platform.board_facts` / adapter 提供的
audit facts 放进同一份 sidecar，展示 `fact_resolution.fact_inventory`
如何从“列出缺口”推进到“解释 required facts 被哪些来源满足”。

为了避免 inspector 继续在“支持哪些查询 / 哪些 scope / 哪些边界”上漂移，
当前也需要把它压成一张更具体的支持矩阵。
这张矩阵现在已经由：

- `scripts/system_compiler_explain_surface_contract_smoke.ps1`
- `scripts/materialized_graph_bringup_evidence_compare_smoke.ps1`
- `scripts/materialized_graph_bringup_evidence_compare_root_smoke.ps1`
- `scripts/materialized_graph_resource_contract_compare_smoke.ps1`
- `scripts/materialized_graph_resource_contract_compare_root_smoke.ps1`
- `scripts/materialized_graph_system_input_compare_smoke.ps1`
- `scripts/materialized_graph_system_formation_compare_smoke.ps1`

一起冻结成 v0 契约。

| inspector 入口 | 单 report / export_only | 单 report / compare | artifact_root / export_only | artifact_root / compare | 备注 |
| --- | --- | --- | --- | --- | --- |
| 默认总览 | 支持 | 支持 | 支持 | 支持 | `-Case` 为空时读取整 root；显式多 case 子集也继续返回 artifact_root 聚合摘要 |
| `-CapList` | 支持 | 支持 | 支持 | 支持 | 只接受精确单 report 或整 root；显式多 case 子集直接拒绝 |
| `-WhyCapability <cap>` | 支持 | 支持 | 支持 | 支持 | 只接受精确单 report 或整 root；显式多 case 子集直接拒绝 |
| `-GraphPath <cap>` | 支持 | 支持 | 不支持 | 不支持 | 必须精确命中一个 artifact report |
| `-RecentTransitions` | 支持 | 支持 | 不支持 | 不支持 | 必须精确命中一个 artifact report |
| `-BringupEvidence` | 支持 | 支持 | 支持 | 支持 | root 侧允许整 root 或显式多 case 子集聚合 |
| `-ResourceSummary` | 支持 | 支持 | 支持 | 支持 | root 侧允许整 root 或显式多 case 子集聚合 |

对应地，当前也把两个附录型 flag 的边界说明写死：

- `-ShowTransitions`
  不是独立 query kind。
  它只在单 report 默认总览里把 `recent transitions` 投影成附录；
  compare 模式会先给最小 `TRANSITION COMPARE` 摘要，
  export_only 模式则保持无 compare 头部的纯附录展示。
- `-ShowArtifacts`
  同样不是独立 query kind。
  它只是把当前 report 已经持有的 bundle / sample / runtime observe / diff / report manifest 等引用追加展示出来。

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
- `comparison.fact_resolution_changed`
- `comparison.required_fact_resolution_change_kinds`
- `comparison.required_facts_changed`
- `comparison.required_fact_resolution_changes`

与此同时，
单 report 默认总览里的 `comparison` 现在也会继续带出一份最小 `capability_summary`，
至少包括：

- `comparison.drift_headline.text`
- `comparison.drift_headline.changed_dimensions`
- `comparison.drift_headline.dimension_counts`
- `comparison.capability_summary.compared_capability_count`
- `comparison.capability_summary.bringup_compare_capability_count / resource_compare_capability_count / fact_resolution_compare_capability_count`
- `comparison.capability_summary.compared_capabilities`
- `comparison.capability_summary.fact_resolution_compare_capabilities`
- `comparison.capability_summary.required_fact_resolution_change_kinds`
- `comparison.capability_summary.required_facts_changed`

单 report 里的 `comparison.drift_headline` 与 artifact_root 级字段保持同一输出形状，
但它的 `dimension_counts` 不是跨 case 汇总，而是当前 report 的维度命中标记：
每个维度只会是 `0` 或 `1`，表示这个 case 是否在该维度发生 compare drift。

如果选择的是整组 compare report，
artifact_root 级 `cap list` 现在也会继续带出：

- capability 级 `compare_cases / bringup_compare_cases / resource_compare_cases / fact_resolution_compare_cases`
- capability 级 `bringup_change_kinds / resource_change_kinds / required_fact_resolution_change_kinds`
- capability 级 `required_facts_changed`
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

- 顶层 `formation_headline.text`
- 顶层 `formation_headline.status / status_counts`
- 顶层 `formation_headline.formed_cases / blocked_cases`
- 顶层 `formation_headline.unresolved_capabilities / blocked_nodes / blockers`
- 顶层 `compiler_headline.text`
- 顶层 `compiler_headline.status / has_comparison / has_drift`
- 顶层 `compiler_headline.formation_text / drift_text`
- 顶层 `compiler_headline.drift_dimensions`
- 顶层 `compiler_headline.blocked_cases / unresolved_capabilities / blocked_nodes`
- `compared_case_count`
- `metadata_changed_case_count`
- `system_formation_changed_case_count`
- `binding_result_changed_case_count`
- `bringup_order_changed_case_count`
- `bringup_changed_case_count`
- `resource_changed_case_count`
- `fact_resolution_changed_case_count`
- `drift_headline.text`
- `drift_headline.changed_dimensions`
- `drift_headline.dimension_counts`
- `capability_summary.compared_capability_count`
- `capability_summary.bringup_compare_capability_count / resource_compare_capability_count / fact_resolution_compare_capability_count`
- `capability_summary.compared_capabilities`
- `capability_summary.fact_resolution_compare_capabilities`
- `capability_summary.required_fact_resolution_change_kinds`
- `capability_summary.required_facts_changed`
- `system_compiler_summary.changed_case_count / unchanged_case_count`
- `system_compiler_summary.stage_change_matrix / status_change_matrix`
- `system_compiler_summary.system_spec_change_matrix / resolved_input_change_matrix`
- `system_compiler_summary.declared_fact_change_matrix / declared_contract_change_matrix / subject_fact_change_matrix`
- `system_compiler_summary.unresolved_capability_change_matrix / blocked_node_change_matrix / blocker_change_matrix`
- `system_compiler_summary.blocker_reason_change_matrix / blocker_missing_requires_change_matrix / blocker_depends_on_change_matrix`
- `system_compiler_summary.binding_reason_change_matrix / bringup_phase_change_matrix / bringup_dependency_change_matrix`
- `system_compiler_summary.formation_drift / binding_drift / bringup_drift`
- `system_compiler_summary.result_map`
- `comparison.system_compiler_summary.cases[*].formation_basis_changes / binding_summary_changes / bringup_summary_changes`
- `system_input_summary.changed_case_count / unchanged_case_count`
- `system_input_summary.system_spec_change_matrix / resolved_input_change_matrix`
- `system_input_summary.declared_fact_change_matrix / subject_fact_change_matrix`
- `system_input_summary.declared_contract_change_matrix`
- `binding_result_summary.changed_case_count / unchanged_case_count`
- `binding_result_summary.capability_change_matrix / unresolved_capability_change_matrix`
- `bringup_order_summary.changed_case_count / unchanged_case_count`
- `bringup_order_summary.entry_change_matrix / blocked_node_change_matrix`
- `system_formation_summary.changed_case_count / unchanged_case_count`
- `system_formation_summary.status_change_matrix`
- `system_formation_summary.blocker_change_matrix`
- `fact_resolution_summary.changed_case_count / unchanged_case_count`
- `fact_resolution_summary.fact_inventory_change_matrix`
- `fact_resolution_summary.required_fact_resolution_change_matrix`

这意味着默认总览已经能直接回答：

> **这一组 compare report 里，到底有多少 case 真正在 compare 维度上发生了漂移。**

其中顶层 `compiler_headline` 是默认总览的第一眼扫读入口：
它把当前结果的 `formation_headline` 与 compare 侧的 `comparison.drift_headline`
合成一行 `text`，形如 `status:blocked drift:formation,binding,bringup_order`。
如果当前 report/root 没有 compare 结果，`drift` 会写成 `n/a`；
如果存在 compare 结果但没有漂移，`drift` 会写成 `none`。
它只负责回答“当前是否成立 + 漂移在哪些维度”，
不替代 `formation_headline`、`comparison.drift_headline` 或各个 `comparison.*_summary`。

其中顶层 `formation_headline` 描述的是当前 artifact_root 这组结果“如何成立”：
它把 `formed / blocked / unresolved_bindings / blocked_nodes / blockers`
压成一行 `text`，并保留 blocked case 与阻塞热点给轻量工具消费。
它不是 compare drift 字段；如果需要看左右两份 report 的变化，
仍应读取 `comparison.drift_headline` 与各个 `comparison.*_summary`。

其中 `comparison.drift_headline` 是给人类和轻量工具看的扫读入口：
它把 `metadata / input / formation / binding / bringup_order / bringup_evidence / resource / fact_resolution`
这些维度压成一行 `text`，并保留 `changed_dimensions` 与 `dimension_counts` 供机器消费。
详细诊断仍以各个 `*_summary` 与 matrix 为准。

其中 case summary 行级 `FactCmp` 只统计该 case 的
`comparison.fact_resolution.required_fact_resolution_changes` 数量，
用于把“事实解析漂移”从 `ResCmp` 这种资源法律漂移计数中分出来。

它现在还会继续给出一份最小 capability 热点入口，
用来先回答：

> **这些漂移主要集中在哪些 capability 上。**

与此同时，artifact_root 默认总览顶层现在也会继续显式带出一份
`system_compiler_summary`，至少包括：

- `case_count / formed_case_count / blocked_case_count`
- `totals.declared_fact_count / totals.declared_contract_count / totals.subject_fact_count`
- `totals.required_binding_count / totals.unresolved_binding_count`
- `totals.ordered_node_count / totals.blocked_node_count / totals.blocker_count`
- `case_kind_matrix`
- `resolved_profile_matrix / resolved_board_matrix / resolved_active_facet_matrix`
- `unresolved_capability_matrix / blocked_node_matrix / blocker_matrix`
- `blocker_reason_matrix / blocker_missing_requires_matrix / blocker_depends_on_matrix`
- `binding_reason_matrix / bringup_phase_matrix / bringup_dependency_matrix`
- `formation_basis / binding_basis / bringup_basis`
- `result_map`
- `cases[*].formation_basis / binding_summary / bringup_summary`

它不取代后面的分阶段摘要，
而是把“系统如何成立”的主链先压成一份 root 级总结果物，
用来先回答：

> **这一组 case 到底以什么输入成立、在哪个阶段收口、最终为什么 formed 或 blocked。**

为了让这份 root 级总结果物不再只是“inspector 顺手吐出的一个大对象”，
`system_compiler_summary` 现在也已经被提升成独立协议对象：

- summary 模式显式带出 `kind = system_compiler_summary/v0` 与 `mode = summary`
- comparison 模式显式带出 `kind = system_compiler_summary/v0` 与 `mode = comparison`
- 对应 schema 入口见 [`../../schemas/system_compiler_summary.v0.schema.json`](../../schemas/system_compiler_summary.v0.schema.json)
- 对应最小样例见 [`../../schemas/examples/system_compiler_summary.summary.v0.sample.json`](../../schemas/examples/system_compiler_summary.summary.v0.sample.json)
- comparison 样例见 [`../../schemas/examples/system_compiler_summary.comparison.v0.sample.json`](../../schemas/examples/system_compiler_summary.comparison.v0.sample.json)

这样外部脚本、CI 与 IDE 原型如果直接消费 artifact_root 默认总览里的
`system_compiler_summary` 或 `comparison.system_compiler_summary`，
就不再需要依赖“当前上下文是不是 artifact_root 默认总览”来猜对象类型，
而可以直接通过 `kind / mode` 识别它。

这里的 `result_map.stage_blocks[*].root_fields` 语义也要收紧理解：
它描述的是 `system_compiler_summary` 根上哪些字段归属于该 stage，
以及这些字段应如何和 `formation_basis / binding_basis / bringup_basis`
或 `formation_drift / binding_drift / bringup_drift` 这类 stage block 一起阅读；
它不承诺 `system_formation_summary`、`binding_result_summary`、`bringup_order_summary`
一定逐字段同名镜像这些 root field。

其中 `cases[*]` 现在也会显式携带：

- `formation_basis.case_kind / declared_fact_count / declared_contract_count / subject_fact_count`
- `binding_summary.required_binding_count / resolved_binding_count / unresolved_binding_count`
- `binding_summary.resolved_capabilities / unresolved_capabilities`
- `bringup_summary.ordered_node_count / blocked_node_count / blocked_nodes / phase_counts`

这样调用方在 root 级总结果里，
就已经可以直接看到每个 case 的最小成立 basis，
而不必先跳回各个分阶段摘要再重新拼接。

与此同时，这份总结果现在也会继续把 blocker 热点直接聚成：

- `blocker_reason_matrix`
- `blocker_missing_requires_matrix`
- `blocker_depends_on_matrix`

以及 compare 模式下的：

- `blocker_reason_change_matrix`
- `blocker_missing_requires_change_matrix`
- `blocker_depends_on_change_matrix`

而且还会把 binding / bringup 的成立模式与变化模式直接聚成：

- `binding_reason_matrix`
- `bringup_phase_matrix`
- `bringup_dependency_matrix`

以及 compare 模式下的：

- `binding_reason_change_matrix`
- `bringup_phase_change_matrix`
- `bringup_dependency_change_matrix`

与此同时，这些热区现在也会继续被压进更正式的 stage-level result block：

- `formation_basis`
- `binding_basis`
- `bringup_basis`

以及 compare 模式下的：

- `formation_drift`
- `binding_drift`
- `bringup_drift`

这样调用方在 root 级总结果里，
就已经可以先回答：

> **这组系统实例主要卡在哪些 blocker reason、缺哪几个 require、被哪些 dependency node 牵住。**

以及：

> **这组系统实例的 binding 为什么能成立，bringup 主要落在哪些 phase，又是沿着哪些 dependency node 展开的。**

而现在 `formation / binding / bringup` 三段也已经在同一份 root 结果里都拥有正式 block，
这让 `system_compiler_summary` 更接近一个真正的 `system compiler v0 result object`，
而不再只是“若干热点矩阵并排摆放”。

同时，`result_map` 现在也开始把这些 block 和分阶段 summary 的对应关系正式机器可读化。
而 `system_compiler_summary` 自身的 schema 也会继续直接引用这份 relation language，
让“总结果物”与“结果物内部关系图”留在同一个协议边界内：

如果外部工具要直接消费这份关系语言，当前最小 schema 锚点见
[`../../schemas/system_compiler_result_map.v0.schema.json`](../../schemas/system_compiler_result_map.v0.schema.json)。

- 哪个 stage 对应 `cases[*]` 里的哪个 case projection 字段
- `cases[*]` 里的 projection 内部字段到底来自哪一个 stage case summary，是否存在 fallback source
- 哪个 stage 对应 root 里的哪个 block
- 哪个 stage 对应 artifact_root 里的哪个分阶段 summary
- 哪些 root 级矩阵仍然属于 input bridge，而不是 formation / binding / bringup block 本体
- 每个 root field 在 field-level 上到底是直接复用分阶段 summary 字段、只是 block 内部别名，还是当前没有 direct mirror

这样 `system_compiler_summary` 不只是“有块”，
而是已经开始带出“这些块之间如何组成 result object”的语言。

其中这层 field-level 关系现在会继续收进：

- `input_bridge.field_relations[*]`
- `case_projection_field_relations.<stage>[*]`
- `stage_blocks[*].field_relations[*]`

每条 relation 至少会带出：

- `projection_field`
- `source_candidates[*].stage / field_path / relation`
- `root_field`
- `block_field_path`
- `block_relation`
- `summary_field_path`
- `summary_relation`

当前 `block_relation / summary_relation` 只使用三种最小语义：

- `same_field`
- `field_alias`
- `none`

而 `case_projection_field_relations.<stage>[*].source_candidates[*].relation`
当前则会使用：

- `same_field`
- `field_alias`

也就是说，调用方现在不只能知道
“`binding` 这段对应 `binding_basis` 和 `binding_result_summary`”，
还可以继续知道：

- `binding_reason_matrix` 在 root 上属于 `binding`，但在 block 内对应 `binding_basis.reason_matrix`
- `bringup_phase_matrix` 在 root 上属于 `bringup`，但当前没有 direct summary field mirror
- `unresolved_capability_matrix` 这类字段则既能在 root 上看，也能在 block 和部分分阶段 summary 上按同一字段名看到
- `cases[*].formation_basis.declared_fact_count` 优先来自 `system_formation_summary.cases[*].declared_fact_count`，缺位时再退回到 `system_input_summary.cases[*].declared_fact_count`
- `cases[*].binding_summary.resolved_capabilities` 当前来自 `binding_result_summary.cases[*].resolved_capabilities`，并不要求 `system_formation_summary.cases[*]` 也同名提供

与此同时，artifact_root 默认总览顶层现在也会继续显式带出一份
`system_input_summary`，至少包括：

- `case_count`
- `totals.declared_fact_count / totals.declared_contract_count / totals.subject_fact_count`
- `case_kind_matrix`
- `declared_profile_matrix / declared_board_matrix`
- `resolved_profile_matrix / resolved_board_matrix`
- `resolved_active_facet_matrix`
- `declared_fact_matrix / declared_contract_matrix / subject_fact_matrix`

现在这份 input-side summary object 也已经被提升成独立协议对象：

- summary 模式显式带出 `kind = system_input_summary/v0` 与 `mode = summary`
- comparison 模式显式带出 `kind = system_input_summary/v0` 与 `mode = comparison`
- 对应 schema 入口见 [`../../schemas/system_input_summary.v0.schema.json`](../../schemas/system_input_summary.v0.schema.json)
- 对应最小样例见 [`../../schemas/examples/system_input_summary.summary.v0.sample.json`](../../schemas/examples/system_input_summary.summary.v0.sample.json)
- comparison 样例见 [`../../schemas/examples/system_input_summary.comparison.v0.sample.json`](../../schemas/examples/system_input_summary.comparison.v0.sample.json)

这样外部脚本与 CI 如果直接消费 artifact_root 默认总览里的
`system_input_summary` 或 `comparison.system_input_summary`，
就不必再依赖“当前字段名碰巧叫这个”来识别对象语义，
而可以直接通过 `kind / mode` 把它当作正式的 input-side result object。

并继续显式带出一份
`binding_result_summary`，至少包括：

- `case_count`
- `totals.required_binding_count / totals.resolved_binding_count / totals.unresolved_binding_count`
- `capability_matrix`
- `resolved_capability_matrix / unresolved_capability_matrix`

现在这份 binding-side summary object 也已经被提升成独立协议对象：

- summary 模式显式带出 `kind = binding_result_summary/v0` 与 `mode = summary`
- comparison 模式显式带出 `kind = binding_result_summary/v0` 与 `mode = comparison`
- 对应 schema 入口见 [`../../schemas/binding_result_summary.v0.schema.json`](../../schemas/binding_result_summary.v0.schema.json)
- 对应最小样例见 [`../../schemas/examples/binding_result_summary.summary.v0.sample.json`](../../schemas/examples/binding_result_summary.summary.v0.sample.json)
- comparison 样例见 [`../../schemas/examples/binding_result_summary.comparison.v0.sample.json`](../../schemas/examples/binding_result_summary.comparison.v0.sample.json)

这样外部脚本与 CI 如果直接消费 artifact_root 默认总览里的
`binding_result_summary` 或 `comparison.binding_result_summary`，
就不必再依赖外围上下文去猜“这是不是 binding 热区汇总对象”，
而可以直接通过 `kind / mode` 把它当作正式的 binding-side result object。

并显式带出一份 `bringup_order_summary`，
至少包括：

- `case_count`
- `totals.ordered_node_count / totals.blocked_node_count`
- `phase_counts`
- `node_matrix / blocked_node_matrix`

现在这份 bringup-side summary object 也已经被提升成独立协议对象：

- summary 模式显式带出 `kind = bringup_order_summary/v0` 与 `mode = summary`
- comparison 模式显式带出 `kind = bringup_order_summary/v0` 与 `mode = comparison`
- 对应 schema 入口见 [`../../schemas/bringup_order_summary.v0.schema.json`](../../schemas/bringup_order_summary.v0.schema.json)
- 对应最小样例见 [`../../schemas/examples/bringup_order_summary.summary.v0.sample.json`](../../schemas/examples/bringup_order_summary.summary.v0.sample.json)
- comparison 样例见 [`../../schemas/examples/bringup_order_summary.comparison.v0.sample.json`](../../schemas/examples/bringup_order_summary.comparison.v0.sample.json)

这样外部脚本与 CI 如果直接消费 artifact_root 默认总览里的
`bringup_order_summary` 或 `comparison.bringup_order_summary`，
就不必再依赖外围上下文去猜“这是不是 bringup 顺序与阻塞热区汇总对象”，
而可以直接通过 `kind / mode` 把它当作正式的 bringup-side result object。

并继续显式带出一份
`system_formation_summary`，至少包括：

- `case_count / formed_case_count / blocked_case_count`
- `formed_cases / blocked_cases`
- `totals.required_binding_count / totals.unresolved_binding_count`
- `unresolved_capability_matrix`
- `blocked_node_matrix`
- `blocker_matrix`

现在这份 formation-side summary object 也已经被提升成独立协议对象：

- summary 模式显式带出 `kind = system_formation_summary/v0` 与 `mode = summary`
- comparison 模式显式带出 `kind = system_formation_summary/v0` 与 `mode = comparison`
- 对应 schema 入口见 [`../../schemas/system_formation_summary.v0.schema.json`](../../schemas/system_formation_summary.v0.schema.json)
- 对应最小样例见 [`../../schemas/examples/system_formation_summary.summary.v0.sample.json`](../../schemas/examples/system_formation_summary.summary.v0.sample.json)
- comparison 样例见 [`../../schemas/examples/system_formation_summary.comparison.v0.sample.json`](../../schemas/examples/system_formation_summary.comparison.v0.sample.json)

这样外部脚本与 CI 如果直接消费 artifact_root 默认总览里的
`system_formation_summary` 或 `comparison.system_formation_summary`，
就不必再依赖外围上下文去猜“这是不是 formation 结果与 formation drift 汇总对象”，
而可以直接通过 `kind / mode` 把它当作正式的 formation-side result object。

并继续显式带出一份 `fact_resolution_summary`，
至少包括：

- `case_count`
- `totals.declared_contracts / totals.satisfied_count / totals.violated_count / totals.unknown_count`
- `required_fact_matrix / provided_fact_matrix`
- `required_fact_resolution_matrix`
- `contract_matrix`
- `resource_hotspot_matrix`

对应 schema 入口见 [`../../schemas/fact_resolution_summary.v0.schema.json`](../../schemas/fact_resolution_summary.v0.schema.json)，
最小样例见 [`../../schemas/examples/fact_resolution_summary.summary.v0.sample.json`](../../schemas/examples/fact_resolution_summary.summary.v0.sample.json)，
comparison 样例见 [`../../schemas/examples/fact_resolution_summary.comparison.v0.sample.json`](../../schemas/examples/fact_resolution_summary.comparison.v0.sample.json)。
现在外部脚本也可以直接通过 `kind = fact_resolution_summary/v0` 与 `mode = summary | comparison`
把它当作独立的 fact-resolution-side result object 识别。
其中 `required_fact_resolution_matrix` 专门用于横向回答：

- 某个 required fact 在多少 case 中被声明
- 在哪些 case 中是 `satisfied`
- 在哪些 case 中是 `missing`
- 当前能追溯到哪些 provider source / role

这意味着 inspector 已经不只会逐 case 地回答
“系统是否 formed / blocked”，
还会横向回答：

> **这一组系统实例当前是以哪些规范化输入被成立出来的，输入侧事实和解析结果如何收口。**

同时也会横向回答：

> **这一组系统实例整体是怎么形成的，阻塞面主要集中在哪里。**

同时也会横向回答：

> **这一组系统实例依赖哪些 facts、哪些资源法律在多少 case 中成立、热点集中在哪些 contract/fact 上。**

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
- `comparison.fact_resolution_changed`
- `comparison.required_fact_resolution_change_kinds`
- `comparison.required_facts_changed`
- `comparison.fact_resolution.required_fact_resolution_changes`

如果选择的是整组 compare report，
artifact_root 级该查询现在也会继续带出：

- `state_counts`
- `compared_case_count / bringup_compare_case_count / resource_compare_case_count`
- `compared_cases / bringup_compare_cases / resource_compare_cases`
- `resource_contracts`
- `fact_resolution_compare_case_count / fact_resolution_compare_cases`
- `required_facts_changed`

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
- `comparison.fact_resolution_changed`
- `comparison.required_fact_resolution_change_kinds`
- `comparison.required_facts_changed`
- `comparison.fact_resolution.required_fact_resolution_changes`

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
  当前至少区分 `declared_facts / subject_facts / required_facts / graph_provided_facts / audit_provided_facts`
- `contracts`
  则把每条输入侧合同压成稳定查询结果，
  至少带出 `state / requires / present_facts / missing_facts / fact_sources`

如果当前 report 已经带出顶层 `fact_resolution`，
这里也优先直接投影这份正式结果物，
避免 explain 面与 artifact report 结果物重新漂移出两套形状。

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

如果调用方当前只是在看单 report 默认总览，
`-ShowTransitions` 现在也会继续复用同一套 transition 行展示语言，
至少带出：

- `order / capability / action / before / after`
- 行级 `BrCmp / ResCmp`

这里的 transition 行级 `ResCmp` 仍然只表达“该 capability 在资源/事实相关 compare 面有漂移”，
不会把未出现在 `recent_transitions` 里的 required fact resolution 漂移强行混入 runtime 视图。
case summary 行级的 `FactCmp` 才是默认总览里用于观察 required fact resolution 漂移的字段。

如果当前 report 来自 compare 模式，
它还会在 `[TRANSITIONS]` 前先给出一行最小 `TRANSITION COMPARE` 摘要，
用来快速说明这批附录里的 transition 有多少条同时落在 compare 漂移面上。

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

当前建议把 `artifact report` 分成十一组字段。

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

### 5.3 规范化输入摘要

这一组回答：

> “这个系统实例是按什么输入成立的，profile / board / facets 又是从哪里解析出来的。”

当前建议至少包含：

- `system_spec`
- `declared_input`
- `resolved_input`

其中：

- `system_spec`
  当前先把 `case_name / case_kind / source / build_dir / build_target / export_target` 收成最小输入投影
- `declared_input`
  当前先保留来自 case entry 的 `subject / declared_facts / declared_contract_entries`
- `resolved_input`
  当前先保留：
  - `profile.value / profile.source`
  - `board.value / board.source`
  - `active_facets.values / active_facets.source`
  - `subject_facts`

这组字段的意义，不是发明最终 DSL，  
而是先把当前散落在：

- case manifest
- export bundle case entry
- CLI override
- CI subject defaults

里的输入载体，压成一份正式结果物里的规范化输入摘要。

也就是说，`artifact report` 现在不仅能回答“系统长成了什么样”，
也开始能回答：

- 这个 case 当前属于哪类 `SystemSpec` 投影
- profile / board / facets 是从显式参数、默认值还是 case subject 来的
- 哪些事实和合同是 case 自己显式声明的

### 5.4 静态结构摘要

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

### 5.5 binding 结果摘要

这一组回答：

> “当前 required binding 到底成立了多少，还有哪些没成立。”

建议至少包含：

- `required_binding_count`
- `resolved_binding_count`
- `unresolved_binding_count`
- `resolved_capabilities`
- `unresolved_capabilities`
- `binding_entries`

其中 `binding_entries` 当前建议至少稳定保留：

- `capability`
- `state`
- `provider_nodes`
- `consumer_nodes`
- `reason`

这组字段的意义在于把原先散落在：

- `required_facts`
- `unresolved_bindings`
- capability provider / consumer 关系

里的成立性结论，正式压成一个结果物。

也就是说，`binding_result` 当前不只是“哪些 capability 缺了”，
还要能回答：

- 这个 required capability 由谁提供
- 谁在消费它
- 为什么当前被判成 `resolved` 或 `unresolved`

### 5.6 bringup 顺序摘要

这一组回答：

> “系统当前按什么顺序被 bring up，以及每个节点依赖谁。”

建议至少包含：

- `ordered_node_count`
- `blocked_node_count`
- `phase_counts`
- `entries`

其中 `entries` 当前建议至少稳定保留：

- `order`
- `node`
- `kind`
- `phase`
- `runlevel_text`
- `provides`
- `requires`
- `dependency_nodes`
- `resolved_requires`
- `missing_requires`
- `state`

这组字段的价值不在于重复 DOT，
而在于把“系统如何成立”的过程性结果正式写进报告对象。

也就是说，它应该开始稳定回答：

- 谁先被 bring up
- 谁依赖谁
- 哪些 require 已满足
- 哪些 require 仍缺失，因此当前只能标成 `blocked`

与此同时，v0 当前还把 `binding_result + bringup_order` 的合成成立性结论压成一组顶层 `system_formation` 摘要，
用来正式回答：

- 当前系统整体是 `formed` 还是 `blocked`
- 这份成立性判断基于哪类 case、多少 declared fact / declared contract / subject fact
- unresolved capability 与 blocked node 最终如何收敛成 blocker 列表

`system_formation` 当前建议至少包含：

- `status`
- `formation_basis`
- `binding_summary`
- `bringup_summary`
- `blocker_count / blockers`

单 report 默认总览也会在顶层带出 `compiler_headline` 与 `formation_headline`。

`compiler_headline` 至少包含：

- `compiler_headline.text`
- `compiler_headline.status / has_comparison / has_drift`
- `compiler_headline.formation_text / drift_text`
- `compiler_headline.drift_dimensions`
- `compiler_headline.blocked_cases / unresolved_capabilities / blocked_nodes`

它把当前单 report 的成立状态与 compare drift 摘要合并成扫读入口；
`case_count` 通常为 `1`，`drift_dimensions` 也只表达当前 case 命中的 drift 维度。
如果当前 report 不是 compare 模式，`has_comparison` 为 `false`，`text` 中的 `drift` 为 `n/a`。

`formation_headline` 至少包含：

- `formation_headline.text`
- `formation_headline.status / status_counts`
- `formation_headline.formed_cases / blocked_cases`
- `formation_headline.unresolved_capabilities / blocked_nodes / blockers`

这份 headline 只描述当前 report 的成立状态，
因此 `case_count` 通常为 `1`，`status_counts` 也只是当前 case 的 `0/1` 命中。
它用于让默认视图第一眼回答“这个系统是否成立、阻塞在哪里”，
详细诊断仍以 `system_formation.binding_summary / bringup_summary / blockers` 为准。

它不取代 `binding_result` 或 `bringup_order`，
而是把“系统是否成立、为什么没成立”正式压成一个顶层结果物。

### 5.7 bringup 证据摘要

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

### 5.8 资源契约摘要

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

在这层最小审计之上，
artifact report 现在也把“事实从哪里来、合同为什么成立或不成立”扶正成了
`fact_resolution` 顶层结果物。

`fact_resolution` 当前建议至少包含：

- `declared_contracts / audited_count / satisfied_count / violated_count / unknown_count`
- `fact_inventory`
- `contracts`
- `required_fact_resolution`
- `resource_hotspots`

其中：

- `fact_inventory`
  当前至少区分
  `declared_facts / subject_facts / required_facts / graph_provided_facts / audit_provided_facts / all_available_facts`
- `contracts`
  则把每条声明输入里的资源法律压成稳定结果项，
  至少带出
  `contract / state / requires / present_facts / missing_facts / fact_sources / status_text`
- `required_fact_resolution`
  则把每条 `required_fact` 压成稳定结果项，
  至少带出
  `fact / state / fact_sources / providers / provider_count / status_text`
  这让报告能回答“这个 required fact 是被哪个事实桶、哪个 raw evidence provider 满足的”

也就是说：

- `resource_contract`
  继续保留输入侧法律文本与最小审计层
- `fact_resolution`
  则负责把输入事实、图事实、required fact 满足关系与合同成立性收束成正式结果语言

### 5.9 运行时观察摘要

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

### 5.10 比较摘要（compare 模式可选）

这一组回答：

> “如果当前报告来自 compare 模式，这个 case 相对 baseline 到底发生了什么。”

建议至少包含：

- `status`
- `summary_changes`
- `metadata_changes`
- `node_changes`
- `edge_changes`
- `binding_result`
- `bringup_order`
- `bringup_evidence`
- `resource_contract`
- `fact_resolution`

这组字段不应取代底层 `bundle_diff`，
但它应该把 case 级最重要的比较结论直接拉到报告顶层。
对于 metadata-only diff，当前 `status` 仍可保持 `unchanged`，
但 `metadata_changes` 不应被吞掉。
而 `comparison.system_input` 则用于承载“系统如何成立”的输入投影相对 baseline 发生了什么漂移，
把 `system_spec / declared_input / resolved_input` 正式收进 compare 结果物。

`comparison.system_input` 当前建议至少包含：

- `changed`
- `left / right`
- `summary_changes`
- `system_spec_changes / declared_subject_changes / resolved_input_changes`
- `declared_fact_changes / declared_contract_changes / subject_fact_changes`

这意味着 compare 模式下即使顶层 `status = unchanged`，
报告也已经能继续回答：

- 当前输入侧到底有没有漂移
- 漂移发生在 `SystemSpec`、声明输入还是解析后的输入
- 哪个 declared fact / declared contract / subject fact 让“系统如何成立”出现了变化

而 `comparison.system_formation` 则用于承载“系统整体是否成立、阻塞点为何物”相对 baseline 发生了什么变化，
把 formation status、summary drift 与 blocker drift 正式收进 compare 结果物。

`comparison.system_formation` 当前建议至少包含：

- `changed`
- `left / right`
- `summary_changes`
- `blocker_changes`
- `unresolved_capability_changes`
- `blocked_node_changes`

这意味着 compare 模式下报告已经能继续回答：

- 当前 candidate 是否已经从 `formed` 退化为 `blocked`
- 哪些 blocker 是新增、删除或发生了状态变化
- 哪些 unresolved capability / blocked node 已经进入正式 formation 结果面

而 `comparison.binding_result` 则用于承载“同一份结构相对 baseline 的 binding 成立情况发生了什么变化”，
把 `resolved / unresolved` 与 capability 级 binding 切换正式拉进结果物。

`comparison.binding_result` 当前建议至少包含：

- `changed`
- `left / right`
- `summary_changes`
- `binding_changes`
- `resolved_capability_changes`
- `unresolved_capability_changes`

这意味着 compare 模式下报告已经能直接回答：

- 哪些 required capability 从 `resolved` 漂移到 `unresolved`
- 哪些 binding 只是 provider / consumer / reason 发生变化

而 `comparison.bringup_order` 则用于承载“系统 bringup 次序与阻塞面相对 baseline 发生了什么变化”，
把 `ready / blocked` 与 blocked node 漂移也收口进正式 compare 结果物。

`comparison.bringup_order` 当前建议至少包含：

- `changed`
- `left / right`
- `summary_changes`
- `entry_changes`
- `blocked_node_changes`

这意味着 compare 模式下报告已经能继续回答：

- 哪些 node 的 bringup 状态发生了漂移
- 哪些 blocked node 是新出现的

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

而 `comparison.fact_resolution` 则用于承载“事实库存与合同成立性结果面相对 baseline 发生了什么漂移”，
把输入事实、图事实与合同状态变化正式收进 compare 结果物。

`comparison.fact_resolution` 当前建议至少包含：

- `changed`
- `left / right`
- `summary_changes`
- `fact_inventory_changes`
- `required_fact_resolution_changes`
- `contract_changes`
- `hotspot_changes`

这意味着 compare 模式下即使顶层 `status = unchanged`，
报告也已经可以继续回答：

- 哪组 `declared / subject / required / graph_provided / audit_provided` facts 发生了变化
- 哪个 required fact 从 `missing` 变为 `satisfied`，或 provider/source 发生了变化
- 哪条资源法律虽然仍存在，但其成立性结果已经漂移
- 当前 drift 到底停留在最小审计层，还是已经进入正式 fact resolution 结果面

### 5.11 支持工件引用

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

### 5.12 Artifact root index

当一组 `artifact report` 通过 `-OutputRoot` 落到同一个 root 时，
root 下可以额外存在一份 `index.json`：

- `schema = system_compiler.artifact_report_index/v0`
- `report_kind = system_compiler.artifact_report_index`

它回答的问题不是“完整诊断是什么”，而是：

> “这组 report 当前是否成立、是否发生 drift、阻塞热点在哪里、full report 在哪里。”

当前 index 建议保持轻量，至少包含：

- `artifact_root`
- `bundle.root / bundle.index / bundle.input_manifest`
- `artifacts.diff / artifacts.ci_summary / artifacts.report_manifest`
- `case_count`
- `compiler_headline`
- `cases[*].name / path / mode`
- `cases[*].profile / board / active_facets`
- `cases[*].formation_status / comparison_status`
- `cases[*].has_drift / drift_dimensions`
- `cases[*].unresolved_binding_count / blocked_node_count / blocker_count`
- `cases[*].unresolved_capabilities / blocked_nodes`

其中 `compiler_headline` 复用 artifact_root 默认总览的第一眼语义，
至少带出：

- `text`
- `status`
- `formation_text / drift_text`
- `has_comparison / has_drift`
- `case_count / formed_case_count / blocked_case_count`
- `unresolved_binding_count / blocked_node_count / blocker_count`
- `compared_case_count / changed_dimension_count`
- `drift_dimensions / dimension_counts`
- `blocked_cases / unresolved_capabilities / blocked_nodes`

这里必须守住两个边界：

- index 不复制 full report 的各类 summary matrix。
- index 不替代 inspector 的 artifact_root 默认总览。

如果工具需要完整 system input、binding result、bringup order、fact resolution、
resource contract 或 capability 级 explain，
仍应继续读取 case 级 `*.artifact_report.json`，
或调用 `inspect_system_compiler_artifact_report.ps1` 的对应查询。

CI 链路当前也会把这个 first-read 入口继续暴露到 `ci summary`：

- `summary.artifact_report.index`
- `summary.artifact_report.compiler_headline`

这样自动化系统可以先读 CI summary，
再按需跳转到 root index 或 full report。

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
  "system_input": {
    "system_spec": {
      "case_name": "bringup-minimal-observe-demo",
      "case_kind": "materialized_graph",
      "source": "Examples/init/bringup_minimal_observe_demo",
      "build_dir": "cmake-build-init-bringup-minimal-observe-clang",
      "build_target": "init-bringup-minimal-observe-demo",
      "export_target": "export_bringup_minimal_materialized_graph"
    },
    "declared_input": {
      "subject": {
        "profile": null,
        "board": "stm32_stub",
        "active_facets": ["runtime", "input"]
      },
      "declared_facts": ["board.stm32_stub"],
      "declared_contract_entries": [
        { "contract": "needs_monotonic_clock", "requires": ["system.clock"] }
      ]
    },
    "resolved_input": {
      "profile": { "value": "MCU_MIN", "source": "explicit_argument" },
      "board": { "value": "stm32_stub", "source": "case_subject" },
      "active_facets": { "values": ["runtime", "input"], "source": "case_subject" },
      "subject_facts": [
        "profile.MCU_MIN",
        "board.stm32_stub",
        "facet.runtime",
        "facet.input"
      ]
    }
  },
  "structure": {
    "capability_count": 12,
    "node_count": 9,
    "edge_count": 8,
    "declared_facts": ["board.stm32_stub"],
    "required_facts": ["platform.irq", "system.clock"],
    "unresolved_bindings": []
  },
  "binding_result": {
    "required_binding_count": 2,
    "resolved_binding_count": 2,
    "unresolved_binding_count": 0,
    "binding_entries": [
      {
        "capability": "platform.irq",
        "state": "resolved",
        "provider_nodes": ["platform.irq"],
        "consumer_nodes": ["hal.uart1"],
        "reason": "required capability is provided by at least one materialized node"
      }
    ]
  },
  "bringup_order": {
    "ordered_node_count": 5,
    "blocked_node_count": 0,
    "phase_counts": {
      "core": 3,
      "service": 2
    },
    "entries": [
      {
        "order": 3,
        "node": "hal.uart1",
        "phase": "service",
        "requires": ["platform.irq", "system.clock"],
        "dependency_nodes": ["platform.irq", "system.clock"],
        "missing_requires": [],
        "state": "ready"
      }
    ]
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
