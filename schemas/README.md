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
- `system_compiler_summary/v0`：当前 artifact_root 默认总览里的 root-level system compiler result object 锚点，负责冻结 summary/comparison 两种模式下的对象形状与 `kind / mode` 自描述语义
- `system_input_summary/v0`：当前 artifact_root 默认总览里的 input-side summary object 锚点，负责冻结 summary/comparison 两种模式下的对象形状与 `kind / mode` 自描述语义
- `binding_result_summary/v0`：当前 artifact_root 默认总览里的 binding-side summary object 锚点，负责冻结 summary/comparison 两种模式下的对象形状与 `kind / mode` 自描述语义
- `bringup_order_summary/v0`：当前 artifact_root 默认总览里的 bringup-side summary object 锚点，负责冻结 summary/comparison 两种模式下的对象形状与 `kind / mode` 自描述语义
- `system_formation_summary/v0`：当前 artifact_root 默认总览里的 formation-side summary object 锚点，负责冻结 summary/comparison 两种模式下的对象形状与 `kind / mode` 自描述语义
- `system_compiler_result_map/v0`：当前 system compiler root summary 关系语言的对象锚点，语义继续由脚本契约与样例共同收紧

也就是说，Charm 当前不是在假装“所有导出都已经终局稳定”，
而是在把不同层次的协议边界分别钉清楚。

## 配套文档

- 设计说明：`docs/system/init_materialized_graph_observe.md`
- 输出面：`docs/system/explain_surface_v0.md`
- 统一报告对象：`docs/system/artifact_report_v0.md`
- 方法论复盘：`docs/architecture/charm_methodology_charter.md`
