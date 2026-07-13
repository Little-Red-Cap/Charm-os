# Spine / H747 压力路线保留笔记

> `status`: `exploration`（停线冻结）

历史路线曾用以下链条讨论 capability composition，并尝试以 H747 Display + Player 作为压力验证：

```text
Capability -> Component -> Profile -> Projection -> Evidence
```

RTE/Spine 被设想为 composition boundary，不接管调度、事件循环或 service locator；H747 只提供
真实板证据，不能定义跨平台语义。仓库没有独立 Spine API、manifest、DSL 或 graph compiler 证据。

## 保留边界

- App/domain 依赖 capability 语义，不依赖 HAL handle、BSP global 或 provider 名称。
- Provider identity 只进入 binding 和诊断；init、runtime、ABI 与 evidence 投影不得各自重选 provider。
- 板级事实、source context、resident ABI 和 report/tooling 是不同边界。
- Host proof 不能替代真实板证据，板级 workaround 也不能自动进入公共契约。
- ABI capability table 只能镜像稳定语义，不能直接导出 C++ world/template 类型。
- Explain/report 是派生读取面，不得成为系统源事实。

## 当前裁决

RTE、Spine、Profile 和 Evidence 不因这条链进入 Charm Core；Display + Player 也不是平台成立的
必选垂直切片。旧 target、构建命令、阶段排期和 host smoke 只属于历史 fixture，不能证明统一
平台模型已经获准。

重新启用这些概念前，必须给出真实 consumer、跨环境证据、失败语义，以及比现有
Requirement/Provision/Binding 或更小机制更必要的理由。当前裁决以
[`CONSTITUTION.md`](../../../CONSTITUTION.md) 和
[`charm_core_contract.md`](../../architecture/charm_core_contract.md) 为准。
