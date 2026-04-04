# Player 工程变体模型第一轮落地草案

上位总模型见：`docs/project/charm_工程对象模型草案.md`

## 1. 目标

本文档不是继续抽象新的概念，而是把已经提出的工程变体模型，第一次真正映射到 `Player` 项目上。

第一轮只聚焦三个最关键对象：

- `Platform`
- `Board`
- `Profile`

原因很简单：

- 这三个对象已经在 `Player` 中真实存在
- 当前最大的混乱也主要来自这三层边界尚未稳定
- 只要先把这三层收住，后续 `Runtime / Bundle / Workflow / Variant` 才有清晰的承载位置

## 2. 试点范围

本轮试点仅围绕当前 `Player` 已经真实跑起来的主线展开。

### 当前重点场景

- `USB_AUDIO`
- `USB_SELF_MSC`
- `USB_STORAGE`

### 当前重点板级

- `hqzy_cm7`

### 当前重点宿主环境

- `stm32h747-hal`
- `windows-sdl3`

## 3. 先给 Player 当前结构做映射

### 3.1 Platform 映射

建议把当前 Player 中的宿主环境明确理解为两类：

#### `platform.windows-sdl3`

当前可映射对象：

- `Examples/project/player/win/`
- `Examples/project/player/app-vivid/`
- 现有 Windows / SDL3 相关示例链路

职责：

- 提供窗口、输入、图形、音频、时间等宿主能力
- 承接应用逻辑在 PC 上的开发与验证

#### `platform.stm32h747-hal`

当前可映射对象：

- `Examples/project/player/stn32h747_HQZY/CM7/`
- CubeMX / HAL / USB Device / 启动文件 / linker 配置

职责：

- 提供 MCU 真实硬件环境下的宿主能力
- 承接 USB、Audio、Storage 等真机验证

### 3.2 Board 映射

建议明确把当前板级对象沉淀为：

#### `board.hqzy_cm7`

当前可映射对象：

- `Examples/project/player/stn32h747_HQZY/CM7/`
- `Examples/project/player/bsp/`
- `Examples/project/player/runtime/hqzy_cm7/`

职责：

- 描述 HQZY 这块板在 CM7 侧的真实硬件事实
- 提供该板的运行时 bringup / USB / SDMMC / platform glue

当前问题：

- 板级事实散落在 `CM7/`、`bsp/`、`runtime/hqzy_cm7/` 多处
- 有些板级内容和场景内容仍有混叠

### 3.3 Profile 映射

建议明确把当前场景对象沉淀为：

#### `profile.player.usb_audio`

当前可映射对象：

- `Examples/project/player/profiles/hqzy_cm7_usb_audio.cpp`
- `Examples/project/player/stn32h747_HQZY/CM7/app/main-usb-audio.cpp`

#### `profile.player.usb_self_msc`

当前可映射对象：

- `Examples/project/player/profiles/hqzy_cm7_usb_self_msc.cppm`
- `Examples/project/player/profiles/hqzy_cm7_usb_self_msc.system.cppm`
- `Examples/project/player/stn32h747_HQZY/CM7/app/main-usb-self-msc.cpp`

#### `profile.player.usb_storage`

当前可映射对象：

- `Examples/project/player/profiles/hqzy_cm7_usb_storage.cpp`
- `Examples/project/player/stn32h747_HQZY/CM7/app/main-usb-storage.cpp`

当前问题：

- profile 的代码已经在收敛，但命名和构建映射还不统一
- 兼容入口还在 `CM7/app` 下保留，语义上容易反复污染主线

## 4. 第一轮落地的目标状态

第一轮不追求一次到位，而是追求把结构关系明确下来。

### 目标状态 1：目录语义明确

建议逐步形成如下认知：

- `platform/`：宿主环境实现
- `boards/`：具体板级定义与共享资产
- `profiles/`：场景入口与场景实现
- `runtime/`：平台/板级运行时胶水
- `bundles/`：可复用能力组合

在第一轮里，不一定要立刻全部改名，但至少要在文档和构建层上先按这个模型理解现有目录。

### 目标状态 2：构建系统开始显式理解这三层

构建系统至少要能回答：

- 当前选的是哪个 `Platform`
- 当前选的是哪个 `Board`
- 当前跑的是哪个 `Profile`

不要再把这些信息隐藏在：

