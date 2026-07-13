# Project 示例入口

Project 目录包含跨模块、平台和构建 preset 的组合工程，不是单 module smoke。项目做法不能反向
定义 Charm Core 或通用 backend 契约。

## 仓库内项目

| 项目 | 用途 |
|---|---|
| [`h747-lab`](h747-lab/README.md) | 自制 STM32H747 板的 bring-up、resident runtime 与真实硬件验证 |
| [`player`](player/README.md) | Host/H747 的多能力 App 压力项目 |
| [`scope`](scope/README.md) | 较小的项目结构与能力组合样本 |

各项目可以包含 app、platform binding、preset、资产和硬件资料。具体状态以其源码、CMake 和当次
验证为准，不从本页推导。

## 已迁出项目

- DAPLink / CMSIS-DAP：[`Little-Red-Cap/Charm-dap`](https://github.com/Little-Red-Cap/Charm-dap)
- STM32G431 USB Audio：[`Little-Red-Cap/Nocturne`](https://github.com/Little-Red-Cap/Nocturne)

迁出项目不再是本仓库构建或回归入口。

全局文档权威和项目规范见 [`docs/README.md`](../../docs/README.md) 与
[`docs/project/README.md`](../../docs/project/README.md)。
