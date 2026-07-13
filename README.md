# Charm

> **Charm 是一个能力导向的嵌入式应用平台。**

应用描述行为，并只通过 Capability Contract 声明对运行环境的要求；具体实现、平台和
操作系统由组合关系与外部承载层决定。

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![C++26](https://img.shields.io/badge/C%2B%2B-26-blue.svg?style=flat-square)](https://en.cppreference.com/w/cpp)
[![Clang Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-clang.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)
[![ARM Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-arm-none-eabi.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)

## Charm 与普通嵌入式框架的差异

Charm 不以 HAL 数量、驱动数量、支持的 RTOS 或目录规模定义自己。它首先保护应用依赖的
行为契约，再把具体实现作为可替换的组合选择。

因此，Linux、RTOS、裸机、Host、QEMU 和真实板都可以承载 Charm 应用，但它们都不是 Charm
本身。未来可以形成独立的 Charm OS 发行物，它也不能反向定义 Charm Core。

## 三个核心问题

1. **应用需要什么行为？** `Requirement` 指向 `Capability Contract`。
2. **运行环境能提供什么行为？** `Provision` 指向同一 `Capability Contract`。
3. **这次组合由谁满足谁？** `Binding` 关联 Requirement 与 Provision，并在应用启动前解析。

`Provider` 只是 Provision 关系中的角色，`Interface` 只是 Contract 的一种投影，
`Backend`、`Driver`、`Compiler`、`Graph`、`Loader` 和 Resident ELF 都是实现或工具，
不自动获得 Core 身份。

## 当前首先要证明什么

Charm MVP 只验证一句话：

> **同一应用源码无需描述目标平台，只声明所需 Capability Contract。**

最小应用只依赖 `TextSink`、`Clock`、`BlockDevice`，并以同一源码运行于 Host、QEMU 和一块
真实板。三个环境只能更换 Profile、Binding 和具体实现；缺少 required capability 时，
必须在应用启动前稳定失败。

完整验收见 [`docs/architecture/charm_core_contract.md`](docs/architecture/charm_core_contract.md)。

## 当前已经做到

- 仓库已有 Host、QEMU 和真实板的独立运行与证据链。
- 非公共 Capability MVP 已在 Host、真实 QEMU 固件和 H747 实板运行同一 App，三域得到相同
  timestamp/checksum，并证明缺失 required capability 时不会启动 App。
- 已有 IO、装配、运行时、板级和 Resident ELF 等实现，可作为 Core 审判与 MVP 复用材料。
- 已有多个真实项目持续验证代码能否离开单一宿主或单一板卡。

这些事实已经证明当前 exploration MVP 的跨环境命题，但不会自动批准任何既有名词进入 Core。

## 当前尚未做到

- MVP 的候选关系与 Contract 投影仍位于 exploration；正式公共所有权与准入尚未裁决。
- 尚未完成全仓术语审计、唯一术语表与统一路线图。
- Project 外移、目录重整、C/C++ 规范合并和 CMake 收敛尚未开始。
- Charm 不是完整 OS，也没有承诺进程隔离、生产级驱动生态或自动 Binding Compiler。

## 阅读顺序

1. [`CONSTITUTION.md`](CONSTITUTION.md)：什么有资格进入 Charm Core。
2. [`docs/architecture/charm_core_contract.md`](docs/architecture/charm_core_contract.md)：定位、最小关系、MVP 与 OS 边界。
3. [`docs/README.md`](docs/README.md)：canonical、supporting、exploration 和 archive 的文档路由。
4. [`AGENTS.md`](AGENTS.md)：仓库协作与操作规则。

当前实现盘点从以下 supporting 文档进入：

- [`docs/overview.md`](docs/overview.md)
- [`docs/architecture_overview.md`](docs/architecture_overview.md)

它们不能覆盖 Constitution 或核心契约。核心收敛前的多战线快照已归档到
[`docs/archive/repo-tracks-pre-core-reset/README.md`](docs/archive/repo-tracks-pre-core-reset/README.md)。

## 当前停线规则

在核心审计完成前，默认暂停新增核心概念、公共 API、顶层目录、架构主线和大规模 CMake
能力。现有功能允许修复、闭环和提供证据；任何新名词必须先通过 Constitution 的六问裁决。

构建和专题入口继续从 [`docs/project/README.md`](docs/project/README.md) 与
[`docs/agent/routes/README.md`](docs/agent/routes/README.md) 按任务进入，现有构建与证据流程不因
本轮文档收敛而改变。