- 注释切 main
- 大片 CMake 分支
- 临时脚本命名

### 目标状态 3：兼容层不再伪装成主线

当前兼容文件仍然存在是可以接受的，但它们必须被清晰视为：

- compatibility shim
- legacy bridge

而不是“看起来还像主线入口”。

## 5. 第一轮建议的目录映射方式

为了避免一上来大搬家，建议先采用“语义先行，物理渐进”的方法。

### 5.1 Platform 层

先不急着建立完整 `platform/` 目录，但要先在文档和构建中明确：

- `Examples/project/player/win/` 对应 `platform.windows-sdl3`
- `Examples/project/player/stn32h747_HQZY/CM7/` 对应 `platform.stm32h747-hal`

### 5.2 Board 层

建议逐步将 HQZY 的共享板级事实汇聚为一个清晰对象：

- `board.hqzy_cm7`

其实现上可以逐步整合：

- `bsp/`
- `runtime/hqzy_cm7/`
- `CM7/` 下的板级专属构建资源

### 5.3 Profile 层

`profiles/` 已经是当前最健康的方向，应继续强化。

建议后续统一命名模式：

- `player.profile.usb_audio`
- `player.profile.usb_self_msc`
- `player.profile.usb_storage`

不再继续让 `main-usb-*` 承载长期实现。

## 6. 对构建层的第一轮要求

第一轮不要求构建系统一次理解所有对象。

但至少要逐步具备以下能力：

### 能力 1：声明当前 profile

当前已有 `PLAYER_PROFILE`，方向正确，应保留。

### 能力 2：把 board 从 profile 中拆出来

不要让 profile 同时承担：

- 场景选择
- 板级共享源拼接

### 能力 3：为 platform / board / profile 建立构建映射文件

建议下一步不是继续堆主 `CMakeLists.txt`，而是逐步拆成：

- `cmake/player_platforms.cmake`
- `cmake/player_boards.cmake`
- `cmake/player_profiles.cmake`

## 7. 第一轮建议的实施顺序

### 步骤 1：先在文档中正式定义 Player 的三层映射

这一步就是本文档本身。

### 步骤 2：在构建层引入三层命名

例如：

- `PLAYER_PLATFORM`
- `PLAYER_BOARD`
- `PLAYER_PROFILE`

即使初期只有一个 board / 一个 MCU platform，也值得先把结构位留出来。

### 步骤 3：把 `CM7/CMakeLists.txt` 从“实现文件”降回“装配入口”

它不应该继续承担：

- 全部共享源罗列
- 全部 profile 特判
- 全部工作流拼接

当前已开始的第一步是：将对象身份与场景映射逻辑外移到独立 cmake 模块。

当前已开始的第二步是：将场景选择逻辑外移到独立 cmake 模块。

当前已开始的第三步是：将模块组定义外移到独立 cmake 模块。

当前已开始的第四步是：将主 target 的基础配置外移到独立 cmake 模块。

当前这些模块已开始按对象层级命名，而不是继续混用产品名与板级名。

当前已开始的第五步是：将 `stm32h747-hal` 的平台事实外移到独立 cmake 模块。

### 步骤 4：让 `USB_AUDIO` 成为第一条完整样板链

原因：

- 它是你当前最直接的使用场景
- 它同时覆盖 host / mcu / audio / usb / clion workflow 问题
- 它足够典型，适合成为样板 profile

### 步骤 5：把 `USB_SELF_MSC` 与 `USB_STORAGE` 套入同一模型

这样才能验证该模型不是只服务单一场景。

## 8. 当前不该做的事

### 8.1 不回退到注释切入口

那只是把复杂度重新藏回去。

### 8.2 不继续往一个 CMake 文件里堆更多特判

当前状态已经证明这样会迅速恶化维护性。

### 8.3 不急着一次定义所有对象的最终物理目录

第一轮应该先稳定概念与映射，再逐步做物理迁移。

## 9. 第一轮完成后的预期收益

如果这一轮落地成功，Player 至少会得到以下收益：

- 目录语义更清楚
- 场景与板级职责更清楚
- Host / MCU 之间的映射更自然
- 构建层后续收敛有清晰抓手
- 复杂度开始从业务目录和大 CMake 文件中撤出

更重要的是，这一轮不是只服务 `Player`。

它会成为 `Charm` 工程平台化的第一块真实样板地基。
