# Charm Core 宪法

## 1. 本文件负责什么

本文件只回答一个问题：

> 什么概念有资格进入 Charm Core？

它是 Charm 核心身份、核心词汇与核心公共语义的最高裁决依据。任何 README、架构文档、
路线图、实现、工具或既有代码与本文件冲突时，都必须让位于本文件。

本文件不负责：

- 规划产品路线；
- 设计具体 API；
- 规定目录布局；
- 选择操作系统、MCU、工具链或部署格式；
- 为当前实现补写合理性。

Charm 当前定位为一个能力导向的嵌入式应用平台。这个定位的最小语义由
[`docs/architecture/charm_core_contract.md`](docs/architecture/charm_core_contract.md) 定义；
它不能反向放宽本文件的准入规则。

## 2. Core 准入六问

一个概念进入 Core 前，提案者必须逐项回答以下问题。

1. **实现替换**：替换所有具体实现后，这个概念是否仍然成立？
2. **消费方必要性**：消费方是否必须依赖这项语义，而不是实现方希望暴露它？
3. **独立可证明性**：这项主张是否能脱离单一实现，被重复验证或证伪？
4. **平台无关性**：删除 MCU、OS、RTOS、vendor 和具体产品名称后，它是否仍有完整含义？
5. **低例外预算**：它是否无需平台例外、optional method、flag soup 或不断增加的特殊情况才能成立？
6. **浅概念依赖**：理解它所需的前置概念是否足够少，并且它没有冒充比自己更基础的原语？

不能给出清晰证据时，默认结论不是“先放进来再说”，而是 `Rejected / Deferred`。

## 3. 裁决等级

每个拟进入公共语言的概念必须被裁决为下列一种，不允许用“暂时都算 Core”绕过分类。

| 裁决 | 含义 |
|---|---|
| `Core Primitive` | 应用与组合模型不可再约简的最小语义。数量必须极少。 |
| `Core Derived` | 仅由已获准原语推导出的关系或结果，不得冒充新原语。 |
| `Stable Boundary` | 需要稳定互操作，但不定义 Charm 身份的边界。 |
| `Implementation / Tool` | 实现、投影、执行机制、工程组织或开发工具。可以重要，但不属于 Core。 |
| `Project Fact` | 板卡、产品、BSP、工作区和部署选择等组合输入。 |
| `Rejected / Deferred` | 语义不清、证据不足、引力过大或当前没有必要进入公共语言。 |

`Stable Boundary` 不等于 `Core`，公共可见也不等于核心原语。一个概念可以拥有稳定接口，
同时仍然只是实现边界。

## 4. 举证与裁决

### 4.1 举证责任

举证责任属于提案者。提案至少必须给出：

- 明确的消费方，以及消费方为什么不能依赖更小的语义；
- 不包含具体平台名称的定义；
- 至少两个独立实现，且在至少两个运行环境中保持一致含义；
- 可重复的正例、反例和失败行为；
- 所依赖的既有核心概念；
- 预计会产生的附属概念、管理器、注册表、反射面或例外。

代码存在、文档很多、已经被广泛调用、某个项目急需，均不能替代上述证据。

### 4.2 准入流程

1. 先把新名词放在 exploration 文档或局部实现中。
2. 使用六问审查其必要性和概念依赖。
3. 给出唯一裁决等级和适用范围。
4. 只有获准后，才能进入 canonical 文档或新增公共 Core API。
5. 实现继续积累反例；裁决可以被收窄、降级或撤销。

同义词不能作为新概念重复申请。一个实现细节也不能通过改名为 `Manager`、`Context`、
`Graph`、`Runtime` 或 `Service` 获得 Core 身份。

### 4.3 没有祖父条款

现有名称和代码不享有祖父条款。`Capability`、`Component`、`Profile`、`Binding`、
`RTE`、`Compiler`、`IR`、`Graph`、`Runtime`、`Evidence` 等都必须重新举证。

一项准入在出现以下情况时应重新审判：

- 平台例外持续增加；
- 消费方开始依赖实现身份；
- 名词需要越来越多附属名词才能解释；
- 两个运行环境无法维持同一含义；
- 更小的关系已经足以表达原语。

## 5. 首批裁决

下表是停线收敛阶段的首批裁决，不是对现有代码结构的追认。每项裁决仍可在新证据下收窄或撤销。

| 概念 | 裁决 | 允许的含义与限制 |
|---|---|---|
| Capability Contract | `Core Primitive` | 消费方所依赖的行为契约；具体接口只是它的投影。 |
| Requirement | `Core Primitive` | 消费方声明“需要某项契约”的关系。 |
| Provision | `Core Primitive` | 某实现声明“可满足某项契约”的关系。 |
| Binding | `Core Derived` | 在一次组合中，把 Requirement 关联到 Provision 的关系；不是容器或运行时对象。 |
| ResolvedBinding / BindingSnapshot | `Stable Boundary` | 某次解析的结果物，不是新的核心原语。 |
| Provider | `Core Derived` | 仅表示 Provision 关系中的提供方角色；公共 Provider 基类、管理器或注册表不获准。 |
| Component | `Stable Boundary` | MVP 中可审查的静态装配单位；不得成为所有对象的共同基类。 |
| Profile | `Stable Boundary` | 一组项目级组合选择；不定义 Capability Contract。 |
| Execution Environment | `Stable Boundary` | Host、QEMU、真实板等承载差异的边界，不进入应用语义。 |
| Evidence | `Stable Boundary` | 可比较、可重复验证的观察结果；不等同于日志，也不是 Core 原语。 |
| Interface | `Implementation / Tool` | C++ interface、C ABI、RPC、共享内存等契约投影，不等同于契约本身。 |
| Backend | `Implementation / Tool` | 运行环境适配与实现组织，不是 Core 身份。 |
| Driver | `Implementation / Tool` | 工程实现与硬件控制组织，不是世界模型。 |
| Compiler | `Implementation / Tool` | 可生成或验证组合结果，但不拥有领域语义。 |
| IR | `Implementation / Tool` | 工具内部表示，不得反向定义应用契约。 |
| Graph | `Implementation / Tool` | 关系的派生表示；init DAG、runtime topology、ownership 和 hot-plug state 不得合并成一张宇宙图。 |
| Loader | `Implementation / Tool` | 生命周期或部署机制，不属于 Capability Core。 |
| Runtime | `Rejected / Deferred` | 名词过宽；具体 App runtime、scheduler 或 OS runtime 必须按各自边界命名和裁决。 |
| RTE | `Rejected / Deferred` | 现有探索可保留为证据，但该名称当前不进入 canonical Core 词汇。 |
| BSP / BoardFacts / Product / Workspace | `Project Fact` | 外部组合输入，不进入 Core。 |
| Resident ELF / ModuleX | `Implementation / Tool` | 可选 image 与 deployment 机制，不定义 Charm 身份。 |
| Charm OS | `Project Fact` | 未来可能形成的发行物或承载环境，不能反向定义 Charm Core。 |

特定 Capability Contract 仍需各自证明其行为语义。上表允许的是 `Capability Contract`
这一关系类别，不是自动批准所有名为 capability 的 API。

## 6. 语言纪律

`Core Gravity`、`审判庭`、`Composable Unit` 可以帮助讨论，但它们只是比喻或 deferred note，
不是正式术语。比喻不得出现在公共类型、canonical 模型或裁决等级中。

优秀的 Core 不以拥有多少概念衡量，而以能否长期拒绝不必要概念衡量。
