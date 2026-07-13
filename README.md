# Charm

Charm 是一个能力导向的嵌入式应用平台。

应用通过 Capability Contract 声明所需行为；运行环境声明可提供行为，并在应用启动前完成
Requirement、Provision 与 Binding 的解析。完整语义由
[`Charm Core Contract`](docs/architecture/charm_core_contract.md) 定义。

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![C++26](https://img.shields.io/badge/C%2B%2B-26-blue.svg?style=flat-square)](https://en.cppreference.com/w/cpp)
[![Clang Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-clang.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)
[![ARM Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-arm-none-eabi.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)

## 仓库范围

| 范围 | 入口 |
|---|---|
| Core 身份与准入 | [`CONSTITUTION.md`](CONSTITUTION.md) |
| Core 最小关系与边界 | [`charm_core_contract.md`](docs/architecture/charm_core_contract.md) |
| 当前源码分区与运行路径 | [`architecture_overview.md`](docs/architecture_overview.md) |
| 专题文档 | [`docs/README.md`](docs/README.md) |
| 示例、语义 smoke 与板级工程 | [`Examples/README.md`](Examples/README.md) |

Charm 不以某个 MCU、RTOS、Linux、编译器、驱动集合或 image format 定义自己。Host、QEMU 和
真实板是不同证据域；项目、BSP、backend、loader 与 Resident ELF 是消费者或实现，不自动获得
Core 身份。

当前 Capability MVP 的源码与三环境证据从
[`Examples/system/charm_capability_mvp`](Examples/system/charm_capability_mvp/README.md) 进入；该证据
验证局部命题，不替代 Constitution 准入。

## 使用顺序

1. 先读 [`CONSTITUTION.md`](CONSTITUTION.md) 判断概念是否有资格进入 Core。
2. 按 [`docs/README.md`](docs/README.md) 进入对应专题契约。
3. 以源码、CMake、真实 target 和当次测试核对实现状态。
4. Agent 操作与任务路由遵守 [`AGENTS.md`](AGENTS.md)。

核心审计完成前，不新增未经裁决的 Core 概念、公共身份或顶层架构主线。构建与专题任务从
[`docs/agent/routes/README.md`](docs/agent/routes/README.md) 进入。
