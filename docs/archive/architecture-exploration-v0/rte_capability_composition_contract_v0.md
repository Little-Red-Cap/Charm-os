# RTE Capability Composition v0 保留笔记

> status: `exploration`（停线冻结）
>
> scope: Requirement/Provision/Binding resolution 与投影一致性实验

`RTE` 当前裁决为 `Rejected / Deferred` Core 词汇。最高边界见
[`CONSTITUTION.md`](../../../CONSTITUTION.md) 和
[`charm_core_contract.md`](../../architecture/charm_core_contract.md)。本文不定义 runtime framework、
component topology、service locator、DI container、manifest、generator 或统一系统模型。

## 可保留的关系

实验中真正可复用的是三类已获准关系：

- Requirement：消费方声明依赖某项 Capability Contract；
- Provision：提供方声明可满足该契约；
- Binding：一次项目/profile 组合中把 requirement 关联到 provision。

Provider 只是 Provision 关系中的角色，不需要公共基类、manager 或 registry。Profile 是项目组合选择，
不定义 Capability Contract。

## Kind 与 Role

同一类能力可能在一个项目里承担不同用途，例如 log 与 debug trace 都消费文本输出。局部 role 可以帮助
binding 消歧，但 role 不是自动获准的全仓 vocabulary。

每个 role 必须有独立 requirement/provision/binding。提供某种 capability kind 不表示 provider 自动
满足所有 role，也不能让消费方按 kind 在运行时任选 provider。

## Resolution

一个最小 resolved binding 过程应满足：

1. 每个 required relation 有且只有一个显式 binding；
2. binding 指向声明了匹配 provision 的提供方；
3. duplicate、missing、wrong-role、stale 与 extra binding 被明确拒绝；
4. 同一 provider 绑定多个 role 时，每个 role 分别声明；
5. unresolved requirement 是组合错误，不触发隐藏 fallback lookup；
6. consumer source 不依赖具体 provider 名称。

这些规则可由普通 C++ 数据和 host smoke 验证，不需要先建立 DSL 或 RTE module。

## Resolved Binding Snapshot

Resolution 的输出可以形成局部 `ResolvedBinding`/snapshot，供不同工具或 runtime boundary 消费。它是
一次组合结果，不是新的 Core 原语，也不拥有 provider instance lifetime。

所有投影必须消费同一份已解析结果，不能各自重新选择 provider：

- init projection 只派生启动依赖，不等于 runtime topology；
- context projection 只暴露当前 consumer 声明的 requirements；
- evidence/explain projection 只报告 binding identity 与来源；
- ABI projection 若存在，只投影稳定调用表，不复制 binding 语义。

投影可以有不同数据布局和生命周期，不能因 capability 名称相同就互相替代。

## Context Slice

历史 `ContextView` 实验的有效边界是最小权限切片：consumer 只能访问自己声明的 requirement，不能看到
整个 profile、provider registry 或其它 component 的能力。

Context slice 不负责 fallback、discovery、ownership 或 hotplug。动态 App ABI 的 capability table 与
C++ context view 可以表达相似行为，但需要各自的 ABI/lifetime 证据。

## Evidence Side Channel

Evidence 应是结构化、带来源的只读事实，不是预格式化 log。采集不应隐式改变 provider 状态，也不应
为了观察把 collector 注入每个 consumer context。

Evidence projection 成功只证明 snapshot 可读取，不证明 provider 正确、hardware live 或 capability
contract 已满足。Host、QEMU 与真实板证据域保持分离。

## 明确拒绝

- 不建立全局 `World` 或 capability bag；
- 不把 init graph、runtime topology、evidence 和 ABI 合成一张 component graph；
- 不新增 RTE manager、公共 Component 基类或按名 service locator；
- 不从 reflection/schema 数量推断统一 compiler 已成立；
- 不让 profile/provider 名称进入 App 业务接口；
- 不因历史 smoke 存在就恢复 RTE roadmap。

## 重新推进条件

先选择一个真实 consumer 和一个无法由现有 binding helper 表达的最小问题。只有在至少两个 execution
environment 中证明相同关系、失败语义和替换实现后，才评估局部 Stable Boundary；仍不自动恢复
`RTE` 名称。
