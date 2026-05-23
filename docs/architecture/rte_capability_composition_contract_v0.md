# Charm RTE Capability Composition Contract v0

本文件定义 Charm RTE v0 的边界。

这里的 RTE 不是 runtime framework。它不接管调度、不实现事件循环、
不充当 service locator、DI container，也不试图复制 AUTOSAR RTE。

Charm RTE v0 的职责只有一个：

```text
capability composition boundary
```

也就是把组件声明的能力需求、板级或系统服务提供的能力、profile 中的绑定关系、
以及后续 init / evidence / host / ABI 等投影统一收束成一套可解释的系统结构。

## 1. 非目标

RTE v0 明确不做以下事情：

- 不做 YAML / TOML / JSON manifest。
- 不做 DSL。
- 不做 graph compiler。
- 不做 preset generator。
- 不新增 C++ module 实现。
- 不修改 H747 Lab 底座。
- 不把 `init.graph` 扩展成运行期拓扑框架。
- 不把 runtime capability ref 变成 service locator。

v0 只验证语义。任何未来工具化形式都必须先证明这些语义能在普通 C++
中自然表达。

## 2. 核心不变量

### 2.1 RTE 只表达能力装配

RTE 的输入是：

- component 的 requirements。
- provider 的 provides。
- profile 的 binding。

RTE 的输出不是一个新的 runtime，而是多个投影：

- init projection：进入 `init.graph`。
- context projection：形成 app 可见的 `ContextView`。
- evidence projection：形成结构化证据面。
- host projection：形成 mock / host backend 装配。
- ABI projection：未来形成 hostcall / capability table。

### 2.2 Component topology 是源头

`ComponentDesc` 描述系统节点。

`init.graph`、scheduler 注册、reactor 绑定、evidence 收集、host mock
都只是 component topology 的不同投影。不要把 `init.graph` 反过来当作
系统结构的唯一源头。

### 2.3 World 是投影，不是容器

应用不应拿到全局能力容器。

应用只能拿到由 requirements 裁剪出的 `ContextView`。这个 view 只暴露
该 app 声明需要的能力，避免 `world` 膨胀成 god object。

### 2.4 Evidence 不是 log

Evidence 是结构化事实，不是字符串日志。

provider 可以产出 `EvidenceFrame`；presentation 层可以把它打印为文本。
实时路径不得在 evidence 采集阶段做格式化、分配或策略判断。

### 2.5 语义统一，载体分层

同一个 capability 语义可以有三种载体：

| 载体 | 作用 | 当前状态 |
|---|---|---|
| Concept capability | 编译期零成本语义边界 | v0 主载体 |
| Runtime capability ref | 运行期装配与 profile materialization | v0 约束语义 |
| ABI capability table | 未来 ELF / dynamic boundary | v0 只保留方向 |

三者表达同一语义，但不能互相替代。

## 3. 词汇表

### 3.1 `CapabilityKind`

能力种类。它回答“这是什么能力”。

示例：

- `cap::TextSink`
- `cap::Clock`
- `cap::LineSource`
- `cap::SolidFillDisplay`
- `cap::RasterDisplaySink`
- `cap::BlockDevice`
- `cap::AudioSink`

`CapabilityKind` 不回答“由谁提供”，也不回答“绑定到哪个用途”。

### 3.2 `CapabilityRole`

能力角色。它回答“这个能力在当前组件里承担什么用途”。

示例：

- `role::log`
- `role::shell_input`
- `role::monotonic_time`
- `role::primary_display`
- `role::firmware_storage`

同一种 `CapabilityKind` 可以有多个 role。例如同一 profile 中可以同时存在
`log`、`shell`、`debug_trace` 三个 `TextSink` 角色。

### 3.3 `ProviderDesc`

provider 声明它提供的能力。

provider 可以来自 board port、system service、domain backend 或 host mock。
它应把 provider 身份和可提供的能力 token 分开表达：

- `ProviderDesc`
  描述“由谁提供”。
- `Provided<CapabilityKind, CapabilityRole>`
  描述“这个 provider 可以满足哪个 role 的哪类能力”。

示例语义：

