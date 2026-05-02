# Charm Schemas

这个目录存放 Charm 当前已经公开的机器可读协议文件。

它们的目标不是替代各自的设计说明文档，
而是给脚本、CI、IDE 原型、外部工具一个明确的协议锚点。

## 当前协议分组

### `materialized_graph` 观察导出链

- `materialized_graph.export_case_manifest.v1.schema.json`
  - 对应 `scripts/materialized_graph.export_case_manifest.v1.json`
  - 用途偏向 `materialized_graph` 批量导出 case 的声明式输入事实
  - 它当前服务于 export 脚本与 case 审计，但不等于最终 `SystemSpec` DSL
  - 当前通过 `case_kind` 显式区分 `materialized_graph` 与 `runtime_only` 两类 case
  - 当前也允许 case 以可选 `runtime_observe` sidecar 的形式声明独立 runtime 观察工件入口
  - 当前也允许 case 通过 `runtime_observe_target / runtime_observe_cache / default_runtime_observe`
    声明“由 export 脚本一并生成”的 sidecar，而不只是不透明外部路径
  - 当前也可通过 `python ./scripts/validate_materialized_graph_artifacts.py --export-case-manifest ...` 进入统一校验脚本

- `materialized_graph.sample.v2.schema.json`
  - 对应 `format_json_sample(...)` 当前导出的样例协议
  - 用途偏向字段勘探、原型接入、脚本分析
  - 它是“当前受支持的样例协议”，不是长期冻结协议

- `materialized_graph.export_bundle.v1.schema.json`
  - 对应 `scripts/export_materialized_graph.ps1` 生成的 `index.json`
  - 用途偏向批量导出结果组织、bundle 检视与 diff
  - 它已经是当前脚本链的稳定消费面之一，并且现在也可承载 per-case `subject` 元数据、可选 `runtime_observe` sidecar 与输入 `manifest` provenance
  - 它当前也允许 `runtime_only` case 在 `dot/json` 为空时正式进入 bundle，而不再要求所有真实 runtime producer 都先伪装成 graph case

- `materialized_graph.bundle_diff.v1.schema.json`
  - 对应 `scripts/diff_materialized_graph_bundle.ps1 -AsJson` 的输出
  - 用途偏向结构差异分析、报告生成前的数据交换、工具侧增量审阅
  - 它已经是 diff / report / CI 这条链上的机器可读中间协议，并可继续带出左右 case 的 `subject` 视图与左右 bundle 的输入 `manifest` provenance
  - 它当前也正式支持 mixed bundle：`runtime_only` case 可在 `graph = null` 时继续进入 diff / report / CI，而不再被脚本链误判为坏输入

- `materialized_graph.ci_summary.v1.schema.json`
  - 对应 `scripts/ci_materialized_graph_bundle.ps1` 生成的 `summary.json`
  - 用途偏向 CI 编排、状态汇总、上层自动化消费
  - 它已经是当前 CI / 工作流消费面之一，并且现在也可引用生成出的 `artifact report`、`subject_defaults` 与 candidate/baseline bundle 的输入 `manifest` provenance

- `materialized_graph.report_manifest.v1.schema.json`
  - 对应 `scripts/report_materialized_graph_bundle.ps1` 生成的 `report manifest`
  - 用途偏向报告工件发现、报告层元数据交换、上层工具对 Markdown / HTML 的稳定引用
  - 它把“报告本身”从纯文件输出推进成可被自动化消费的对象，并继续保留左右 bundle 的输入 `manifest` provenance

### `system_compiler` 输出面草案

- `system_compiler.artifact_report.v0.schema.json`
  - 对应 `docs/system/artifact_report_v0.md` 中定义的最小统一报告对象
  - 用途偏向字段收敛、样例校验、后续脚本/CI 接入前的协议锚定
  - 它当前是 v0 草案协议，已能覆盖 `export_only` 与 `compare` 两种最小输出场景，并可继续引用 `bundle / input_manifest / runtime_observe / diff / report manifest`
  - 它当前的 `runtime_observe` 摘要也已经可以继续带出 `observed_capabilities`，让 runtime-only case 在 explain 面里不再退化成“完全未声明”

