# Charm 构建系统升级方向草案

上位总模型见：`docs/project/charm_工程对象模型草案.md`

## 背景

随着 `Player` 项目推进，`Charm` 不再只是一个模块集合，而是在真实复杂场景中承担平台职责。

这意味着：

- 代码架构需要收敛
- 运行时组织需要收敛
- 构建系统也必须同步升级

如果构建系统仍停留在“单工程 + 单入口 + 手工切换”的阶段，那么随着 `profile`、`runtime`、`bundle`、调试工作流、IDE 工作流不断增加，复杂度会持续泄漏到工程文件中，最终形成新的构建层屎山。

`Player` 当前暴露出的构建问题，不是个例，而是 `Charm` 平台能力成长到一定阶段后的必然信号。

## 当前暴露出的核心问题

### 1. 单工程单入口假设已经失效

当前同一块板上已经承载多个真实场景：

- `USB_AUDIO`
- `USB_SELF_MSC`
- `USB_STORAGE`
- 后续还可能有 `player-ui`、`audio decode`、`fs bringup`

这说明“一个工程只对应一个固件”的前提正在失效。

### 2. 场景切换需求是真实且长期存在的

`PLAYER_PROFILE` 不是临时技巧，而是真实存在的系统需求。

开发者需要稳定地表达：

- 当前要跑哪个场景
- 这个场景依赖哪些 runtime
- 这个场景带哪些 bundle
- 这个场景如何构建、调试、烧录

如果这些需求继续通过注释源码入口来表达，短期轻便，长期会迅速失控。

### 3. 现有 CMake 缺少中间抽象层

当前 CMake 的直接操作对象仍然主要是：

- `sources`
- `target`
- `compile definitions`

但复杂项目真正需要的构建中间层其实是：

- `profile spec`
- `board package`
- `runtime package`
- `bundle package`
- `flash/debug workflow`

缺少这层抽象，会导致所有逻辑都被挤压进单个 `CMakeLists.txt`。

### 4. IDE 工作流和命令行工作流尚未统一建模

从日常开发视角看，真正需要的不是单次成功编译，而是一条完整链路：

- 选场景
- 独立构建
- 选择调试/烧录配置
- 快速反复迭代

当前工作流已经开始引入 `preset`、独立构建目录和脚本，但这些能力还没有成为统一的构建层模型。

### 5. 共享板级资源的复用方式过于粗糙

当不同场景共享同一块板子的 CubeMX / HAL / USB / BSP 资源时，如果没有明确的构建组织方式，就会出现两类问题：

- 为了复用方便，把整个大包都拉进来，造成依赖扩散
- 为了隔离场景，又开始复制源列表，导致维护成本飙升

这表明我们需要正式的“板级共享包”概念。

## 能用但不好用的地方

### `PLAYER_PROFILE`

- 对：它准确表达了场景选择
- 不足：它目前仍然主要体现在单个 `CMakeLists.txt` 分支里，工程表达不够优雅

### `preset`

- 对：它解决了独立构建目录和缓存污染问题
- 不足：它更像使用层工具，还不是构建抽象的核心载体

### `custom target`

- 对：适合提供工作流提示和辅助动作
- 不足：不能替代真正的 executable target

### 独立 executable 尝试

- 对：这是 CLion / IDE 真正识别多个可执行目标的正确方向
- 不足：当前还缺少共享构建层，导致一旦复制就容易显得笨重

## 升级目标

### 目标 1：让场景构建成为一等公民

每个场景都应该有正式构建表达，而不是通过源码入口切换来间接表达。

例如：

- `usb_audio`
- `usb_self_msc`
- `usb_storage`

这些对象在构建层上应该是显式可见的。

### 目标 2：让共享板级构建资产成为正式概念

同一板子的共享能力应当被整理为可复用包，而不是散落在目标列表中。

例如：

- CubeMX 生成源
- HAL 源
- USB Device Core 源
- BSP 源
- 公共 include 路径
- 默认链接选项

### 目标 3：让 IDE 工作流成为系统能力的一部分

不是“会用 CMake 的人自己想办法”，而是仓库正式提供：

- preset
- 可执行目标
- flash / debug 建议路径
- 一键脚本

### 目标 4：让复杂度从主 `CMakeLists.txt` 撤离

