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

- `materialized_graph.sample.v2.schema.json`
  - 对应 `format_json_sample(...)` 当前导出的样例协议
  - 用途偏向字段勘探、原型接入、脚本分析
  - 它是“当前受支持的样例协议”，不是长期冻结协议

- `materialized_graph.export_bundle.v1.schema.json`
  - 对应 `scripts/export_materialized_graph.ps1` 生成的 `index.json`
  - 用途偏向批量导出结果组织、bundle 检视与 diff
  - 它已经是当前脚本链的稳定消费面之一，并且现在也可承载 per-case `subject` 元数据

- `materialized_graph.bundle_diff.v1.schema.json`
  - 对应 `scripts/diff_materialized_graph_bundle.ps1 -AsJson` 的输出
  - 用途偏向结构差异分析、报告生成前的数据交换、工具侧增量审阅
  - 它已经是 diff / report / CI 这条链上的机器可读中间协议，并可继续带出左右 case 的 `subject` 视图

- `materialized_graph.ci_summary.v1.schema.json`
  - 对应 `scripts/ci_materialized_graph_bundle.ps1` 生成的 `summary.json`
  - 用途偏向 CI 编排、状态汇总、上层自动化消费
  - 它已经是当前 CI / 工作流消费面之一，并且现在也可引用生成出的 `artifact report` 与 `subject_defaults`

- `materialized_graph.report_manifest.v1.schema.json`
  - 对应 `scripts/report_materialized_graph_bundle.ps1` 生成的 `report manifest`
  - 用途偏向报告工件发现、报告层元数据交换、上层工具对 Markdown / HTML 的稳定引用
  - 它把“报告本身”从纯文件输出推进成可被自动化消费的对象

### `system_compiler` 输出面草案

- `system_compiler.artifact_report.v0.schema.json`
  - 对应 `docs/system/artifact_report_v0.md` 中定义的最小统一报告对象
  - 用途偏向字段收敛、样例校验、后续脚本/CI 接入前的协议锚定
  - 它当前是 v0 草案协议，已能覆盖 `export_only` 与 `compare` 两种最小输出场景

- `examples/system_compiler.artifact_report.v0.sample.json`
  - 对应 `system_compiler.artifact_report/v0` 的最小机器可验样例
  - 用途偏向 schema 自检、字段讨论与后续脚本接入前的样例锚点

## 稳定性约定

当前建议这样理解稳定性：

- `export_case_manifest/v1`：当前 `materialized_graph` 批量导出链依赖的 case manifest 输入协议
- `sample/v2`：当前支持、显式校验，但不承诺长期冻结
- `export_bundle/v1`：当前脚本链稳定依赖的 bundle 索引协议
- `bundle_diff/v1`：当前 diff / report 链稳定依赖的差异协议
- `ci_summary/v1`：当前 CI / workflow 层稳定依赖的摘要协议
- `report_manifest/v1`：当前报告层稳定依赖的工件元数据协议
- `system_compiler.artifact_report/v0`：当前 system compiler 输出面的对象草案锚点，字段仍允许继续收敛

也就是说，Charm 当前不是在假装“所有导出都已经终局稳定”，
而是在把不同层次的协议边界分别钉清楚。

## 配套文档

- 设计说明：`docs/system/init_materialized_graph_observe.md`
- 输出面：`docs/system/explain_surface_v0.md`
- 统一报告对象：`docs/system/artifact_report_v0.md`
- 方法论复盘：`docs/architecture/charm_methodology_charter.md`