- `examples/system_compiler.artifact_report.v0.sample.json`
  - 对应 `system_compiler.artifact_report/v0` 的最小机器可验样例
  - 用途偏向 schema 自检、字段讨论与后续脚本接入前的样例锚点

- `examples/system_compiler.artifact_report.v0.i2c_facts.sample.json`
  - 对应 `system_compiler.artifact_report/v0` 中 I2C device contract facts 的投影样例
  - 用途偏向验证 `io.device_i2c_facts` 可以通过现有 `fact_resolution.fact_inventory`
    与 `resource_contract.provided_facts` 进入 artifact report，而不新增顶层 I2C 专用字段
  - 它是 schema-level sample，并与当前 `i2c-device-contract-facts-smoke` 真实导出链保持同一种投影语义

- `system_compiler.fact_evidence.v0.schema.json`
  - 对应 `system_compiler.fact_evidence/v0` 的通用事实证据 sidecar
  - 用途偏向让 contract-local 或 board/package-local facts 在不伪造 graph 的前提下进入 export bundle 与 artifact report

- `examples/system_compiler.fact_evidence.v0.i2c_facts.sample.json`
  - 对应 `io.device_i2c_facts` 的 contract-local fact evidence 样例
  - 用途偏向钉住 I2C driver contract facts 如何投影到通用 `fact_evidence` sidecar

- `examples/system_compiler.fact_evidence.v0.board_facts.sample.json`
  - 对应 `platform.board_facts` 的 board/package-local fact evidence 样例
  - 用途偏向钉住 `BoardCaps` 当前事实载体如何投影到通用 `fact_evidence` sidecar

- `system_compiler.canonical_world.v0.schema.json`
  - 对应 `docs/system/canonical_world_v0.md` 里定义的 canonical world 对象
  - 用途偏向把一组 case / contract / witness plan 收成“这个世界想证明什么”的正式声明对象
  - 它当前刻意只覆盖 `artifact_report / runtime_evidence_bundle / example_ref` 三类 witness plan

- `examples/system_compiler.canonical_world.v0.sample.json`
  - 对应 `system_compiler.canonical_world/v0` 的最小样例
  - 用途偏向 schema 自检，以及 witness bundle 脚本的 sample 输入

- `system_compiler.witness_bundle.v0.schema.json`
  - 对应 `docs/system/witness_bundle_v0.md` 与 `scripts/export_system_compiler_witness_bundle.ps1`
  - 用途偏向把 canonical world、artifact report、runtime evidence bundle 与 example refs 收成正式交付对象
  - 它当前关注的是“证词是否齐、来源在哪里、缺口是什么”，而不是替代下层更细的 runtime / compare 语义

- `examples/system_compiler.witness_bundle.v0.sample.json`
  - 对应 `system_compiler.witness_bundle/v0` 的最小样例
  - 用途偏向 schema 自检、脚本输出锚点和后续 CI / IDE 原型消费

- `examples/system_compiler.witness_bundle.v0.candidate_drift.sample.json`
  - 对应 `system_compiler.witness_bundle/v0` 的 candidate drift 样例
  - 用途偏向 `world compare` 脚本和 sample 输出的最小基线/候选输入对

- `system_compiler.world_compare.v0.schema.json`
  - 对应 `docs/system/world_compare_v0.md` 与 `scripts/compare_system_compiler_world.py`
  - 用途偏向把 baseline / candidate witness bundle 提升成一个世界级 drift / collapse verdict 对象
  - 它当前关注的是“世界是否还站住、哪条 witness 先坏、最小塌陷面落在哪”，而不是替代下层 case compare

