# `init.materialize` 观察导出面

> **文档状态：`supporting`**

本文记录当前 `materialized_graph` 的只读观察与导出边界。行为以模块源码、示例与脚本为准。

## 实现入口

`init.materialize` 将 Plan 规范化为 graph，`init.observe` 提供只读 view 与 DOT/JSON sample。示例和
导出脚本从 [`Examples/init/README.md`](../../Examples/init/README.md) 进入，具体入口不在本文复制。

## 当前承诺

- 工具读取 materialize 后的只读视图，不依赖输入 Plan 的具体语法；
- node/edge view 表达当前源码定义的节点、capability、ordering 与 connection 关系；
- DOT 与 JSON 是工具输出，不是 Charm Core、runtime topology 或统一系统模型；
- 导出结果不能反向修改 graph，也不能证明 graph 中的硬件或 runtime 已实际运行。

## 未承诺

- 稳定跨版本 wire ABI；
- 全局唯一 capability vocabulary；
- runtime discovery、ownership 或 hot-plug 状态；
- materialized graph 等于整个系统；
- DOT/JSON sample 是产品配置格式。

行为变化应以模块源码、示例输出和脚本 smoke 为证据。
