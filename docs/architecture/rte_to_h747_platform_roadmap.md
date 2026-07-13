# RTE 到 H747 历史路线摘要

> status: `exploration`
>
> `RTE -> H747` 与 `Display + Player` 是已停线的压力路线，不是当前 Charm roadmap、
> Core 定义或 H747 验收要求。完整五阶段原文已删除；退出原因见
> [`architecture-exploration-v0`](../archive/architecture-exploration-v0/README.md)。

## 历史假设

该路线曾尝试用 H747 Display + Player 验证：

```text
Capability -> Component -> Profile -> Projection -> Evidence
```

其中 RTE 被设想为 capability composition boundary，不接管调度、事件循环或 service
locator；H747 只作为真实板压力场，不能定义跨平台语义。

## 仍可复用的局部约束

- App/domain 依赖 capability 语义，不依赖 HAL handle 或 BSP global。
- Provider identity 只进入 binding 和诊断，不泄漏到 App 业务接口。
- 板级事实、source-level context、resident ABI 和 report/tooling 是不同边界。
- Host proof 不能替代真实板证据；真实板 workaround 也不能自动进入公共契约。
- ABI capability table 只能镜像已稳定语义，不能直接导出 C++ template/world 类型。
- Explain/report 是派生读取面，不得反向成为系统源事实。

这些约束只在具体实现中作为审查依据，不恢复 RTE、Spine、Profile 或 Evidence 为
Charm Core 名词。Core 准入仍由 [`../../CONSTITUTION.md`](../../CONSTITUTION.md) 和
[`charm_core_contract.md`](charm_core_contract.md) 裁决。

## 不再成立的路线声明

- `RTE -> H747` 不是当前主线。
- `Display + Player` 不是 Charm 平台成立的必选垂直切片。
- 旧 H747 target 名、构建目录和 CMake 命令不是现行验证入口。
- 五阶段排期、公共化候选和 reflection/system-compiler 顺序没有当前承诺。
- host RTE/Spine smoke 的存在只证明各自 fixture，不证明统一平台模型已获准。

H747 当前事实必须从 `Examples/project/h747-lab` 的源码、CMake 和局部契约重新核对；
Player 也不能反向定义 Charm 平台边界。

## 保留证据

`Examples/system` 仍保留 RTE/Profile/Spine 相关 host smoke。它们可用于追溯旧模型、
比较局部语义和防止历史 fixture 意外损坏，但不构成默认回归基线或 Core admission。

需要重新启用其中任何概念时，必须重新给出真实消费者、跨环境证据、失败语义和比
更小机制更必要的理由，不能从历史文档或 smoke 数量反推资格。
