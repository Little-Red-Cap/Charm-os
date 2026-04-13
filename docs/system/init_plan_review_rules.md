# Init Plan Review 规则

这份规则只做红绿灯，不做百科全书。

## 绿灯
- 新增装配代码优先写成 `Recipe + Plan`
- 默认通过 `start_plan(...)`、`build_graph(...)`、`start_graph(...)` 落地
- 框架内 `*Chain` / `CoreSystemChain` 优先暴露并使用 `plan()`
- 单节点 binding 优先写成 `as_plan(binding)`
- 可选装配单元优先写成 `maybe(optional_item)`
- multi-node legacy 优先暴露 `for_each_legacy_node(...)` 这类内部遍历协议
- 旧式 chain 的 `node_span()` 只保留为通用 compat fallback，不再作为默认适配面；框架内 `*Chain` 不再新增或保留公开 `node_span()`

## 红灯
- 业务 / 驱动接入层直接写 `init::Node`
- 新增代码把 `node_span()` 当作组合接口或默认接入表面
- 新增业务代码把 `legacy(...)` 当常规装配语法糖使用

## 迁移例外
- `legacy(...)` 只用于旧 chain 迁移与少量兼容胶水
- `as_plan(...)` 用于单节点 binding 或已有 `plan()` 的装配对象
- `maybe(...)` 用于 `optional` 链或 `optional` 装配单元
- `compat_nodes(...)` 只用于旧节点数组兼容；`legacy_nodes(...)` 仅保留为兼容别名；`wrap_nodes_with_requires(...)` 若不得不用，也优先接 `for_each_legacy_node(...)` 或显式原始节点数组
- 需要导出阶段完成能力时，用 `ready_as(...)` 或显式 barrier，不让 `Plan` 继承产出
