# bringup block observe demo

这个示例把一个更贴近真实 bringup 的组合导出成 `materialized_graph`：

- `BringupBlock`
- `FileInitChain`

它的目标不是运行文件块设备，而是验证：系统 bringup helper 组合出的装配结果，也能直接成为 `materialize(...)` 之后的观察与工具消费输入。

默认输出文件：

- `bringup_block_materialized_graph.dot`
- `bringup_block_materialized_graph.sample.json`

如果已经完成 CMake 配置，也可以直接使用导出目标：

- `cmake --build <build-dir> --target export_bringup_block_materialized_graph`

仓库根目录脚本也支持把它当作一个批量导出 case：

- `scripts/export_materialized_graph.ps1 -Case bringup-block-observe-demo`