```cpp
provide<cap::TextSink, provider::uart1_console>();
provide<cap::TextSink, provider::usb_cdc_console>();
provide<cap::Clock, provider::systick_clock>();
```

provider 只是候选能力来源。它不会被 app 隐式查找。
`Provided` token 也不是 provider 实例；它只是 profile resolution 可检查的
类型级事实。

### 3.4 `RequirementDesc`

component 声明它需要的能力和 role。

示例语义：

```cpp
require<cap::TextSink, role::log>();
require<cap::Clock, role::monotonic_time>();
require<cap::SolidFillDisplay, role::primary_display>();
```

requirement 不指定 provider。provider 由 profile binding 决定。
resolution 时，requirement 必须匹配某个 provider 声明过的
`Provided<CapabilityKind, CapabilityRole>` token。

### 3.5 `ComponentDesc`

系统节点的静态语义描述。

`ComponentDesc` 不是运行期对象，也不是 service locator 注册项。

它至少表达：

- name
- phase
- provides
- requires
- lifecycle entry points
- optional evidence producer
- optional runtime participation tags

示例语义：

```cpp
constexpr ComponentDesc display_demo{
    .name = "display_demo",
    .requires = requirements(
        require<cap::TextSink, role::log>(),
        require<cap::Clock, role::monotonic_time>(),
        require<cap::SolidFillDisplay, role::primary_display>()),
    .provides = providers(
        provide<cap::App, role::main_app>()),
    .phase = init::Phase::app,
};
```

### 3.6 `ComponentInstance`

组件的运行期状态实体。

它可以保存 app/service/backend 的状态，但不能替代 `ComponentDesc`。
一个 desc 可以对应一个或多个 instance。一个 instance 不能自行扩大它的
requirements。

### 3.7 `ProfileBinding`

profile 对 requirements 和 providers 的绑定结论。

示例语义：

```text
role::log             <- provider::uart1_console
role::shell_input     <- provider::uart1_console_rx
role::monotonic_time  <- provider::systick_clock
role::primary_display <- provider::hx8394d_panel
```

binding 是显式结论。app 不应在运行时“查找一个可用 TextSink”。

### 3.8 `ContextView`

按组件 requirements 裁剪出来的能力访问面。

示例语义：

```cpp
using DisplayDemoContext = ContextView<
    Requirement<cap::TextSink, role::log>,
    Requirement<cap::Clock, role::monotonic_time>,
    Requirement<cap::SolidFillDisplay, role::primary_display>>;
```

`ContextView` 只暴露 requirements 声明过的能力。它不是全局 world。

### 3.9 `EvidenceFrame`

结构化证据帧。

示例语义：

```cpp
EvidenceFrame {
    capability = "display.primary",
    provider = "hx8394d.dsi_ltdc",
    status = ok,
    fields = {
        {"mode", "720x1280"},
        {"format", "rgb888"},
        {"lanes", "2"},
    },
};
```

v0 不规定具体字段编码，但规定 evidence 必须是结构化事实，而不是预格式化日志。

## 4. Capability 三层载体

### 4.1 Concept capability

Concept capability 用于源码级协作、host mock、MCU 静态绑定和零成本抽象。

它适合表达：

- app template requirements。
- provider 是否满足语义。
- host / MCU 共享 app 的编译期约束。

它不适合直接作为 ELF ABI。

### 4.2 Runtime capability ref

Runtime capability ref 用于 profile materialization 后的运行期访问。

它适合表达：

- 已绑定 provider 的轻量引用。
- registry / context view 的内部存储。
- 静态组件和运行期发现平面之间的桥。

它不得退化成“随时按名字查服务”的 service locator。

### 4.3 ABI capability table

ABI capability table 是未来 dynamic / ELF 边界的方向。

它适合表达：

- hostcall table。
- C ABI ops table。
- dynamic app 与 resident monitor 之间的稳定边界。

v0 不实现 ABI table，只要求 concept 和 runtime ref 的语义不要阻塞未来映射。

## 5. Component projection

### 5.1 Init projection

init projection 把 component topology 中的启动依赖 materialize 到 `init.graph`。

规则：

