# Charm 工程变体模型草案

上位总模型见：`docs/project/charm_工程对象模型草案.md`

## 1. 为什么需要工程变体模型

随着 `Charm` 从模块集合逐步走向复杂项目平台，单纯依靠“源文件 + target + 宏定义”已经难以承载真实工程需求。

在 `Player` 的推进中，我们已经真实遇到以下情况：

- 同一套应用逻辑需要直接在 Windows 上开发、运行和验证
- 同一套应用逻辑需要在 MCU 上对接 HAL / SDK / 板级外设
- 同一个项目会同时存在 UI、音频、存储、USB、自研协议栈等复杂子系统
- 同一个硬件上会并行推进多个实验场景
- 同一个设备会有多种传输与接法差异
- 项目会依赖不同层级的第三方库与资源处理流程

这些都说明：

> `Charm` 需要一套正式的工程变体模型，用来统一表达平台、板级、场景、实现差异、依赖和工作流。

如果缺少这套模型，复杂度最终会退化为：

- 大量 `#ifdef`
- 大量工程复制
- 大量入口切换
- 大量构建脚本特判

## 2. 目标

工程变体模型的目标不是让系统更抽象，而是让复杂工程更顺手、更稳定。

核心目标包括：

- 让复杂场景有正式表达方式
- 让 Host / MCU 开发路径统一化
- 让依赖按层次挂接，而不是到处散落
- 让实验代码纳入系统，而不是旁路系统
- 让构建、调试、烧录、资源处理成为正式工作流能力

## 3. 核心概念

### 3.1 `Platform`

`Platform` 表示宿主运行环境。

它回答的问题是：

- 程序运行在哪类系统上
- 底层能力由什么提供
- 默认接入哪些平台依赖

例如：

- `windows-sdl3`
- `stm32h747-hal`
- `linux-posix`

`Platform` 负责提供的通常是：

- 时间源
- 窗口/图形/输入能力
- 音频输出/输入能力
- USB / 文件系统 / 外设框架的底层对接入口

它不是具体硬件，也不是场景。

### 3.2 `Board`

`Board` 表示具体硬件实例。

它回答的问题是：

- 这块板子有什么外设事实
- 使用什么 linker / memory / flash 配置
- 具备哪些总线、GPIO、USB、SDMMC、屏幕、按键等资源

例如：

- `hqzy_cm7`
- `nucleo_f4`
- `windows_stub_board`

`Board` 是 `Platform` 的具体落点。

### 3.3 `Profile`

`Profile` 表示当前要运行的场景。

它回答的问题是：

- 当前要验证什么
- 当前场景的入口是什么
- 当前场景需要哪些 bundle / runtime / observability

例如：

- `usb_audio`
- `usb_self_msc`
- `usb_storage`
- `player_ui`
- `imu_bringup`

`Profile` 是复杂项目中最重要的开发表达单元之一。

### 3.4 `Runtime`

`Runtime` 表示运行时胶水与宿主绑定集合。

它回答的问题是：

- 某个场景运行时所依赖的板级 / 平台级胶水由谁提供
- 这些胶水如何组织为可复用集合

例如：

- `hqzy_cm7_runtime`
- `hqzy_cm7_usb_runtime`
- `windows_sdl3_runtime`

`Runtime` 不承载场景语义，而承载宿主环境的运行事实。

### 3.5 `Bundle`

`Bundle` 表示可复用的能力组合。

它回答的问题是：

- 某组能力如何作为稳定整体被装配
- 某个子系统如何从“每次手工拼接”进化成“可重复挂接”的系统单元

例如：

- `usb_storage_bundle`
- `audio_output_bundle`
- `ui_text_bundle`

`Bundle` 的作用是降低复杂场景装配成本。

### 3.6 `Variant`

`Variant` 表示同类对象的实现差异。

它回答的问题是：

- 同一个设备或模块在不同接法、不同实现、不同传输方式下如何被显式区分

例如：

- `imu_lsm6dsr_i2c`
- `imu_lsm6dsr_spi`
- `player_ui_ink`
- `player_ui_vivid`

`Variant` 用来避免宏分支和实现污染。

### 3.7 `Dependency Scope`

`Dependency Scope` 表示依赖所在层级。

它回答的问题是：

- 这个依赖属于平台、模块、项目，还是工具链

建议至少分为：

- `platform dependency`
- `module dependency`
- `project dependency`
- `tool dependency`

例如：

- `SDL3` 属于平台级依赖
- `HAL / CMSIS` 属于平台级依赖
- `FreeType` 属于模块级依赖
- `material_color_utils` 属于项目级依赖
- `OpenOCD` 属于工具级依赖

### 3.8 `Workflow`

`Workflow` 表示开发动作与工具链动作。

