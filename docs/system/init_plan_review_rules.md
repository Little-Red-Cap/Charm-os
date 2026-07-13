# Init Plan Review 规则

> **文档状态：`supporting`**

本文列出装配代码评审的允许项、拒绝项与迁移例外。

它受 [`init_graph_contract.md`](init_graph_contract.md) 约束，并对应
`Modules/init/init.recipe.cppm`、`init.plan.cppm`、`init.barrier.cppm` 与 `init.materialize.cppm`。
Recipe/Plan 是当前装配实现，不是 Charm Core 或跨运行环境的统一系统模型。

## 绿灯
- 新增装配代码优先写成 `Recipe + Plan`
- 默认通过 `start_plan(...)`、`build_graph(...)`、`start_graph(...)` 落地
- 框架内 `*Chain` / `CoreSystemChain` 优先暴露并使用 `plan()`
- 单节点 binding 优先写成 `as_plan(binding)`
- 可选装配单元优先写成 `maybe(optional_item)`
- 框架级 bringup helper 只暴露 `start_plan(...)` 这类 Plan 入口

## 红灯
- 业务 / 驱动接入层直接写 `init::Node`
- 新增代码把 `node_span()` 当作组合接口或默认接入表面
- 新增业务代码继续依赖 `legacy(...)`、`compat_nodes(...)` 或 `start(..., extra_nodes)`

## 迁移例外
- `as_plan(...)` 用于单节点 binding 或已有 `plan()` 的装配对象
- `maybe(...)` 用于 `optional` 链或 `optional` 装配单元
- 遗留 `node_span()` / raw `Node*` span 需要先收敛成 `plan()` 或单节点 binding，再接入新装配表面
- 需要导出阶段完成能力时，用 `ready_as(...)` 或显式 barrier，不让 `Plan` 继承产出

早期分层与合并规则见
[`../archive/system-evidence-and-staging-v0/init_plan_retained_notes.md`](../archive/system-evidence-and-staging-v0/init_plan_retained_notes.md)。
它只保留设计取舍，不说明迁移完成度。