- `examples/system_compiler.world_compare.v0.sample.json`
  - 对应 `system_compiler.world_compare/v0` 的最小样例
  - 用途偏向 schema 自检、world compare 输出锚点与后续 CI / IDE 原型消费

- `system_compiler.biography.v0.schema.json`
  - 对应 `docs/system/system_compiler_biography_v0.md` 与 `scripts/export_system_compiler_biography.py`
  - 用途偏向把 runtime evidence、witness bundle 与可选的 world compare 压成一个更适合交付与追问的顶层 biography 对象
  - 它当前关注的是“这个世界是谁、为什么成立、现在站不站得住、下一步该追问什么”，而不是替代下层 witness / compare 细节

- `system_compiler.biography_index.v0.schema.json`
  - 对应 `docs/system/system_compiler_biography_index_v0.md` 与 `scripts/export_system_compiler_biography_index.py`
  - 用途偏向把一个或多个 biography summary 收拢成可验证、可发布、可审阅的 world shelf 目录对象

- `system_compiler.biography_index_compare.v0.schema.json`
  - 对应 `docs/system/system_compiler_biography_index_compare_v0.md` 与 `scripts/compare_system_compiler_biography_index.py`
  - 用途偏向把 baseline / candidate 两份 biography index summary 收成一个 shelf-to-shelf compare 对象

- `system_compiler.world_shelf_review.v0.schema.json`
  - 对应 `docs/system/system_compiler_world_shelf_review_v0.md` 与 `scripts/review_system_compiler_world_shelf.ps1`
  - 用途偏向把 candidate shelf、baseline shelf 与 shelf compare verdict 收成一个可验证的 review envelope 对象

- `system_compiler.front_page_route.v0.schema.json`
  - 对应 `docs/system/system_compiler_front_page_route_v0.md`、`scripts/export_system_compiler_front_page_route.py`
    与 `scripts/validate_system_compiler_front_page_route.py`
  - 用途偏向把一个 root summary 的 `front_page` 消费路径收成可验证的 route 对象，
    明确记录 supporting surface 展开、revisit 与 cycle

- `system_compiler.front_page_route_compare.v0.schema.json`
  - 对应 `docs/system/system_compiler_front_page_route_compare_v0.md`、`scripts/compare_system_compiler_front_page_route.py`
    与 `scripts/validate_system_compiler_front_page_route_compare.py`
  - 用途偏向比较两份 `front_page route` 总结对象，回答消费路径如何变化、哪些 level-1
    surface 出现或消失，以及候选 route 是否更丰富还是发生了 consumer-facing drift

- `system_compiler.front_page_entry_capability.v0.schema.json`
  - 对应 `docs/system/system_compiler_front_page_entry_capability_v0.md`、`scripts/export_system_compiler_front_page_entry_capability.py`
    与 `scripts/validate_system_compiler_front_page_entry_capability.py`
  - 用途偏向把一份 `front_page route` 总结对象收成“这个入口已经具备哪些 explain 能力”的能力表，
    明确推荐默认 landing mode、能力 tier、首选入口与 provenance hints

- `system_compiler.front_page_entry_landing.v0.schema.json`
  - 对应 `docs/system/system_compiler_front_page_entry_landing_v0.md`、`scripts/export_system_compiler_front_page_entry_landing.py`
    与 `scripts/validate_system_compiler_front_page_entry_landing.py`
  - 用途偏向把一份 `front_page entry capability` 总结对象进一步收成更薄的 open-plan，
    明确 primary landing、secondary tabs、fallback mode order 与可展开 provenance roots

- `system_compiler.front_page_entry_landing_compare.v0.schema.json`
  - 对应 `docs/system/system_compiler_front_page_entry_landing_compare_v0.md`、`scripts/compare_system_compiler_front_page_entry_landing.py`
    与 `scripts/validate_system_compiler_front_page_entry_landing_compare.py`
  - 用途偏向比较两份 `front_page entry landing` 总结对象，回答默认 landing、direct mode、
    tab 集合与 provenance roots 是否发生 consumer-facing drift