它回答的问题是：

- 编译前后需要做什么
- 如何烧录、调试、运行、导出、生成资源

例如：

- `generate_assets`
- `build_profile`
- `flash_openocd`
- `run_host`
- `debug_gdb`

`Workflow` 不应散落为临时脚本，而应成为构建系统可组织的一部分。

## 4. 概念之间的关系

建议用以下方式理解这些对象：

- `Platform`：我运行在哪类宿主环境上
- `Board`：我运行在这类环境下的哪块具体板子/实例上
- `Profile`：我这次想跑什么场景
- `Runtime`：这个场景运行所需的宿主胶水集合
- `Bundle`：这个场景需要哪些可复用能力组合
- `Variant`：同类能力的具体实现差异是什么
- `Dependency Scope`：依赖挂在哪一层
- `Workflow`：开发者如何构建、运行、调试和烧录

## 5. 用 Player 映射这套模型

以当前 `Player` 为例，可映射为：

### Platform

- `windows-sdl3`
- `stm32h747-hal`

### Board

- `hqzy_cm7`

### Profile

- `usb_audio`
- `usb_self_msc`
- `usb_storage`

### Runtime

- `hqzy_cm7_runtime`
- `hqzy_cm7_usb_runtime`

### Bundle

- `usb_storage_bundle`

### Variant

- `player_ui_ink`
- `player_ui_vivid`

### Workflow

- `build_usb_audio`
- `flash_usb_audio`
- `run_host_player`

## 6. 这套模型解决什么问题

### 问题 1：Windows / MCU 双路径开发

解决方式：

- 用 `Platform` 统一宿主差异
- 用相同 `Profile` 表达相同场景

### 问题 2：第三方依赖复杂且分层不同

解决方式：

- 用 `Dependency Scope` 明确依赖层级
- 不再把所有依赖都堆到工程顶层

### 问题 3：同一项目跑多硬件

解决方式：

- 用 `Board` 表达硬件事实
- 用 `Runtime` 提供胶水

### 问题 4：同一硬件跑多场景

解决方式：

- 用 `Profile` 显式管理场景
- 不回退到注释切入口

### 问题 5：同一设备多种接法

解决方式：

- 用 `Variant` 显式表达实现差异

### 问题 6：开发过程中存在大量实验代码

解决方式：

- 把实验代码视为“实验 profile”
- 纳入正式场景系统，而不是旁路入口

### 问题 7：编译前/中/后存在真实工作流动作

解决方式：

- 用 `Workflow` 承接 build / flash / debug / resource pipeline

## 7. 不该再继续使用的旧表达

以下方式可以临时救火，但不应成为长期主线：

- 注释切换 `main`
- 用 `#ifdef` 表达大场景差异
- 把所有逻辑塞进一个 `CMakeLists.txt`
- 把 Host / MCU 工程割裂成两套互不相通的系统
- 让实验代码长期以临时入口形式存在

## 8. 构建层应该如何承接这套模型

未来构建系统不应只提供：

- `add_executable`
- `target_sources`
- `target_compile_definitions`

而应逐步沉淀出更高一层的工程能力，例如：

- `charm_add_platform(...)`
- `charm_add_board(...)`
- `charm_add_profile(...)`
- `charm_add_runtime(...)`
- `charm_add_bundle(...)`
- `charm_add_workflow(...)`

这样复杂项目的表达会从“拼文件”升级为“声明系统对象”。

## 9. 演进建议

### 第一阶段：先把概念和边界钉死

不要再让 `Platform / Board / Profile / Runtime / Bundle / Variant / Workflow` 混用。

### 第二阶段：先用 Player 做试点

优先在 `Player` 中验证这套模型：

- `windows-sdl3`
- `hqzy_cm7`
- `usb_audio`
- `usb_self_msc`
- `usb_storage`

### 第三阶段：让构建系统开始理解这些对象

把当前 `Player` 中已经暴露出的构建复杂度，逐步从业务工程文件中抽离出来。

### 第四阶段：沉淀为 Charm 通用工程能力

使未来新项目可以直接复用这套结构，而不是从头再打一遍补丁。

## 10. 当前判断

`Charm` 现在需要的不只是更多模块，更需要一套正式的工程变体模型。

只有当这套模型建立起来后：

- 多平台开发才会自然
- 多硬件适配才会自然
- 多场景调试才会自然
- 复杂依赖管理才会自然
- 构建、调试、烧录、资源处理才会自然地进入系统纪律

因此，`Player` 当前暴露出来的复杂度，并不是“项目变乱了”，而是在逼 `Charm` 从代码架构继续成长为工程平台。

`Player` 的第一轮试点落地见：`docs/project/player_工程变体模型第一轮落地草案.md`