- init dependency 必须是 DAG。
- `init.graph` 只负责启动顺序和 init lifecycle。
- 运行期事件循环、scheduler、reactor 不得被强行塞进 init DAG。

示例：

```text
power -> display -> display_demo
```

这只是启动依赖，不代表 display 在运行期同步调用 app。

### 5.2 Runtime projection

runtime projection 描述组件进入 scheduler / reactor / event pump 的方式。

它可以引用 component topology，但不能和 init projection 混为一谈。

运行期拓扑通常是 temporal、reactive、cyclic、asynchronous 的，不要求是 DAG。

### 5.3 Context projection

context projection 根据 `ProfileBinding` 生成 app 可见的 `ContextView`。

规则：

- app 只能访问它声明过的 requirements。
- ContextView 不暴露 provider registry。
- ContextView 不提供 fallback lookup。

### 5.4 Evidence projection

evidence projection 收集 provider / component 产出的结构化状态。

规则：

- evidence producer 不能隐式改变硬件状态。
- evidence snapshot 应可重复读取。
- presentation 层负责格式化。

## 6. Profile resolution rules

Profile resolution 至少要满足以下规则：

1. 每个 `RequirementDesc` 必须有显式 binding。
2. 多个 provider 满足同一 `CapabilityKind` 时，必须通过 role / binding 消歧。
3. binding 只能指向提供了匹配 `Provided<CapabilityKind, CapabilityRole>` token 的 provider。
4. app 不能直接依赖 provider 名称。
5. provider 不能通过隐藏全局对象绕过 binding。
6. unresolved requirement 是配置错误，不是运行期 fallback 场景。

示例：

```text
providers:
  uart1_console: TextSink
  usb_cdc_console: TextSink

requirements:
  display_demo.log: TextSink as log

binding:
  display_demo.log <- uart1_console
```

## 7. 与现有 Charm 文档的关系

- `docs/system/init_graph_contract.md`
  继续定义 init graph 的现行契约。本文只说明 init graph 是 component topology
  的 init projection。
- `docs/architecture/driver_model.md`
  继续定义设备/驱动与 runtime discovery plane。本文只定义静态能力装配边界。
- `docs/architecture/capability_recovery_rules.md`
  继续定义能力回收流程。本文为后续回收提供 RTE 词汇。
- `docs/system/bringup_evidence_pipeline_v0.md`
  继续定义 bringup evidence 流程。本文只定义 RTE 层 evidence projection 的语义位置。
- `docs/architecture/system_compiler_vocabulary_v0.md`
  继续定义 system compiler 词汇。本文的词汇可作为后续 system compiler 输入对象。

## 8. 验证场景

### 8.1 Multiple TextSink providers

同一 profile 中存在 `uart1_console` 与 `usb_cdc_console`。

期望：

- app 声明 `TextSink as log`。
- profile 显式绑定 `log <- uart1_console`。
- app 不能隐式选择任意 `TextSink`。

### 8.2 Host / MCU shared app

同一 app requirements 在 host mock 与 H747 provider 下生成不同 `ContextView`。

期望：

- app 代码不依赖 HAL。
- app 代码不依赖 BSP global。
- app 代码不关心 provider 是 host 还是 MCU。

### 8.3 Init projection

`power -> display -> app` 进入 init projection。

期望：

- init graph 只表达启动顺序。
- runtime event / scheduler / reactor 另行投影。

### 8.4 Evidence side channel

display provider 初始化后产出结构化 `EvidenceFrame`。

期望：

- provider 不直接格式化日志。
- presentation 层可以把 evidence 打印为文本。
- evidence snapshot 可被 host / CI 对比。

### 8.5 No DSL dependency

所有 v0 语义必须能用普通 C++ constexpr 描述表达。

期望：

- 没有 manifest parser。
- 没有 code generator。
- 没有 graph DSL。

## 9. v0 采用顺序建议

1. 先在文档和 review 语言中使用本文词汇。
2. 再挑一个 host / MCU 共享 app 作为 `ComponentDesc + ContextView` 语义样本。
3. 再把一个 board service 的 snapshot 接入 `EvidenceFrame` 语义。
4. 最后再评估是否需要 manifest 或 generator。

任何工具化动作都必须晚于语义验证。
