# Capability Route

## 适用场景

- capability map
- Capability Contract 准入
- 能力命名
- 能力归属与装配可见性

## 最短阅读顺序

1. [`../../../CONSTITUTION.md`](../../../CONSTITUTION.md)
2. [`../../architecture/charm_core_contract.md`](../../architecture/charm_core_contract.md)
3. [`../../README.md`](../../README.md)
4. [`../../capability_map.md`](../../capability_map.md)（supporting 实现盘点）
5. [`../../architecture_overview.md`](../../architecture_overview.md)（supporting 实现盘点）
6. [`../skills/charm-capability-map/SKILL.md`](../skills/charm-capability-map/SKILL.md)
7. [`../rules/charm-architecture.md`](../rules/charm-architecture.md)

## 先不要做什么

- 不要先发明新能力名，再回头补 Core 理由。
- 不要把现有 capability 名称、接口或 map 条目视为自动获准的 Capability Contract。
- 不要把 Provider 角色实体化为公共基类、Manager 或 Registry。
- 不要把阶段性装配写成默认路径。

## 完成前自检

- 是否通过 Constitution 六问并获得明确裁决。
- 消费方依赖的是行为契约，还是具体实现身份。
- 正反例和失败语义能否跨实现、跨运行环境重复证明。
- 若只是在更新现有实现 map，是否明确保持 supporting 范围。
