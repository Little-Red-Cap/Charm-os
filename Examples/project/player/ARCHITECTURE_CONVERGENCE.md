# Player 架构收敛方案（强约束版）

本文档不是讨论稿，而是 Player 后续重构的收敛基线。

目标不是“把代码摆整齐”，而是强制建立一套唯一可演进的结构，让 USB、Audio、UI、存储等能力都在同一条启动主线上推进。

当前判断：

- Player 最大问题不是某个子系统未完成，而是入口、装配、板级胶水、实验代码混在一起。
- 如果不先收敛架构，任何 USB/Audio/UI 推进都会不断返工。
- 因此，后续一切功能推进，以本文件为约束前提。

## 一、当前问题（不留情面版）

### 1. 入口混乱

当前至少同时存在以下几类入口：

- `stn32h747_HQZY/CM7/app/main.cpp`
- `stn32h747_HQZY/CM7/app/main-usb-as.cpp`
- `stn32h747_HQZY/CM7/app/main-usb-audio.cpp`
- `stn32h747_HQZY/CM7/app/main-usb-self-msc.cpp`
- `stn32h747_HQZY/CM7/app/main-usb-storage.cpp`
- `app-test-hqzy/main.cpp`

并且通过 `CMakeLists.txt` 中注释/取消注释的方式切换。

这不是“灵活”，这是把场景切换写进构建文件本体，导致：

- 构建入口不可审计
- 主线入口不唯一
- 实验路径长期常驻
- 任何人接手都不知道“当前正确入口”是什么

### 2. 启动模型并存

当前存在至少三类启动模型：

- 直接 `main -> 若干 init 函数 -> while(1)`
- `player_entry/system_entry` 形式的服务顺序启动
- `AppHost + CoreSystemChain + init.graph` 形式的系统装配

这意味着同一个能力可能有多种“正确接法”，最终结果就是没有正确接法。

### 3. 同一能力重复装配

USB 是最明显的例子：

- 一条路径走 `usb_system_init()` 的直接初始化
- 一条路径走 `app_init_graph` / `UsbMscBlockInitChain`
- 一条路径把板级 DCD 胶水塞进 `board_usb`
- 一条路径把实验 glue 放进 `app-test-hqzy`

这会直接破坏两个原则：

- 能力必须走统一装配
- 板级事实与场景逻辑必须分离

### 4. 目录职责失真

当前目录虽然已经有 `app-common`、`app-ink`、`bsp`、`stn32h747_HQZY` 等划分，但职责并未真正收敛：

- `bsp/` 里有板级能力，也混入了实验承载点
- `stn32h747_HQZY/CM7/app/` 既像板级入口层，又像场景仓库
- `app-test-hqzy/` 名义上是测试工程，实际上承担了最接近“正确系统主线”的实验装配职责

换句话说：

真正先进的那条路径，反而被放在了一个看起来像临时目录的地方。

## 二、收敛原则（后续一律遵守）

### 原则 1：Player 只允许一套系统启动模型

唯一认可的启动主线：

`main -> profile 选择 -> AppHost/CoreSystemChain -> init.graph -> run loop`

禁止继续扩散以下模式：

- `main` 内直接拼业务 init 顺序
- 不经过 graph 的服务拼装
- 某个实验为了省事再开一套旁路启动代码

### 原则 2：`main` 不承载业务差异，只承载 profile 选择

`main` 的职责必须退化为：

- 初始化最外层宿主环境
- 选择要运行的 profile
- 调用统一入口

`main` 不再直接决定：

- USB 还是 Audio
- 存储怎么挂载
- UI 要不要启
- 某块板卡要不要绕过 graph

这些都必须下沉到 profile。

### 原则 3：实验代码必须成为“受控 profile”，不能继续做“旁路入口”

USB MSC 自研栈、USB Audio、存储验证、UI bringup，这些都不是“例外工程”，而是 Player 系统下的不同场景。

因此：

- 可以有多个 profile
- 不能有多个风格不同的系统入口

### 原则 4：BSP 只表达板级事实，不表达场景策略

`bsp/` 只负责：

- 引脚/外设/句柄/中断/缓冲区等板级事实
- HAL 绑定与 driver adapter
- 板级 capability provider

`bsp/` 不负责：

- “当前跑 USB MSC 还是 Audio”
- “这个实验要不要启某项服务”
- “系统启动顺序怎么拼”

### 原则 5：目录名必须反映长期职责，不能让“临时名字”承载主线

像 `app-test-hqzy/` 这种名称如果承载主线能力，最终一定会误导维护者。

主线要么归入：

- `profiles/`
- `runtime/`
- `board/.../profiles/`

总之不能长期寄居在 `test` 语义目录里。

## 三、目标结构（建议落地形态）

以下不是一次性重命名要求，而是最终目标结构。

```text
Examples/project/player/
    app-common/                 # Player 领域公共逻辑
    app-ink/                    # Ink UI 领域逻辑
    app-vivid/                  # Vivid UI 领域逻辑
    bsp/                        # 板级事实、HAL 绑定、capability provider
    profiles/                   # 场景装配（新增，核心）
        player_core.cppm
        usb_msc_selftest.cppm
        usb_audio_bringup.cppm
        storage_bringup.cppm
        ui_ink_bringup.cppm
    stn32h747_HQZY/
        CM7/
            app/
                main.cpp        # 唯一 main，只做 profile 选择
                profile_select.cppm
                board_entry.cppm
            CMakeLists.txt
    win/
```

