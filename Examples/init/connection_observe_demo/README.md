# connection observe demo

这个示例用于演示 `init.connection` 如何把静态 `source / sink / mode` wiring
materialize 成 `kind=connection` 的观察节点，并导出：

- `DOT`
- `JSON sample`

详细说明见：

- `docs/architecture/signal_state_contract_v0.md`
- `docs/system/init_materialized_graph_observe.md`

默认输出文件：

- `connection_graph.dot`
- `connection_graph.sample.json`

如果已经完成 CMake 配置，也可以直接用导出目标：

- `cmake --build <build-dir> --target export_connection_graph_demo`

可选参数：

- `--dot <path>`
- `--json <path>`
