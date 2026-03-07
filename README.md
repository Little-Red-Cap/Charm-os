<div align="center">

# Charm

**C++26 Modules · Zero-alloc · constexpr config · Type-level FSM**

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![C++26](https://img.shields.io/badge/C%2B%2B-26-blue.svg?style=flat-square)](https://en.cppreference.com/w/cpp)
<br>
[![Clang Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-clang.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)
[![ARM Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-arm-none-eabi.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)

> 面向 MCU/PC 的统一能力图系统：IO/系统/媒体/UI 可装配、可裁剪、可验证。

</div>

---

## 这是什么
Charm 是一个面向嵌入式与桌面环境的系统框架，核心是“能力图 + 非阻塞 IO + 统一装配”。  
它希望把分散的子系统变成可复用、可组合、可验证的模块集合。

## 设计关键词
- **能力图装配**：所有底层能力通过 `init.graph` 注册与启动
- **非阻塞 IO**：`Channel + Reactor + Registry` 三件套统一入口
- **单入口模块**：`charm.foundation / charm.runtime / charm.domain` 约束依赖边界
- **零动态内存**：默认固定容量与零分配策略
- **可观测**：统一 trace/诊断入口

## 文档入口（先看这里）
- 入门指南：`docs/overview.md`
- 文档索引：`docs/README.md`
- 架构总览：`docs/architecture_overview.md`

## 目录速览
- `Modules/core/` util/trace/service/alg/init
- `Modules/system/` kernel/modulex/boot/init/bringup
- `Modules/io/` channel/reactor/registry/hal/fs/shell/out/usb
- `Modules/media/` audio
- `Modules/ui/ink/` Ink UI
- `Modules/ui/vivid/` Vivid UI
- `Modules/platform/` board_caps/irq/clock
- `Examples/` 示例工程
- `docs/` 架构与协作
- `Draft/` 草案/实验

## 新功能接入三步
1) **板级描述**：在 `platform/board` 提供 UART/I2C/SPI 等能力描述  
2) **能力适配**：在 driver 层把外设暴露为 `Channel` 或 `block.device`  
3) **图中装配**：在 `init.graph` 注册 `provides/requires`，通过 registry 打开能力

## 硬约束（必须遵守）
- `Channel` 禁止 `Ok(0)`，无数据必须返回 `would_block`
- 协议层禁止 busy-spin/自带超时循环
- 禁止隐式全局入口，必须通过 Context 注入
- 未注册能力禁止直接 init

## 快速构建（Windows）
```bash
cmake -S Examples/kernel/windows -B Examples/kernel/windows/build -G Ninja
cmake --build Examples/kernel/windows/build
Examples/kernel/windows/build/os-example-win.exe
```

## 示例工程入口
- Kernel：`Examples/kernel/windows`
- USB CDC：`Examples/usb/usb_cdc_minimal`
- FS：`Examples/fs/`
- Audio：`Examples/audio/sdl3_wav_demo`
- Shell/Service：`Examples/shell/`、`Examples/service/`

## 状态
该仓库持续重构中，文档以 `docs/overview.md` 与 `docs/README.md` 为准。
