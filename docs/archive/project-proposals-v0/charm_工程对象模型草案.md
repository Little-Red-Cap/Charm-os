# Charm 工程对象模型草案

## 1. 这份文档的定位

这份文档是 `Charm` 在工程层面的总模型文档。

它不是某个具体项目的设计说明，也不是某个单独构建技巧的总结，而是用于定义：

- `Charm` 如何理解一个真实工程
- `Charm` 如何组织平台、板级、场景、依赖和工作流
- `Charm` 后续的代码架构、构建架构和开发工作流应该围绕哪些正式对象演进

这份文档的目的，是把复杂项目里不断出现的真实需求，统一到一套稳定的对象模型中。

## 2. 为什么需要工程对象模型

随着 `Charm` 逐步走向真实项目，工程复杂度已经不再只是“模块数量增加”，而是出现了系统性的多维变化：

- 同一套应用代码需要直接在 Windows 上开发、运行和验证
- 同一套应用代码需要在 MCU 上对接 HAL / SDK / 板级外设
- 同一个产品会存在多个实验场景与验证路径
- 同一个硬件会承载不同阶段、不同目标的固件场景
- 同一个设备会有多种实现差异与接法差异
- 同一个项目会依赖不同层级的第三方依赖与资源流水线
- 项目会在编译前、编译中、编译后触发多类开发动作

这些问题如果没有统一对象模型，就会退化为：

- 注释切入口
- 工程复制
- 大量宏分支
- 巨型 `CMakeLists.txt`
- 临时脚本到处生长

因此，`Charm` 需要的不只是“模块架构”，而是一个正式的工程对象模型。

## 3. 核心原则

### 原则 1：对象显式化

复杂度不应依赖隐式约定和手工习惯，而应以正式对象表达。

### 原则 2：语义优先于文件拼装

工程结构的核心不应是“把哪些源文件放进 target”，而应是“当前在构造什么对象”。

### 原则 3：Host / MCU 统一视角

Windows / Linux / MCU 不应被视为完全割裂的世界，而应统一在同一套工程对象模型下。

### 原则 4：试验也是正式场景

实验代码不应长期以旁路入口存在，而应成为正式场景的一部分。

### 原则 5：构建系统服务工程对象，而不是反过来

构建脚本应当围绕正式对象组织，而不是让对象概念被构建脚本结构绑架。

## 4. Charm 的通用工程对象

本模型建议 `Charm` 至少引入以下顶层对象：

- `Product`
- `Platform`
- `Board`
- `Scenario`
- `Variant`
- `Bundle`
- `Runtime`
- `Workflow`
- `Dependency Scope`

下面分别说明。

## 4.1 运行时分层补充

上述对象定义的是工程对象视角。

复杂装配前可能需要一个局部 early-diagnostics 边界，以承接：

- 最小日志输出
- 最小 panic / fault 输出
- 最小时间基准
- 最小启动身份或版本信息（仅在具体项目需要时）

这不自动构成新的全局 Runtime 或 Charm Core 概念。具体 Platform/BSP 可以提供早期 sink；只有形成
跨环境稳定消费者、失败语义和独立证据后，才应评估公共边界。

保留的取舍见：

- [`early_diagnostics_retained_notes.md`](early_diagnostics_retained_notes.md)

## 5. `Product`

`Product` 表示一个产品级或项目级组织单元。

它回答的问题是：

- 我正在做的是什么产品/项目
- 哪些资源、场景、依赖、验证目标属于这个产品

例如：

- `player`
- `sensor_hub`
- `boot_demo`
- `kernel_lab`

`Product` 是工程组织的最高层业务对象。

它不是平台，也不是板级。

## 6. `Platform`

`Platform` 表示宿主运行环境。

它回答的问题是：

- 程序运行在哪类环境上
- 这类环境默认提供什么能力
- 平台级依赖由谁承担

例如：

- `windows-sdl3`
- `linux-posix`
- `stm32h747-hal`

`Platform` 提供的通常是：

- 时间源
- 图形/窗口/输入能力
- 音频能力
- 平台文件系统或设备接入入口
- 平台级第三方依赖

## 7. `Board`

`Board` 表示具体硬件实例或具体部署目标。

它回答的问题是：

- 当前落在哪块板子或哪类具体宿主实例上
- 它有哪些物理资源与约束
- linker / flash / memory / pinout / transport 等现实条件是什么

例如：

- `hqzy_cm7`
- `nucleo_f4`
- `win_stub`

`Board` 是 `Platform` 的具体化。

## 8. `Scenario`

`Scenario` 表示当前要运行或验证的场景。

它回答的问题是：

- 当前要做什么验证
- 当前要跑哪条系统主线
- 当前需要挂哪些能力与运行时

例如：

- `usb_audio`
- `usb_self_msc`
- `usb_storage`
- `ui_preview`
- `imu_bringup`

这里建议优先使用 `Scenario` 而不是 `Profile`，因为它的语义更通用，也更贴近真实工程意图。

