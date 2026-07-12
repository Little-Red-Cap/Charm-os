# Charm 方法论摘要

> **文档状态：`exploration`（冻结扩面）**

本文只保留已被核心治理吸收的方法论结论。完整讨论见
[`../archive/architecture-exploration-v0/charm_methodology_charter.md`](../archive/architecture-exploration-v0/charm_methodology_charter.md)。
核心身份和准入规则以 [`../../CONSTITUTION.md`](../../CONSTITUTION.md) 为准。

## 可复用结论

- 平台实现不应决定应用与系统的核心语义；平台差异应停留在实现、装配或项目事实边界。
- 依赖、生命周期、资源归属和失败行为应有可检查的边界，不能只依赖约定或人脑记忆。
- 抽象必须保留嵌入式约束，不能用额外框架复杂度换取表面统一。
- 机器可检查的结构和可重复证据比叙事性“架构完整”更有价值。

## 不属于当前裁决

`术/法/道`、`主体性`、`系统主脊梁`等表达是讨论用语，不是 Core 类型或准入等级。
`init.graph`、Profile、Binding、Generator 等具体机制也不能仅凭本摘要获得 Core 身份；
必须分别通过 Constitution 的六问和独立实现证据。

## 使用边界

遇到新的架构方案，先阅读 Constitution 和
[`charm_core_contract.md`](charm_core_contract.md)，再把本摘要当作设计检查表。本文不定义 API、目录、路线图或平台选择。
