# Charm Core Contract

## 文档状态

- `status`: `canonical`
- `scope`: Charm 定位、最小关系、MVP、OS 边界与仓库所有权
- `authority`: 受根目录 [`CONSTITUTION.md`](../../CONSTITUTION.md) 约束

本文件是 Charm 唯一权威核心契约。它不定义具体 API、目录布局、生成器、运行时、
驱动模型或产品路线。任何专题文档与本文件冲突时，以 Constitution 和本文件为准。

## 1. 正式定位

> **Charm 是一个能力导向的嵌入式应用平台。**

核心主张是：

> **应用描述行为，并只通过 Capability Contract 声明对运行环境的要求；具体实现、平台和操作系统由组合关系与外部承载层决定。**

Charm 当前不是完整操作系统，不以某个 MCU、RTOS、Linux、编译器、图模型或部署格式定义自己。

## 2. Charm 只回答三个核心问题

1. **应用需要什么行为？** 由 `Requirement` 指向 `Capability Contract`。
2. **当前环境能提供什么行为？** 由 `Provision` 指向同一 `Capability Contract`。
3. **这次组合选择谁满足谁？** 由 `Binding` 把 Requirement 与 Provision 关联起来，并在应用启动前解析。

其它问题都必须先证明自己不能由这三个问题派生，才有资格申请进入 Core。

## 3. 最小关系模型

```text
Application
  -> declares Requirement
  -> Requirement targets Capability Contract

Implementation
  -> asserts Provision
  -> Provision targets Capability Contract

Profile
  -> selects Binding(Requirement, Provision)

Resolution
  -> ResolvedBinding | ResolutionFailure
  -> Application starts only after all required bindings resolve
```

### 3.1 Capability Contract

Capability Contract 定义消费方可依赖的行为、输入、输出、错误和必要不变量。它不定义：

- 谁实现它；
- 使用哪个外设或 OS API；
- 如何调度、传输或存储；
- 如何在某种语言或 ABI 中表达。

一个 C++ interface、C ABI table、RPC schema 或 shared-memory protocol 都只是同一契约的投影。
Interface 不得反向拥有 Contract 的语义。

### 3.2 Requirement

Requirement 是消费方对 Capability Contract 的依赖声明。它必须来自应用行为，而不是来自
某个 provider 的配置便利。应用不得在 Requirement 中包含 vendor 名称、HAL handle、目标名或
provider identity。

### 3.3 Provision

Provision 是某个实现对 Capability Contract 的满足声明。它必须可被验证，不能只靠类型名或
注册成功来证明。

`Provider` 只是 Provision 关系中的提供方角色。Charm Core 不定义统一 Provider 基类、
Provider Manager 或全局 Provider Registry。

### 3.4 Binding 与解析结果

Binding 是在一次组合中选择某个 Provision 满足某个 Requirement 的关系。它不是服务定位器，
不是依赖注入容器，也不是 runtime object graph。

解析完成后的 `ResolvedBinding` 或 `BindingSnapshot` 是结果物。MVP 允许 Profile 和 Binding
由人手写，只要求显式、可审查和可重复；Compiler 生成不是成立前提。

缺少 required capability、存在不允许的歧义或契约不兼容时，必须在应用启动前产生稳定的
`ResolutionFailure`。不得让应用运行后再通过空指针、平台分支或 provider fallback 猜测环境。

## 4. 不属于最小模型的内容

### 4.1 Component

MVP 中的 Component 只是静态装配单位，用于承载 Application、Requirement 或 Provision 的归属。
它不是所有对象的通用基类，也不要求动态生命周期、继承体系、反射或注册中心。

### 4.2 Profile

Profile 是一次运行或产品组合的显式选择集合。它可以选择 Binding、执行环境和项目事实，
但不能创造或修改 Capability Contract 的含义。

### 4.3 Graph

Graph 只允许作为已知关系的派生表示。init DAG、runtime topology、ownership、resource conflict
和 hot-plug state 是不同问题，不得因为都能画成节点和边而合并为单一权威模型。

### 4.4 Backend、Driver、Compiler 与 Loader

这些可以是重要实现或工具，但不属于 Charm Core 身份：