## 9. `Variant`

`Variant` 表示同类对象的具体实现差异。

它回答的问题是：

- 同一种能力或模块，在不同实现、不同接法、不同样式下如何区分

例如：

- `spi`
- `i2c`
- `ink`
- `vivid`
- `host_stub`
- `mcu_hal`

`Variant` 的作用是把“实现差异”从宏分支中提出来，变成显式模型。

## 10. `Bundle`

`Bundle` 表示一组稳定的能力组合。

它回答的问题是：

- 哪些能力会被经常一起装配
- 如何把复杂场景从“手工拼接”变成“复用式装配”

例如：

- `usb_storage_bundle`
- `audio_output_bundle`
- `ui_text_bundle`

## 11. `Runtime`

`Runtime` 表示运行时胶水与宿主绑定集合。

它回答的问题是：

- 某个平台/板级/场景运行时所依赖的胶水由谁提供
- 如何把这层胶水从业务场景中剥离出去

例如：

- `hqzy_cm7_runtime`
- `hqzy_cm7_usb_runtime`
- `windows_sdl3_runtime`

## 12. `Workflow`

`Workflow` 表示开发动作和工具链动作。

它回答的问题是：

- 构建前后需要发生什么
- 如何运行、调试、烧录、生成资源、导出产物

例如：

- `build`
- `flash`
- `run_host`
- `debug_gdb`
- `generate_assets`

## 13. `Dependency Scope`

`Dependency Scope` 表示依赖所属层级。

它回答的问题是：

- 这个依赖挂在哪一层最合理

建议至少区分：

- `platform dependency`
- `module dependency`
- `product dependency`
- `tool dependency`

例如：

- `SDL3`：平台级依赖
- `HAL / CMSIS`：平台级依赖
- `FreeType`：模块级依赖
- `material_color_utils`：产品级依赖
- `OpenOCD`：工具级依赖

## 14. 对象之间的关系

建议用下面的方式理解它们之间的关系：

- `Product`：我要做什么产品
- `Platform`：它跑在哪类宿主环境上
- `Board`：它落在哪个具体硬件/实例上
- `Scenario`：这次要跑什么场景
- `Variant`：同类能力的具体差异是什么
- `Bundle`：场景依赖哪些能力包
- `Runtime`：这些能力运行所需的胶水集合是什么
- `Workflow`：开发者如何 build / run / flash / debug
- `Dependency Scope`：依赖应该挂在哪一层

## 15. 用这套模型理解真实工程

### 示例 1：Player 在 Windows 上做 UI 预览

- `Product = player`
- `Platform = windows-sdl3`
- `Board = win_stub`
- `Scenario = ui_preview`
- `Variant = vivid`
- `Bundle = ui_text_bundle`
- `Workflow = run_host`

### 示例 2：Player 在 HQZY 上跑 USB Audio

- `Product = player`
- `Platform = stm32h747-hal`
- `Board = hqzy_cm7`
- `Scenario = usb_audio`
- `Variant = mcu_hal`
- `Bundle = audio_output_bundle`
- `Workflow = flash`

### 示例 3：同一个 IMU，I2C 和 SPI 两种接法

- `Product = sensor_hub`
- `Platform = stm32h747-hal`
- `Board = some_board`
- `Scenario = imu_bringup`
- `Variant = i2c` 或 `spi`

## 16. 这套模型要避免什么

这套模型的目标，不是创造更多名词，而是替代以下坏味道：

- 用注释切换入口文件
- 用 `#ifdef` 承担大量场景差异
- 用单个巨型 `CMakeLists.txt` 承担全部工程语义
- 用复制工程处理平台差异和板级差异
- 用临时脚本承接长期工作流

## 17. 对构建系统的要求

当工程对象模型建立后，构建系统就不应再只是围绕：

- `add_executable`
- `target_sources`
- `target_compile_definitions`

而应开始理解更高一层对象，例如：

- `charm_add_product(...)`
- `charm_add_platform(...)`
- `charm_add_board(...)`
- `charm_add_scenario(...)`
- `charm_add_runtime(...)`
- `charm_add_bundle(...)`
- `charm_add_workflow(...)`

也就是说，构建系统要从“文件装配器”逐步成长为“工程对象装配器”。

## 18. 对 Player 的意义

`Player` 不是这套模型本身，而是它的第一个高价值试点。

这意味着：

- `Player` 不应绑架 `Charm` 的通用模型
- 但 `Player` 可以作为最早、最强、最真实的试验场

## 19. 当前判断

`Charm` 现在最重要的成长，不只是模块数量增加，而是从代码架构逐步迈向工程平台。

这一步的关键，不是继续堆更多场景，而是先建立正确的工程对象模型。

一旦这套模型稳定下来：

- 多平台开发会更自然
- 多板级适配会更自然
- 多场景调试会更自然
- 依赖管理会更自然
- 构建、调试、烧录、资源生成也会更自然地收敛到系统内

因此，这份文档应被视为 `Charm` 工程层演进的总纲。
