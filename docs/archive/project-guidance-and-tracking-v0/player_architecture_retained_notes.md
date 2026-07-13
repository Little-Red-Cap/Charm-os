# Player 架构收敛与能力地图保留笔记

> status: `archived`
>
> scope: 旧 Player 多入口结构与产品能力域讨论

本文合并旧 `ARCHITECTURE_CONVERGENCE.md` 与 `PLAYER_SYSTEM_CAPABILITY_MAP.md` 中仍可复用的判断。
旧板级目录、功能完成度、近期路线和 M1-M4 排期已经失效。当前入口见
[`../../../Examples/project/player/README.md`](../../../Examples/project/player/README.md)、
[`PLAYER_FILE_OWNERSHIP.md`](../../../Examples/project/player/PLAYER_FILE_OWNERSHIP.md) 和
[`PLAYER_PORT_V1.md`](../../../Examples/project/player/PLAYER_PORT_V1.md)。

## 当时暴露的问题

早期 Player 同时存在多个 `main`、启动模型、场景 source list 和板级装配入口。实验 app、BSP、
profile、runtime glue 与产品代码混在旧 H747 目录中，导致：

- 同一 capability 被不同入口重复装配；
- board fact 与产品场景策略互相渗透；
- 实验路径比长期入口更容易构建；
- Host 和 MCU 通过条件编译维持两套应用结构；
- CMake 文件同时承担 source inventory、场景选择和板级事实。

这些是历史迁移背景，不描述当前 Player 目录。

## 保留的收敛原则

- 产品应用只保留一个 canonical source closure 和生命周期模型；
- `main` 只承担 executable shell 与组合入口，不复制业务实现；
- Player Port 表达应用需要的 clock、raster、input 等行为，不暴露 SDL、HAL 或 board identity；
- adapter 把 execution environment 的事实投影到 Port，不能定义 Player 页面、命令或资源策略；
- BSP 只拥有 startup、linker、MMIO、DMA/cache、pinmux 和外设实例等板级事实；
- profile/target 选择产品组合，不能成为第二份 capability 或应用模型；
- 实验场景需要独立 target、验收和退出条件，不能长期保留旁路 `main`；
- 兼容入口必须标明 consumer 和删除条件，不能伪装成推荐路径。

## 产品能力压力

旧能力地图记录了 Player 对平台边界的实际压力：

| 能力域 | 应用侧边界 | provider/实板责任 |
|---|---|---|
| 媒体库与资源 | 文件、资源和 metadata 语义 | 介质、文件系统、错误映射与容量 |
| 播放队列与状态 | queue、seek、repeat、history | 不涉及硬件身份 |
| 解码与 audio sink | PCM、播放状态和失败语义 | codec、I2S、DMA、时钟与 underrun |
| display 与 input | borrowed raster、present、raw input | framebuffer、flush/cache、触摸与按键 |
| 持久化与统计 | version、损坏恢复和容量降级 | eMMC/QSPI/file/KV provider |
| 诊断 evidence | 产品状态与计数器 | backend 状态、DMA/cache 和板级故障证据 |

Player 可以推动这些 contract 变得可用，但不能把产品私有路径、Store layout、FAT path、HAL handle
或 UI 页面结构提升为公共平台语义。

## 证据域

- Host 验证产品状态机、资源错误、decoder、Port 生命周期和 UI 产品行为；
- QEMU 可验证 startup、loader、trap、runtime domain 和不依赖真实外设的控制流；
- Real Board 验证显示、触摸、音频、存储、DMA/cache、带宽、电气与内存容量。

不同证据域不能互相替代。Host mock 不能证明外设可用，QEMU 缺少设备模型也不能否定实板事实。

## 不保留的旧状态

原文中的旧 H747 路径、USB profile 名、功能完成度、近期 UI/audio 排期和 Draft 引用均不作为当前事实。
判断现状时检查 Player 的 source manifest、CMake preset/target、Port smoke、产品测试和对应实板日志。