- Backend 适配运行环境、ABI、内存模型或宿主服务；
- Driver 控制具体硬件或调用宿主接口；
- Compiler 可以检查、解析或生成 Binding；
- Loader 可以装载 ELF、ModuleX 或其它 image。

它们必须消费 Core 语义，不能通过实现结构反向创造 Core 语义。

## 5. 承载环境

Host、QEMU 和真实板是不同 Execution Environment。它们可以拥有完全不同的实现、时序、
内存模型和硬件真实性，但必须让应用看到同一 Capability Contract 语义。

```text
                         +-> Host implementation
Application -> Contract +-> QEMU implementation
                         +-> Real-board implementation
```

- Host 用于快速开发、确定性测试和语义对照。
- QEMU 用于验证固件形态、启动边界和可仿真的运行时行为，不假装完整复制真实硬件。
- 真实板提供外设、电气、时序、缓存、总线和实际执行的最终证据。

三者是证据域，不是三种 App model。

## 6. MVP Contract

MVP 验证以下命题：

> **同一应用源码无需描述目标平台，只声明所需 Capability Contract。**

### 6.1 MVP 应用

使用一个小型、非 UI、非产品化应用，固定声明：

- `TextSink`
- `Clock`
- `BlockDevice`

应用读取时间，向 BlockDevice 写入一条带时间戳的小记录，并通过 TextSink 报告语义结果。
具体文本格式和存储介质可以由测试契约固定，但应用不得知道 provider identity。

### 6.2 硬验收

- 同一份应用源码和 Requirement 声明运行于 Host、QEMU 和一块真实板。
- 应用代码不包含平台宏、vendor header、HAL handle、目标名或 provider identity。
- 三个环境只更换 Profile、Binding 和具体实现。
- 缺少任一 required capability 时，在应用启动前产生相同类别的 resolution failure。
- 三个环境输出同一语义结果和可比较 Evidence。
- Profile 与 Binding 首先允许手写，但必须显式、可审查和可重复。
- Resident ELF 可以作为后续 deployment projection 复用同一应用与契约，但不是 MVP 成立条件。

在以上证据全部存在前，任何单独的 host demo、QEMU 固件、板级 bring-up、Resident ELF、
Compiler artifact 或 capability graph 都不能宣称已经证明 Charm MVP。

## 7. Charm 与操作系统

Charm 不拥有完整 OS 身份。裸机、RTOS、Linux 或未来的 Charm OS 都可以承载 Charm 应用，
前提是它们通过实现和 Binding 满足应用所需契约。

未来若形成 Charm OS，它应是独立发行物：

```text
Charm OS distribution
  = Charm platform contracts and composition
  + kernel/runtime
  + system services
  + driver packages
  + BSP and product facts
```

Kernel、scheduler、process model、bootloader、driver framework 和 filesystem 可以是该发行物的组成，
但不得因此升级为 Charm Core 原语。Charm OS 的需求也不能反向改写所有 Charm 应用的模型。

## 8. 仓库所有权边界

| Charm 主仓保留 | 独立 Project 拥有 |
|---|---|
| Constitution 与 Capability Contract | 产品应用、资产与业务逻辑 |
| Requirement / Provision / Binding 最小语义 | Product Profile 与具体 Binding 选择 |
| 组合与 resolution 机制 | BSP、startup、linker 与 vendor SDK |
| Host / QEMU reference implementation | 生产级 board/backend/provider |
| Portable semantic smoke | 板测脚本、硬件证据与产品 CI |
| 最小 App / ABI / ELF 验证边界 | 具体部署策略、介质布局与产品生命周期 |

通用 Driver 可以成为独立驱动包，但不能因为“可能复用”直接进入 Core。

## 9. 证据边界

Host、QEMU、真实板、IO、装配和 image loader 的实现与验证属于 supporting 文档、源码和当次
smoke 结果，不在本契约中记录项目进度。它们可以提供 Core 审查材料，但不能因实现存在、构建
通过或样例运行就自动获得 Core 身份，也不能替代第 6 节要求的独立证据。

判断某项 MVP 或跨环境主张是否成立时，应核对对应源码、CMake target、正反例和实际运行证据；
不要从本契约推断当前完成度或下一项排期。
