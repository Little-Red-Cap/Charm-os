# Capability Route

## 适用场景

- capability map
- 能力命名
- 能力归属与装配可见性

## 最短阅读顺序

1. [`../../README.md`](../../README.md)
2. [`../../capability_map.md`](../../capability_map.md)
3. [`../../architecture_overview.md`](../../architecture_overview.md)
4. [`../skills/charm-capability-map/SKILL.md`](../skills/charm-capability-map/SKILL.md)
5. [`../rules/charm-architecture.md`](../rules/charm-architecture.md)

## 先不要做什么

- 不要先发明新能力名，再回头找现行入口。
- 不要绕过 `docs/capability_map.md` 的首用决策表直接扩写结论。
- 不要把阶段性装配写成默认路径。

## 完成前自检

- capability 名称是否稳定、清楚、可路由。
- 首用路径是否说得出“先用什么、默认在哪、何时例外”。
- 与 init.graph、registry、上位总览是否一致。
