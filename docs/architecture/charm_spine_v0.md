# Charm Spine v0 摘要

> **文档状态：`exploration`（冻结）**

历史推演与退出理由见
[`architecture-exploration-v0`](../archive/architecture-exploration-v0/README.md)。
当前 Core 裁决见 [`../../CONSTITUTION.md`](../../CONSTITUTION.md) 和
[`charm_core_contract.md`](charm_core_contract.md)。

## 保留的判断

Spine v0 以这条链组织讨论：

```text
Capability -> Component -> Profile -> Projection -> Evidence
```

它可以作为对装配、平台投影和证据关系的探索性视图，但不是已经批准的 Core 主链。
现有 `Modules/`、`Examples/`、H747 Lab、POSIX、UI 和 ELF 也不能因能映射到该链条而获得统一身份。

## 当前可核对的边界

- `init.recipe/plan/materialize` 是实际存在的初始化装配实现；
- `Capability`、`Requirement`、`Provision`、`Binding` 的当前裁决在 Constitution 与核心契约中；
- Spine 没有独立 C++ API、manifest、DSL 或 graph compiler 实现证据。

## 使用规则

新代码不得以“Spine”作为公共模块、基类或全局架构对象。若要恢复这条路线，必须先给出两个独立环境中的消费方、失败行为和最小语义证明。
