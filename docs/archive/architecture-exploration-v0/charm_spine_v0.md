# Charm Spine v0

> [!IMPORTANT]
> **文档状态：`exploration`（已由核心契约取代身份入口）**
> 本文保留 v0 推演，但 `Capability -> Component -> Profile -> Projection -> Evidence`
> 不是已获准的 Core 主链。当前定位与裁决见 [`../../../CONSTITUTION.md`](../../../CONSTITUTION.md)
> 和 [`../../architecture/charm_core_contract.md`](../../architecture/charm_core_contract.md)。

本文定义 Charm 的第一根主脊梁。

它不是目录重排计划，也不是新 DSL、manifest、generator 或 runtime framework。
它的目标是回答一个更上位的问题：

```text
Charm 到底是什么？
```

v0 结论：

```text
Charm is a capability-oriented embedded application platform.
```

更具体地说：

```text
Capability -> Component -> Profile -> Projection -> Evidence
```

这条链路是 Charm 的平台主语。现有 `Modules/`、`Examples/`、H747 Lab、
init.graph、POSIX、UI、driver model 和未来 ELF/ABI 边界，都必须能被解释为
这条链路上的一部分或一个投影。

## 1. 非目标

Charm Spine v0 明确不做以下事情：

- 不重排全仓库目录。
- 不把 `Modules/` 立即改名或搬迁。
- 不引入 YAML / TOML / JSON manifest。
- 不引入 DSL 或 graph compiler。
- 不把 RTE 扩展成 runtime framework。
- 不把 `init.graph` 升级成系统结构源头。
- 不修改 H747 Lab 底座。
- 不把 host mock、board backend、ELF ABI 混成同一种边界。

v0 先确立主脊梁，再让目录和模块逐步臣服于它。

## 2. 核心链路

### 2.1 Capability

Capability 是能力语义。

它回答：

- 这是什么能力？
- 它以什么 role 被使用？
- provider 是否满足这个能力？
- app 是否只依赖自己声明的能力？

Capability 不回答：

- 系统如何启动？
- 事件如何调度？
- provider 实例在哪里全局查找？
- ELF ABI 如何编码？

这些问题属于后续 projection。

### 2.2 Component

Component 是系统节点描述。

它回答：

- 这个节点提供什么能力？
- 这个节点需要什么能力？
- 这个节点属于哪个 phase？
- 这个节点有哪些 lifecycle entry？
- 这个节点能产生哪些 evidence？

`ComponentDesc` 是系统结构源头。`init.graph`、ContextView、host mock、
evidence report、future ABI table 都只是它的不同投影。

### 2.3 Profile

Profile 是装配结论。

它回答：

- 当前构建选择哪些 component？
- 每个 requirement 绑定到哪个 provider？
- host、mock MCU、真实 board 使用哪组 provider？
- 哪些能力进入 init projection？
- 哪些能力进入 app context projection？

Profile 不是 CMake preset 的同义词。CMake preset 可以选择 profile，
但 profile 的语义是能力装配，不是构建参数集合。

### 2.4 Projection

Projection 是从同一份 component/profile 结构派生出来的视图。

v0 至少承认以下投影：

- init projection：投影到 `init::Node` / `init::Graph`。
- context projection：投影到 app 可见的 `ContextView`。
- evidence projection：投影到结构化事实帧；它是只读 side-channel，
  不参与 init ordering、runtime scheduling 或 provider log 输出。
- explain projection：投影到只读 report / explain surface，用于解释 profile
  binding、provider identity 与 fact 差异；它不读取 runtime provider 实例，
  也不是 artifact schema 或 system compiler 平台本体。
- host projection：投影到 PC/mock backend。
- board projection：投影到真实板级 provider。
- ABI projection：未来投影到 hostcall / capability table。

Projection 不拥有系统结构。它只 materialize 系统结构的一个用途。

### 2.5 Evidence

Evidence 是系统自证事实。

它回答：

- 这个 provider 最终以什么参数工作？
- 这个 board fact 是否被验证？
- 这个 projection 是否得到可重复观测？
- host 和 board 的行为是否可以比较？

Evidence 不是 log。log 是 presentation。Evidence 是结构化事实。

## 3. 新世界里的旧对象

### 3.1 `Modules/`

`Modules/` 当前仍是 Charm 的代码库主承载区。

在 Spine 视角下，它不再只是“模块集合”，而是候选能力、投影工具、
runtime substrate 和旧系统资产的混合区。v0 不移动它，但要求后续新增内容
尽量能说明自己属于以下哪一类：

- capability vocabulary
- component substrate
- projection substrate
- runtime substrate
- provider/backend
- legacy asset

### 3.2 `init.graph`

`init.graph` 是 init projection substrate。

它继续负责：

- 固定容量 DAG。
- capability id 唯一性。
- phase ordering。
- topo sort。
- init callback 顺序执行。

它不负责：

- runtime event topology。
- scheduler。
- ContextView。
- provider discovery。
- profile binding 决策。

### 3.3 H747 Lab

H747 Lab 是现实压力测试场。

它的职责不是定义 Charm 的全部形态，而是用真实硬件持续拷打 Charm Spine：

- board facts 是否能收敛成 provider？
- app 是否能脱离 HAL/global singleton？
- display/input/storage/audio 是否能被切成能力？
- evidence 是否能解释真实硬件状态？
- host projection 是否能减少烧录争抢？

H747 Lab 可以暴露 Charm Spine 的缺口，但不应把板级临时路径直接上升为
Charm 公共契约。

### 3.4 Examples/system

`Examples/system` 是系统语义试验场。

这里的 smoke 不是示例玩具，而是 Spine 的最小证据链：

- `rte_component_context_smoke`
- `rte_init_projection_smoke`
- `rte_profile_materialization_smoke`
- `rte_explain_projection_smoke`
- `charm_spine_smoke`
- `charm_spine_evidence_projection_smoke`
- `charm_spine_reflected_profile_smoke`

这些 smoke 可以先验证语义，再决定是否提升为公共模块。

### 3.5 POSIX / ELF / modulex

POSIX、ELF、modulex 是未来 dynamic boundary 的压力线。

它们不应该绕过 Capability / Component / Profile 语义。未来 hot-load 或
ELF app 边界应接收 capability table / hostcall table，而不是直接依赖 C++
concept、template 或 name mangling。

## 4. Spine v0 不变量

1. Capability 是语义，不是对象查找。
2. Component 是系统节点，不是 runtime service。
3. Profile 是装配结论，不是 preset。
4. Projection 是派生视图，不是系统源头。
5. ContextView 是 app 的裁剪世界，不是全局容器。
6. Evidence 是结构化事实，不是 log。
7. Host、board、ABI 是不同载体，不得混成一种边界。
8. 任何 DSL / generator 必须晚于普通 C++ 语义验证。

## 5. v0 采用顺序

1. 用本文作为 Charm 主语入口。
2. 用 RTE v0 契约定义 capability composition boundary。
3. 用 system smoke 验证 component/profile/projection/evidence 语义。
4. 用 H747 Lab 验证真实 board provider 和 host projection。
5. 再决定哪些 prototype 可以提升到 `Modules/`。
6. 最后再评估 manifest、generator、static reflection 或 ABI table。

任何“翻天覆地”的重构都应优先改变系统主语，而不是先改变目录外观。
