# Init / Observe 示例入口

装配规则见 [`init_graph_contract.md`](../../docs/system/init_graph_contract.md)，观察与 schema 见
[`init_materialized_graph_observe.md`](../../docs/system/init_materialized_graph_observe.md) 和
[`schemas/README.md`](../../schemas/README.md)。

| 示例 | 覆盖 |
|---|---|
| [`materialize_observe_demo`](materialize_observe_demo/README.md) | Plan -> materialized graph -> DOT/JSON |
| [`bringup_minimal_observe_demo`](bringup_minimal_observe_demo/README.md) | bring-up helper 的观察导出 |
| `bringup_block_observe_demo/` | block + bring-up 组合 |
| [`connection_observe_demo`](connection_observe_demo/README.md) | connection materialize/observe |

这些示例验证导出和观察 fixture，不替代 init graph 行为契约或真实平台 bring-up。
