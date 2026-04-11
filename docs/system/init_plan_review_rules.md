# Init Plan Review 规则

这份规则只做红绿灯，不做百科全书。

## 绿灯
- 新增装配代码优先写成 `Recipe + Plan`
- 默认通过 `start_plan(...)`、`build_graph(...)`、`start_graph(...)` 落地
- 框架内 `*Chain` / `CoreSystemChain` 优先暴露并使用 `plan()`
- 可选装配单元优先写成 `maybe(optional_item)`

## 红灯
- 业务 / 驱动接入层直接写 `init::Node`
- 新增代码把 `node_span()` 当作组合接口或默认接入表面
- 新增业务代码把 `legacy(...)` 当常规装配语法糖使用

## 迁移例外
- `legacy(...)` 只用于迁移、单节点 legacy binding 与框架胶水
- `maybe(...)` 用于 `optional` 链或 `optional` 装配单元
- `legacy_nodes(...)` 只用于旧节点数组兼容
- 需要导出阶段完成能力时，用 `ready_as(...)` 或显式 barrier，不让 `Plan` 继承产出
