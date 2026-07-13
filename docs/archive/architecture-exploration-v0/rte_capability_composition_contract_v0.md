# RTE Capability Composition v0 保留笔记

> `status`: `exploration`（停线冻结）

`RTE` 已被裁决为 `Rejected / Deferred` Core 词汇。本文只保留 Requirement、Provision、Binding
resolution 与投影一致性的实验取舍，受 [`CONSTITUTION.md`](../../../CONSTITUTION.md) 和
[`charm_core_contract.md`](../../architecture/charm_core_contract.md) 约束。

## 关系与解析

- Requirement：消费方声明依赖某项 Capability Contract。
- Provision：提供方声明可满足该契约。
- Binding：一次项目/profile 组合中把 requirement 关联到 provision。

Provider 是 Provision 中的角色，不需要公共基类、manager 或 registry。Profile 只选择项目组合，
不定义 Capability Contract。局部 role 可以消歧，但每个 role 仍需独立 requirement/provision/binding。

最小 resolution 必须保证每个 required relation 恰有一个显式 binding，并拒绝 duplicate、missing、
wrong-role、stale 和 extra binding。Unresolved requirement 是组合错误，不触发隐藏 fallback；consumer
源码也不依赖具体 provider 名称。这些关系可由普通 C++ 数据和 smoke 验证，不要求 RTE module 或 DSL。

## Snapshot 与投影

解析结果可以形成局部 snapshot，供 init、context、evidence 或 ABI 投影消费。所有投影必须读取同一份
结果，不能各自重新选择 provider：

- init projection 只派生启动依赖，不等于 runtime topology；
- context slice 只暴露 consumer 声明的 requirements，不提供全局 capability bag；
- evidence projection 只报告 binding identity、来源和失败，不证明 provider 或 hardware 可用；
- ABI projection 只投影已稳定调用表，不复制 binding 语义。

Snapshot 不拥有 provider lifetime，也不负责 fallback、discovery、hotplug 或 service lookup。不同投影
可以有不同布局与生命周期，不能因 capability 名称相同就互换。

## 明确拒绝

- 全局 `World`、capability bag、RTE manager、公共 Component 基类或按名 service locator；
- 把 init graph、runtime topology、evidence 和 ABI 合成统一 component graph；
- 让 profile/provider 名称进入 App 业务接口；
- 由 reflection、schema 或历史 smoke 数量推断统一 compiler/runtime 已成立。

## 重新推进条件

重新推进前必须找到一个真实 consumer，以及现有 binding helper 无法表达的最小问题。只有在至少两个
execution environment 中证明相同关系、失败语义和替换实现后，才评估局部 Stable Boundary；这也不
自动恢复 `RTE` 名称。