- `examples/minimal_kernel.runtime_evidence_bundle.summary.v1.sample.json`
  - 对应 `minimal_kernel.runtime_evidence_bundle.summary/v1` 的最小样例
  - 用途偏向 witness bundle sample 输入与该 summary 协议的补充样例锚点

- `system_compiler.runtime_observe_snapshot.v0.schema.json`
  - 对应 per-case runtime observe sidecar 的最小机器可读协议
  - 用途偏向把 `PublishState / ExportState / recent_transitions` 从示例内存里的瞬时数据，提升成可引用、可验证的独立工件
  - 它服务于 `export_case_manifest -> export_bundle -> artifact_report` 这条输入链，但不替代 `artifact report` 自身

- `examples/system_compiler.runtime_observe_snapshot.v0.sample.json`
  - 对应 `system_compiler.runtime_observe_snapshot/v0` 的最小样例
  - 用途偏向 sidecar schema 自检与后续真实 runtime 导出链对形状的对齐

- `system_compiler_result_map.v0.schema.json`
  - 对应 `inspect_system_compiler_artifact_report.ps1` 导出的 `system_compiler_summary.result_map`
    与 `comparison.system_compiler_summary.result_map`
  - 用途偏向把 system compiler root summary 里的 stage ownership、root/block/summary field relation、
    case projection fallback source 正式锚定成机器可读语言
  - 它当前是 v0 草案协议，负责冻结对象形状与关系语义，不替代更严格的脚本契约校验

- `examples/system_compiler_result_map.summary.v0.sample.json`
  - 对应 `system_compiler_result_map/v0` 在 `mode = summary` 下的最小样例
  - 用途偏向 schema 自检、字段讨论与 explain surface 工具接入前的锚点

- `examples/system_compiler_result_map.comparison.v0.sample.json`
  - 对应 `system_compiler_result_map/v0` 在 `mode = comparison` 下的最小样例
  - 用途偏向 comparison drift 工具接入前的样例锚点

- `system_compiler_summary.v0.schema.json`
  - 对应 `inspect_system_compiler_artifact_report.ps1` 导出的 `system_compiler_summary`
    与 `comparison.system_compiler_summary`
  - 用途偏向把 artifact_root 默认总览里的 root-level system compiler result object
    正式锚定成可验证协议，并把 `kind / mode` 自描述字段固定下来
  - 它当前会继续直接引用 `system_compiler_result_map/v0`，把 result object 与 relation language 收进同一对象边界

- `examples/system_compiler_summary.summary.v0.sample.json`
  - 对应 `system_compiler_summary/v0` 在 `mode = summary` 下的最小样例
  - 用途偏向 schema 自检、artifact_root 总览消费面接入与 explain surface 工具对齐

- `examples/system_compiler_summary.comparison.v0.sample.json`
  - 对应 `system_compiler_summary/v0` 在 `mode = comparison` 下的最小样例
  - 用途偏向 comparison drift 总结果物接入前的样例锚点

- `system_input_summary.v0.schema.json`
  - 对应 `inspect_system_compiler_artifact_report.ps1` 导出的 `system_input_summary`
    与 `comparison.system_input_summary`
  - 用途偏向把 artifact_root 默认总览里的 input-side summary object
    正式锚定成可验证协议，并把 `kind / mode` 自描述字段固定下来
  - 它当前负责冻结 normalized input、fact / contract matrix 与 input drift summary 的对象形状

- `examples/system_input_summary.summary.v0.sample.json`
  - 对应 `system_input_summary/v0` 在 `mode = summary` 下的最小样例
  - 用途偏向 schema 自检、artifact_root 输入侧总览消费面接入与 explain surface 工具对齐

