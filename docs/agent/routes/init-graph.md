# Init Graph Route

## 适用场景

- `init.graph` / 装配问题
- 初始化顺序争议
- `provides / requires` 接线

## 最短阅读顺序

1. [`../../system/init_graph_contract.md`](../../system/init_graph_contract.md)
2. [`../../../Modules/core/init/init.graph.cppm`](../../../Modules/core/init/init.graph.cppm)
3. [`../../../Modules/core/init/init.node.cppm`](../../../Modules/core/init/init.node.cppm)
4. [`../skills/charm-init-graph/SKILL.md`](../skills/charm-init-graph/SKILL.md)
5. [`../rules/charm-architecture.md`](../rules/charm-architecture.md)

## 先不要做什么

- 不要在入口手写初始化顺序。
- 不要从目录或旧 Service/Component 分层推断依赖。
- 不要用 fallback 隐藏 missing/duplicate/cycle/phase 错误。

## 完成前自检

- `provides / requires` 是否闭环。
- 初始化顺序是否来自图，而不是人工约定。
- runlevel、phase、容量和首错停止行为是否已覆盖。
