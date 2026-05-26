# H747 Lab Spine Migration Boundary

本文定义一个很具体的问题：

```text
Charm Spine / RTE 这条 host prototype 线，哪些语义应该迁到 H747，
哪些只能停留在 host proof，哪些绝不能直接带进板级 ABI 或 monitor？
```

它的目标不是再开一条新原型线，而是给后续 `H747 POSIX ELF`、`app_lab`、
`player_md3` 和未来 board tooling 一个稳定边界。

## 1. 要迁到 H747 的，是语义，不是 smoke 实现

当前 `Examples/system/*` smoke 证明的核心语义，可以迁到 H747：

- `Capability -> Component -> Profile -> Projection -> Evidence`
- profile 必须先解析成功，后续 projection 才能成立
- app 只能拿到按 requirements 裁剪后的 `ContextView`
- evidence 是结构化 side-channel，不是 init 控制面，不是 provider log
- explain / report surface 是已解析 profile 的只读投影，不是 runtime 控制面
- host、board、ABI 是不同载体，不能混成一种边界

这些结论应该继续约束 H747。

但 smoke 内部的具体实现形态不直接迁移：

- `main.cpp` 里的 prototype 类型
- host-only 的 verifier / presentation helper
- reflected smoke 的局部命名和帧布局
- `<meta>` 反射参与 proof 的写法

H747 继承的是语义，不是把 smoke 代码挪过去。

`h747_lab_host_profile_compare` 当前承接的是 RTE explain projection 语义：
host/H747 profile evidence 可以被解释成只读 report，用来展示同一 app capability
语义下的 provider identity 与 display/input fact 差异。它不是复制
`Examples/system/rte_explain_projection_smoke` 里的局部类型，也不是公共 RTE API。

## 2. H747 上的三种承载面

### 2.1 Source-level app/world 承载面

这条线对应 `display_raster_demo`、`player`、`player_md3`。

这里真正承接 `ContextView` 语义的，不一定叫 `ContextView`，但必须保持同一原则：

- app 只依赖裁剪后的 capability/world
- profile 决定 provider 绑定
- provider identity 不泄漏进 app 逻辑
- board/HAL/DSI/LTDC/I2C/TIM 事实留在 service 或 board adapter

当前 `RasterDisplayWorld`、`InputWorld`、`RasterDisplayInputWorld`
就是这条 source-level `ContextView` 语义的板级承载面。

### 2.2 Resident monitor / ELF / App ABI 承载面

这条线对应 `app_lab`、`posix_lab` 与后续 resident loader。

这里不能直接把 C++ concept、template、reflection token 当成跨 ELF 边界。
真正可迁的是“能力语义”，承载方式必须换成显式 ABI：

- capability table
- hostcall table
- 明确的 app entry / process entry
- monitor 可解释的 launch / exit / evidence surface

也就是说：

- source-level `ContextView` 语义可以指导 ABI 设计
- 但 ABI 侧必须是显式、稳定、可独立装载的表面
- 不允许把 template identity 或 name mangling 当协议

### 2.3 Monitor / board tooling 承载面

`charm_spine_reflected_profile_smoke` 当前已经证明：

- accepted path 可以形成 `diagnostic + selected provider evidence`
- blocked path 只能形成 diagnostic
- report 顺序应固定为 `diagnostics -> selected facts`

这个叙事可以迁到 monitor / board tooling，但只能作为 explain/report surface：

- 用来解释为什么某个 profile / app / ELF 被接受或阻塞
- 用来解释当前选中了哪些 provider / backend / board facts

它不应直接变成：

- init 排序源头
- runtime 控制面
- app 可见 world
- provider registry

## 3. 允许迁移的稳定结论

以下结论应视为 H747 线可以复用的稳定事实：

- resolved profile 是 projection 的前置门禁
- blocked profile 不允许 fallback provider
- `ContextView` / world 必须是裁剪世界，不是 global world
- evidence collector 不进入 app world
- provider evidence 是只读 side-channel
- explain/report 只能解释装配与事实差异，不进入 app world 或 runtime provider
- board target 可以有自己的 presentation，但不能篡改上面这些语义

## 4. 不应直接迁移的东西

以下内容目前应明确留在 host proof 或 prototype 层：

- reflected smoke 里的局部 `ReportBuilder` / `DiagnosticSetBuilder`
- smoke-local `EvidenceFrame` 容量和 presentation buffer 细节
- 用 `<meta>` 自动发现字段形状的具体写法
- “因为 host proof 方便”而引入的泛化 helper
- 尚未同时经过 host 与 H747 验证的拟公共 API

这些内容可以启发后续实现，但不能直接上升为 H747 契约。

## 5. 对 H747 当前两条主线的直接约束

### 5.1 对 `player` / `player_md3`

- 继续把 world 当 source-level `ContextView` 形态
- app 逻辑不接触 HAL/global singleton
- evidence 继续留在 app/world 之外
- host/mock 与 H747 差异继续收在 profile / provider / evidence 层

### 5.2 对 `app_lab` / `posix_lab`

- ABI 边界必须显式化，不能偷渡 C++ 模板语义
- capability table / hostcall table 只镜像稳定能力语义
- monitor 可以借用 diagnostics-first 叙事解释 load/run 结果
- 不能把 host proof 的局部 report 类型直接当成 resident ABI

## 6. 当前默认判断规则

如果后续遇到一个新结构，不知道它该落在哪一层，默认这样判断：

- 它描述 app 只应看到的能力世界：落 source-level world / `ContextView` 语义
- 它描述板级 provider/HAL/装载细节：落 service / board / runtime adapter
- 它描述跨 ELF 的稳定调用面：落 capability table / hostcall ABI
- 它描述“为什么这次装配/启动成立”：落 evidence/report/tooling

一旦一个结构同时想承担这四种角色，默认就是分层出了问题。

## 7. 与相关文档的关系

- `docs/architecture/charm_spine_v0.md`
  定义平台主语
- `docs/architecture/rte_capability_composition_contract_v0.md`
  定义 capability composition boundary
- `docs/architecture/rte_to_h747_platform_roadmap.md`
  定义 `RTE -> H747` 主线
- `docs/h747_lab_capability_contract.md`
  定义 H747 当前 source-level capability/world 边界
- `docs/h747_lab_layering_contract.md`
  定义 H747 项目分层与板级承载面

本文只回答“host proof 如何服务 H747 落地”，不替代上面任何一个契约。
