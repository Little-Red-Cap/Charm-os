# materialize observe demo

这个示例用于把一个最小 `Plan` materialize 成 `materialized_graph`，再导出：

- `DOT`
- `JSON sample`

详细说明见：

- `docs/system/init_materialized_graph_observe.md`

默认输出文件：

- `materialized_graph.dot`
- `materialized_graph.sample.json`

可选参数：

- `--dot <path>`
- `--json <path>`