- `examples/system_input_summary.comparison.v0.sample.json`
  - 对应 `system_input_summary/v0` 在 `mode = comparison` 下的最小样例
  - 用途偏向 input drift 汇总对象接入前的样例锚点

- `binding_result_summary.v0.schema.json`
  - 对应 `inspect_system_compiler_artifact_report.ps1` 导出的 `binding_result_summary`
    与 `comparison.binding_result_summary`
  - 用途偏向把 artifact_root 默认总览里的 binding-side summary object
    正式锚定成可验证协议，并把 `kind / mode` 自描述字段固定下来
  - 它当前负责冻结 binding hotspot、resolved/unresolved capability 分布与 binding drift 汇总对象形状

- `examples/binding_result_summary.summary.v0.sample.json`
  - 对应 `binding_result_summary/v0` 在 `mode = summary` 下的最小样例
  - 用途偏向 schema 自检、artifact_root binding-side 总览消费面接入与 explain surface 工具对齐

- `examples/binding_result_summary.comparison.v0.sample.json`
  - 对应 `binding_result_summary/v0` 在 `mode = comparison` 下的最小样例
  - 用途偏向 binding drift 汇总对象接入前的样例锚点

- `bringup_order_summary.v0.schema.json`
  - 对应 `inspect_system_compiler_artifact_report.ps1` 导出的 `bringup_order_summary`
    与 `comparison.bringup_order_summary`
  - 用途偏向把 artifact_root 默认总览里的 bringup-side summary object
    正式锚定成可验证协议，并把 `kind / mode` 自描述字段固定下来
  - 它当前负责冻结 bringup node hotspot、blocked node 分布与 bringup drift 汇总对象形状

- `examples/bringup_order_summary.summary.v0.sample.json`
  - 对应 `bringup_order_summary/v0` 在 `mode = summary` 下的最小样例
  - 用途偏向 schema 自检、artifact_root bringup-side 总览消费面接入与 explain surface 工具对齐

- `examples/bringup_order_summary.comparison.v0.sample.json`
  - 对应 `bringup_order_summary/v0` 在 `mode = comparison` 下的最小样例
  - 用途偏向 bringup drift 汇总对象接入前的样例锚点

- `system_formation_summary.v0.schema.json`
  - 对应 `inspect_system_compiler_artifact_report.ps1` 导出的 `system_formation_summary`
    与 `comparison.system_formation_summary`
  - 用途偏向把 artifact_root 默认总览里的 formation-side summary object
    正式锚定成可验证协议，并把 `kind / mode` 自描述字段固定下来
  - 它当前负责冻结 formation status、blocker 分布与 formation drift 汇总对象形状

- `examples/system_formation_summary.summary.v0.sample.json`
  - 对应 `system_formation_summary/v0` 在 `mode = summary` 下的最小样例
  - 用途偏向 schema 自检、artifact_root formation-side 总览消费面接入与 explain surface 工具对齐

- `examples/system_formation_summary.comparison.v0.sample.json`
  - 对应 `system_formation_summary/v0` 在 `mode = comparison` 下的最小样例
  - 用途偏向 formation drift 汇总对象接入前的样例锚点

- `fact_resolution_summary.v0.schema.json`
  - 对应 `inspect_system_compiler_artifact_report.ps1` 导出的 `fact_resolution_summary`
    与 `comparison.fact_resolution_summary`
  - 用途偏向把 artifact_root 默认总览里的 fact-resolution-side summary object
    正式锚定成可验证协议，并把 `kind / mode` 自描述字段固定下来
  - 它当前负责冻结 fact inventory、contract drift 与 resource hotspot 汇总对象形状

- `examples/fact_resolution_summary.summary.v0.sample.json`
  - 对应 `fact_resolution_summary/v0` 在 `mode = summary` 下的最小样例
  - 用途偏向 schema 自检、artifact_root fact-resolution-side 总览消费面接入与 explain surface 工具对齐

