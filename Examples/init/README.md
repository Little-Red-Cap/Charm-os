# Init / Observe 示例入口

> `status`: `supporting`
>
> `scope`: init graph materialize/observe fixture 路由

装配规则见 [`init_graph_contract.md`](../../docs/system/init_graph_contract.md)，观察与 schema 见
[`init_materialized_graph_observe.md`](../../docs/system/init_materialized_graph_observe.md) 和
[`schemas/README.md`](../../schemas/README.md)。

本目录覆盖最小 graph、connection wiring 和 bring-up 组合的导出/观察 fixture；它们不替代 init graph
行为契约或真实平台 bring-up。

准确 case、target 与默认输出由
[`materialized_graph.export_case_manifest.v1.json`](../../scripts/materialized_graph.export_case_manifest.v1.json)
维护。使用 `scripts/export_materialized_graph.ps1 -ListCases` 查询，或通过 `-Case <name>`、
`-AllCases -OutputRoot <path>` 导出；README 不复制 case inventory。
