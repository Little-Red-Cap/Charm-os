# Init Graph Route

## 文档状态

- `status`: `supporting`
- `scope`: `init.graph`、provides/requires 与启动顺序路由

## 最短路径

1. [Init graph contract](../../system/init_graph_contract.md)
2. [`init.graph`](../../../Modules/core/init/init.graph.cppm) 与
   [`init.node`](../../../Modules/core/init/init.node.cppm)
3. [Init graph skill](../skills/charm-init-graph/SKILL.md)
4. [架构规则](../rules/charm-architecture.md)

初始化顺序必须来自图；missing、duplicate、cycle、phase 和容量失败不能由手写顺序或 fallback 隐藏。
