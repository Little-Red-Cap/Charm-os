# Init / Observe 示例入口

装配规则见 [`init_graph_contract.md`](../../docs/system/init_graph_contract.md)，观察与 schema 见
[`init_materialized_graph_observe.md`](../../docs/system/init_materialized_graph_observe.md) 和
[`schemas/README.md`](../../schemas/README.md)。

| 示例 | 覆盖 |
|---|---|
| `materialize_observe_demo/` | 最小 Plan -> materialized graph -> DOT/JSON |
| `connection_observe_demo/` | static source/sink/mode wiring 的 materialize/observe |
| `bringup_block_observe_demo/` | `BringupBlock + FileInitChain` 组合导出 |
| `bringup_minimal_observe_demo/` | `BringupMinimal + BoardCaps` 系统入口导出 |

这些示例验证导出和观察 fixture，不替代 init graph 行为契约或真实平台 bring-up。

case、target 与默认输出由
[`materialized_graph.export_case_manifest.v1.json`](../../scripts/materialized_graph.export_case_manifest.v1.json)
维护。使用 `scripts/export_materialized_graph.ps1 -ListCases` 查询，或通过 `-Case <name>`、
`-AllCases -OutputRoot <path>` 导出；不要在各示例 README 复制同一清单。
