# `init.materialize` 观察导出面

> **文档状态：`supporting`**

本文记录当前 `materialized_graph` 的只读观察与导出边界。行为以模块源码、示例与脚本为准。

## 实现入口

- `Modules/init/init.materialize.cppm`：把 Plan 规范化为 `materialized_graph`；
- `Modules/init/init.observe.cppm`：提供 node/edge/graph view，并格式化 DOT 和 JSON sample；
- `Examples/init/materialize_observe_demo`：最小导出样例；
- `scripts/export_materialized_graph.ps1`：脚本导出入口；
- `scripts/ci_materialized_graph_bundle.ps1`：批量证据入口。

## 当前承诺

- 工具读取 materialize 后的只读视图，不依赖输入 Plan 的具体语法；
- node view 包含 index、name、phase、runlevel、kind、provides/requires 和 connection 字段；
- edge view 表达 capability/ordering/connection 关系；
- DOT 与 JSON 是工具输出，不是 Charm Core、runtime topology 或统一系统模型；
- 导出结果不能反向修改 graph，也不能证明 graph 中的硬件或 runtime 已实际运行。

## 未承诺

- 稳定跨版本 wire ABI；
- 全局唯一 capability vocabulary；
- runtime discovery、ownership 或 hot-plug 状态；
- materialized graph 等于整个系统；
- DOT/JSON sample 是产品配置格式。

行为变化应以模块源码、示例输出和脚本 smoke 为证据。
