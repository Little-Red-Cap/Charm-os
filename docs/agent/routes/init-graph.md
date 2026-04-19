# Init Graph Route

## 适用场景

- `init.graph` / 装配问题
- 初始化顺序争议
- `provides / requires` 接线

## 最短阅读顺序

1. [`../../system/init_graph_contract.md`](../../system/init_graph_contract.md)
2. [`../../system/service_component_init.md`](../../system/service_component_init.md)
3. [`../skills/charm-init-graph/SKILL.md`](../skills/charm-init-graph/SKILL.md)
4. [`../rules/charm-architecture.md`](../rules/charm-architecture.md)

## 先不要做什么

- 不要在入口手写初始化顺序。
- 不要让 runtime / domain 反向侵入 foundation。
- 不要跳过装配文档同步。

## 完成前自检

- `provides / requires` 是否闭环。
- 初始化顺序是否来自图，而不是人工约定。
- 相关契约和入口是否已同步。
