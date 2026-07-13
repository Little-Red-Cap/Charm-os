# Charm 文档入门

## 文档状态

- `status`: `supporting`
- `scope`: 兼容 onboarding 入口
- `authority`: [`../CONSTITUTION.md`](../CONSTITUTION.md) 与 [`README.md`](README.md)

本页只提供短路径，不定义 Charm Core，也不维护实现进度。权威路由见 [`README.md`](README.md)，
实现地图见 [`architecture_overview.md`](architecture_overview.md)。

## 推荐顺序

1. [`../CONSTITUTION.md`](../CONSTITUTION.md)：Core 准入与裁决等级。
2. [`architecture/charm_core_contract.md`](architecture/charm_core_contract.md)：Capability Contract、
   Requirement、Provision、Binding 和 Project/OS 边界。
3. [`architecture_overview.md`](architecture_overview.md)：源码分区、聚合入口、静态/动态装配和专题路由。
4. 按问题进入对应目录 README：[`io/README.md`](io/README.md)、[`system/README.md`](system/README.md)、
   [`storage/README.md`](storage/README.md)、[`input/README.md`](input/README.md)、
   [`usb/README.md`](usb/README.md)、[`project/README.md`](project/README.md)。

## 读取规则

- `canonical` 定义全局身份与核心契约；`supporting` 只在自身专题内有效。
- `exploration` 和 `archive` 不作为当前实现依据。
- 源码、CMake、实际 consumer 和当次测试优先于文档中的进度或能力宣称。
- Host、QEMU、real board、demo 和 build-only 各自只证明对应证据范围。

涉及 Agent 操作规则时读 [`../AGENTS.md`](../AGENTS.md)；涉及文档维护规则时读
[`documentation_maintenance.md`](documentation_maintenance.md)。