- `examples/fact_resolution_summary.comparison.v0.sample.json`
  - 对应 `fact_resolution_summary/v0` 在 `mode = comparison` 下的最小样例
  - 用途偏向 fact inventory / contract drift 汇总对象接入前的样例锚点

## 稳定性约定

当前建议这样理解稳定性：

- `export_case_manifest/v1`：当前 `materialized_graph` 批量导出链依赖的 case manifest 输入协议
- `sample/v2`：当前支持、显式校验，但不承诺长期冻结
- `export_bundle/v1`：当前脚本链稳定依赖的 bundle 索引协议
- `bundle_diff/v1`：当前 diff / report 链稳定依赖的差异协议
- `ci_summary/v1`：当前 CI / workflow 层稳定依赖的摘要协议
- `report_manifest/v1`：当前报告层稳定依赖的工件元数据协议
- `runtime_observe_snapshot/v0`：当前 runtime 观察输入 sidecar 的最小协议，用于把动态观察事实稳定挂接到 bundle / report 链
- `system_compiler.artifact_report/v0`：当前 system compiler 输出面的对象草案锚点，字段仍允许继续收敛
- `system_compiler.canonical_world/v0`：当前“一个世界想证明什么、依赖哪些 witness / contracts”的声明对象锚点
- `system_compiler.witness_bundle/v0`：当前“这次交付拿什么作证”的交付对象锚点
- `system_compiler.world_compare/v0`：当前“这个世界相对基线还站不站得住”的 compare verdict 对象锚点
- `system_compiler.biography/v0`：当前“如何把证据世界压成顶层交付封面并留下继续追问入口”的 biography 对象锚点
- `system_compiler.biography_index/v0`：当前“如何把多个 biography 摆成一个可交付、可审阅的 world shelf”的 directory object 锚点
- `system_compiler.biography_index_compare/v0`：当前“如何把两个 biography shelf 收成一个可验证、可门禁、可交付的 shelf compare verdict 对象”的锚点
- `system_compiler.world_shelf_review/v0`：当前“如何把 world shelf review seam 自己收成一个可验证、可发布、可回归的 review envelope 对象”的锚点
- `system_compiler_summary/v0`：当前 artifact_root 默认总览里的 root-level system compiler result object 锚点，负责冻结 summary/comparison 两种模式下的对象形状与 `kind / mode` 自描述语义
- `system_input_summary/v0`：当前 artifact_root 默认总览里的 input-side summary object 锚点，负责冻结 summary/comparison 两种模式下的对象形状与 `kind / mode` 自描述语义
- `binding_result_summary/v0`：当前 artifact_root 默认总览里的 binding-side summary object 锚点，负责冻结 summary/comparison 两种模式下的对象形状与 `kind / mode` 自描述语义
- `bringup_order_summary/v0`：当前 artifact_root 默认总览里的 bringup-side summary object 锚点，负责冻结 summary/comparison 两种模式下的对象形状与 `kind / mode` 自描述语义
- `system_formation_summary/v0`：当前 artifact_root 默认总览里的 formation-side summary object 锚点，负责冻结 summary/comparison 两种模式下的对象形状与 `kind / mode` 自描述语义
- `fact_resolution_summary/v0`：当前 artifact_root 默认总览里的 fact-resolution-side summary object 锚点，负责冻结 summary/comparison 两种模式下的对象形状与 `kind / mode` 自描述语义
- `system_compiler_result_map/v0`：当前 system compiler root summary 关系语言的对象锚点，语义继续由脚本契约与样例共同收紧

也就是说，Charm 当前不是在假装“所有导出都已经终局稳定”，
而是在把不同层次的协议边界分别钉清楚。

## 配套文档

- 设计说明：`docs/system/init_materialized_graph_observe.md`
- 输出面：`docs/system/explain_surface_v0.md`
- 统一报告对象：`docs/system/artifact_report_v0.md`
- 方法论复盘：`docs/architecture/charm_methodology_charter.md`