说明：

- `profiles/` 承载“场景差异”
- `bsp/` 承载“板级差异”
- `app-common/app-ink/app-vivid` 承载“领域逻辑差异”
- `CM7/app/` 只承载“该板该核的入口桥接”

这是后续必须逼近的方向。

## 四、现有文件的去留判断

### 保留并提升为主线候选

- `app-test-hqzy/app_system.cppm`
- `app-test-hqzy/app_runtime_bringup.cppm`
- `app-test-hqzy/app_usb_glue.cppm`
- `app-test-hqzy/app_sdmmc_glue.cppm`

原因：

- 它们已经更接近统一装配模型
- 已经开始使用 `AppHost/CoreSystemChain/init.graph` 思维
- 比旧式直接 init 更符合 Charm 的系统边界

但它们必须迁出 `app-test-hqzy/` 语义目录。

### 进入冻结/弃用状态

- `stn32h747_HQZY/CM7/app/player_entry.cppm`
- `stn32h747_HQZY/CM7/app/system_entry.cppm`
- `stn32h747_HQZY/CM7/app/usb_system.cppm`

处理原则：

- 不再新增功能
- 不再扩展依赖
- 后续作为迁移兼容层或直接删除对象

当前状态：

- `player_entry.cppm`、`system_entry.cppm` 保留导出，但已不属于主线模块组
- `usb_system.cppm` 仅保留为对 `runtime/hqzy_cm7/usb_storage_bridge.cppm` 的兼容 re-export

### 必须收敛的入口文件

- `stn32h747_HQZY/CM7/app/main.cpp`
- `stn32h747_HQZY/CM7/app/main-usb-as.cpp`
- `stn32h747_HQZY/CM7/app/main-usb-audio.cpp`
- `stn32h747_HQZY/CM7/app/main-usb-self-msc.cpp`
- `stn32h747_HQZY/CM7/app/main-usb-storage.cpp`

目标不是保留五个入口，而是收敛为：

- 一个 `main.cpp`
- 多个 profile 模块

当前阶段补充约束：

- `main-usb-*` 若仍存在，只允许作为兼容跳板文件存在
- 真正的场景实现必须迁入 `profiles/` 或 `runtime/`

## 五、唯一推荐启动链

后续 Player 在 MCU 上的启动链，统一为：

1. `main.cpp`
2. `profile_select`
3. `board_entry`
4. `runtime_bringup`
5. `AppHost/CoreSystemChain`
6. `init.graph`
7. `profile 注册额外节点/能力`
8. run loop

其中：

- `runtime_bringup` 只做底层宿主就绪
- `init.graph` 负责统一装配
- profile 负责声明“这次要跑什么”

## 六、迁移顺序（必须按顺序，不要并发乱改）

### M1：入口收敛

目标：不再靠注释切 `main`

动作：

- 在 `CM7/CMakeLists.txt` 中引入显式 profile 选择变量
- 收敛为单一 `main.cpp`
- 原 `main-usb-*` 变成 profile 或被 main 调用的薄适配层

完成标准：

- CMake 中不再通过注释切换入口源文件

### M2：启动模型收敛

目标：只保留 `AppHost/CoreSystemChain/init.graph` 主线

动作：

- 以当前 `app-test-hqzy/app_system.cppm` 为种子
- 把旧式 `player_entry/system_entry/usb_system` 标记为 deprecated
- 新功能只允许挂到统一主线上

完成标准：

- 新增功能不再接入旧式直接 init 路径

### M3：目录职责收敛

目标：把“主线代码”从 `app-test-hqzy/` 中迁出

动作：

- 将其重组为 `profiles/` 与 `runtime/board_entry` 概念
- `bsp/` 只保留板级事实与驱动适配

完成标准：

- 主线目录名称能够反映长期职责

### M4：实验场景制度化

目标：USB/Audio/UI/Storage 都用 profile 管理

动作：

- 每个 bringup 场景对应一个 profile
- profile 只负责“启哪些能力、注入哪些节点、跑哪种主循环策略”

完成标准：

- 不再新增 `main-xxx.cpp`

## 七、当前阶段的硬禁令

在完成 M1/M2 前，禁止新增以下内容：

- 新的 `main-*.cpp`
- 新的直接 init 旁路
- 在 `bsp/` 中写场景逻辑
- 在 `CMakeLists.txt` 中继续靠注释切入口

允许的事情只有：

- 为收敛入口而做的薄封装
- 为统一 profile 而做的重命名/搬运
- 为统一 graph 装配而做的适配

## 八、为什么这次要“够狠”

因为 Player 不是一个 demo 目录，而是 Charm 架构往真实项目落地的压力测试场。

如果 Player 继续允许：

- 多入口并存
- 多套启动模型并存
- 旁路实验长期驻留

那 USB、Audio、UI 的每一次推进，都会让 Charm 的系统边界继续变形。

这次收敛不是“整理目录”，而是在保护 Charm 未来的演化能力。