主 `CMakeLists.txt` 应该回到“工程入口文件”的角色，而不是承载全部场景逻辑。

## 建议引入的构建层概念

### 1. `Board Package`

描述某块板子的共享构建资产。

内容包括：

- MCU / linker / toolchain 约束
- CubeMX / HAL 源
- USB / Audio / SDMMC 基础源
- 板级 include / compile definitions
- 调试/烧录配置入口

### 2. `Profile Spec`

描述一个场景如何构建。

内容包括：

- 入口源
- 场景实现源
- 需要哪些模块组
- 需要哪些 bundle
- 是否需要独立 executable
- 对应推荐 preset / workflow

### 3. `Runtime Package`

描述运行时胶水的构建集合。

例如：

- `hqzy_cm7_runtime`
- `hqzy_cm7_usb_runtime`
- `hqzy_cm7_storage_runtime`

### 4. `Bundle Package`

描述能力包在构建层的存在方式。

例如：

- `usb_storage_bundle`
- `audio_output_bundle`

### 5. `Workflow Target`

描述开发者侧的一键动作，而不是可执行产物本身。

例如：

- `flash_usb_audio`
- `debug_usb_audio`
- `build_usb_storage`

## 建议的文件组织方向

建议后续逐步将 `Player` 的构建逻辑从单个 `CMakeLists.txt` 中拆出：

```text
Examples/project/player/stn32h747_HQZY/CM7/
    CMakeLists.txt
    CMakePresets.json
    cmake/
        board_hqzy_cm7.cmake
        player_profiles.cmake
        player_targets.cmake
        player_workflows.cmake
```

建议分工如下：

- `CMakeLists.txt`
  - 工程入口
  - 引入共享 cmake 模块
  - 注册默认 target / profile
- `board_hqzy_cm7.cmake`
  - 板级共享源与公共定义
- `player_profiles.cmake`
  - profile 定义与场景声明
- `player_targets.cmake`
  - 从 profile 生成 executable 的规则
- `player_workflows.cmake`
  - preset / flash / debug / helper target 组织

## 演进顺序建议

### 第一阶段：先把构建抽象提出来

不要继续把新逻辑堆进主 `CMakeLists.txt`。

优先动作：

- 提取共享板级源列表
- 提取 profile 描述
- 提取 target 生成函数

### 第二阶段：先跑通一个独立 executable 样板

建议先以 `USB_AUDIO` 为样板：

- 让它成为真正可构建的独立 executable
- 跑通 CLion 识别与构建
- 清理长路径、共享源组织、第三方依赖整理问题

### 第三阶段：把其他 profile 迁入同一模式

当 `USB_AUDIO` 跑通后，再复制模式到：

- `USB_SELF_MSC`
- `USB_STORAGE`

### 第四阶段：把脚本与调试配置纳入工作流层

目标是让以下动作变成正式能力：

- build
- flash
- debug
- 场景切换

## 重要原则

### 原则 1：不回退到注释切入口

虽然它短期顺手，但会破坏长期结构纪律。

### 原则 2：也不接受把所有语义硬堆进单个 `CMakeLists.txt`

语义是对的，但承载位置必须收敛。

### 原则 3：构建层必须服务复杂开发，而不是逼开发者绕开系统

如果复杂场景下开发者总想跳回“手工改入口”，说明系统还没把抽象做顺手。

### 原则 4：实际项目是构建系统成长的驱动力

`Player` 不是普通示例，而是 `Charm` 的复杂场景压力测试器。

## 当前判断

`Player` 对 `Charm` 的价值，不只是推动 USB / Audio / Storage 功能前进。

它更重要的价值在于：

- 持续暴露架构中的真实摩擦
- 逼出真正有价值的抽象
- 逼构建系统从“能编译”成长为“能承载复杂开发”

因此，`Charm` 的下一阶段不应只理解为“继续推进 Player 功能”，而应理解为：

**利用 Player 反向驱动 Charm 平台化，包括代码架构、构建系统与开发工作流的同步升级。**

相关工程抽象模型见：[`charm_工程对象模型草案.md`](charm_工程对象模型草案.md)

复杂装配前的早期诊断取舍见：
[`early_diagnostics_retained_notes.md`](early_diagnostics_retained_notes.md)
