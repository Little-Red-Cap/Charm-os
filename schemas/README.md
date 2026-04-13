# Materialized Graph Schemas

这个目录存放 `materialized_graph` 观察导出链当前已经公开的机器可读协议文件。

它们的目标不是替代 `docs/system/init_materialized_graph_observe.md` 的设计说明，
而是给脚本、CI、IDE 原型、外部工具一个明确的协议锚点。

## 当前协议分层

- `materialized_graph.sample.v2.schema.json`
  - 对应 `format_json_sample(...)` 当前导出的样例协议
  - 用途偏向字段勘探、原型接入、脚本分析
  - 它是“当前受支持的样例协议”，不是长期冻结协议

- `materialized_graph.export_bundle.v1.schema.json`
  - 对应 `scripts/export_materialized_graph.ps1` 生成的 `index.json`
  - 用途偏向批量导出结果组织、bundle 检视与 diff
  - 它已经是当前脚本链的稳定消费面之一

- `materialized_graph.bundle_diff.v1.schema.json`
  - 对应 `scripts/diff_materialized_graph_bundle.ps1 -AsJson` 的输出
  - 用途偏向结构差异分析、报告生成前的数据交换、工具侧增量审阅
  - 它已经是 diff / report / CI 这条链上的机器可读中间协议

- `materialized_graph.ci_summary.v1.schema.json`
  - 对应 `scripts/ci_materialized_graph_bundle.ps1` 生成的 `summary.json`
  - 用途偏向 CI 编排、状态汇总、上层自动化消费
  - 它已经是当前 CI / 工作流消费面之一

- `materialized_graph.report_manifest.v1.schema.json`
  - 对应 `scripts/report_materialized_graph_bundle.ps1` 生成的 `report manifest`
  - 用途偏向报告工件发现、报告层元数据交换、上层工具对 Markdown / HTML 的稳定引用
  - 它把“报告本身”从纯文件输出推进成可被自动化消费的对象

## 稳定性约定

当前建议这样理解稳定性：

- `sample/v2`：当前支持、显式校验，但不承诺长期冻结
- `export_bundle/v1`：当前脚本链稳定依赖的 bundle 索引协议
- `bundle_diff/v1`：当前 diff / report 链稳定依赖的差异协议
- `ci_summary/v1`：当前 CI / workflow 层稳定依赖的摘要协议
- `report_manifest/v1`：当前报告层稳定依赖的工件元数据协议

也就是说，Charm 当前不是在假装“所有导出都已经终局稳定”，
而是在把不同层次的协议边界分别钉清楚。

## 配套文档

- 设计说明：`docs/system/init_materialized_graph_observe.md`
- 方法论复盘：`docs/architecture/charm_methodology_charter.md`
