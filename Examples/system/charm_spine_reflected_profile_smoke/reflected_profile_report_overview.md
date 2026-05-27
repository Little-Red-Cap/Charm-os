# Reflected Profile Report Overview

这个文件收口 `charm_spine_reflected_profile_smoke` 当前已经稳定下来的 report 语义。

它描述的是 smoke 内部原型事实，不是公共 API、不是 `Modules/*` 契约、不是 generator 输出，也不是 board / monitor runtime 接口。

## 目标

这个 smoke 当前证明的是一条统一叙事：

```text
reflected spec
-> compile-time profile resolution
-> resolution diagnostic evidence
-> accepted profile init/context projection
-> selected provider evidence
-> unified report presentation
```

重点不在继续扩 reflection 表面，而在把两类证据收成同一种 report 结构：

- profile resolution diagnostic evidence
- accepted profile selected provider evidence

## Unified Report 语义

统一 report 固定分成两种形态：

- accepted profile report
  - 第一帧必须是 `profile.resolution` diagnostic evidence
  - 后续帧必须是 selected provider evidence
- blocked profile report
  - 只允许出现 `profile.resolution` diagnostic evidence
  - 不允许出现任何 provider evidence

这两种形态共用同一个 frame/collector/report 叙事，只是允许的内容不同。

## Section 顺序

report section 顺序固定为：

```text
diagnostics -> selected_providers
```

对应约束如下：

- diagnostic frame 必须带 `ReportSection::diagnostics`
- provider frame 必须带 `ReportSection::selected_providers`
- provider evidence 不能在 diagnostics 之前追加
- provider section 开始后，diagnostic evidence 不能再回插

这保证了后续 generator、monitor 或 board tooling 如果消费这个叙事，看到的是稳定顺序，而不是依赖实现细节推断。

## Accepted / Blocked 差异

`accepted profile` 的语义：

- compile-time resolution status 为 `ok`
- 允许 materialize `ResolvedProfileProjection`
- 允许 materialize runtime `ContextView`
- unified report 同时包含 diagnostic 与 selected provider evidence
- 未选中的 provider 不能被隐式带入 report

`blocked profile` 的语义：

- compile-time resolution status 非 `ok`
- 只允许投影为 diagnostic evidence
- 不允许进入 `ResolvedProfileProjection`
- 不允许进入 runtime `ContextView`
- report presentation 中不允许出现 provider evidence

## Compile-time 状态分类

当前 smoke 固定的分类如下：

- `ok`
- `duplicate_provider_tag`
- `duplicate_provider_token`
- `missing_binding`
- `duplicate_binding`
- `extra_binding`
- `invalid_binding`

这些状态既是 compile-time resolution taxonomy，也是 diagnostic evidence 中稳定输出的 `status` 字段。

## Runtime 门禁

当前 smoke 保持两个 runtime 门禁：

- 只有 `GoodProfile` 能驱动 `init.graph`
- 只有 `GoodProfile` 能 materialize `ContextView`

坏 profile 不走 fallback，不做半投影，不偷偷选择“看起来能用”的 provider。

## 仍然刻意不做的事

当前仍然不做这些事情：

- 不提升为公共 runtime report API
- 不迁入 `Modules/*`
- 不引入 manifest / DSL / codegen
- 不把 provider evidence 变成 init 控制面
- 不把 blocked profile 做成 runtime 自动降级

## 下一阶段可延伸点

如果后续要继续推进，这个 smoke 当前最自然的延伸点是：

- generator 消费统一 report 语义
- monitor / board tooling 复用 diagnostics-first 叙事
- 将 accepted / blocked report 口径映射到更真实的 profile 选择流程

但前提应保持不变：

- compile-time gate 先成立
- report 顺序先稳定
- blocked profile 不产生 provider evidence
