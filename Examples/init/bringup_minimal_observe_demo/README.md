# bringup minimal observe demo

这个示例把更接近完整系统入口的 `BringupMinimal` 直接 materialize 并导出：

- `BringupMinimal`
- `BoardCaps`
- `console alias / input / can` 等 board bringup 组合

它的目标是验证：系统入口 helper 不只是能 `start_graph(...)`，也能先产出一份稳定、可观察、可导出的装配结果。

默认输出文件：

- `bringup_minimal_materialized_graph.dot`
- `bringup_minimal_materialized_graph.sample.json`

如果已经完成 CMake 配置，也可以直接使用导出目标：

- `cmake --build <build-dir> --target export_bringup_minimal_materialized_graph`

仓库根目录脚本也支持把它当作一个批量导出 case：

- `scripts/export_materialized_graph.ps1 -Case bringup-minimal-observe-demo`
