# Capability Route

## 适用场景

- capability map
- 能力命名
- 能力归属与装配可见性

## 最短阅读顺序

1. [`../../capability_map.md`](../../capability_map.md)
2. [`../../architecture_overview.md`](../../architecture_overview.md)
3. [`../skills/charm-capability-map/SKILL.md`](../skills/charm-capability-map/SKILL.md)
4. [`../rules/charm-architecture.md`](../rules/charm-architecture.md)

## 先不要做什么

- 不要先讨论实现细节，忽略能力边界。
- 不要让 capability 命名和装配入口脱节。
- 不要把临时实现细节写成长期能力模型。

## 完成前自检

- capability 名称是否稳定、清楚、可路由。
- 能力归属层是否明确。
- 与 init.graph、registry、上位总览是否一致。
