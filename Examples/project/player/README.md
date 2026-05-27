# Player Project 示例

## 战线关系声明

- `track_kind`: `pressure`
- `track_status`: `active`
- 这条线的角色：
  - 它是 Charm 的真实项目压力线，用真实需求而不是抽象想象去逼共享能力面收敛。
- 它当前驱动的共享收敛面：
  - UI/Vivid 组合层与 helper 上收
  - profile / runtime / board glue 的组织方式
  - Audio / USB / Storage / UI 在真实项目中的装配边界
- 它不能反向重定义的仓库公共规则：
  - 不能把 Player 目录默认等价为普通示例区。
  - 不能继续保留平行启动模型、平行装配模型和长期旁路入口。
  - 不能把共享底座收窄成 Player 私有规则。

此目录用于音频播放器“项目化”验证，共用逻辑与 UI 变体拆分为 app-common / app-ink / app-vivid。

当前 Player 架构收敛基线见：`ARCHITECTURE_CONVERGENCE.md`

## 目录结构

```
Examples/project/player/
    CMakeLists.txt
    README.md
    profiles/
    runtime/
    app-common/
        player.app.cppm
        player.fs_utils.cppm
    app-vivid/
        player.ui.cppm
        player.ui_debug.cppm
        player.controller.cppm
        player.ui_builder.cppm
    app-ink/
        hqzy/
            player_app_state.cppm
            player_controller.cppm
            player_fs_utils.cppm
            player_ui_ink.cppm
    win/
        CMakeLists.txt
        main.cpp
```

## 说明

- `app-common/`：平台无关的最小应用封装与通用工具。
- `app-vivid/`：Vivid UI 版本的实现与调试辅助。
- `app-ink/`：Ink UI 版本（当前放板级实现，如 HQZY）。
- `profiles/`：场景装配入口，负责声明当前跑哪条系统主线。
- `runtime/`：板级运行时胶水与 bringup 主线实现。
- `win/`：PC 端实现（SDL3 / Windows）相关代码。
- `bsp/`、`stn32h747_HQZY/`、`app-test-hqzy/` 当前承载 MCU 侧板级与实验路径。
- 这些 MCU 目录正在收敛中，后续以 `ARCHITECTURE_CONVERGENCE.md` 为准逐步整理。

## MCU Profile 选择

- `stn32h747_HQZY/CM7` 现已优先通过 `PLAYER_SCENARIO` 选择启动场景，不再通过注释切换 `main`。
- 示例：`cmake -S Examples/project/player/stn32h747_HQZY/CM7 -B build/player-cm7 -G Ninja -DPLAYER_SCENARIO=usb_self_msc`
- 当前可选值见 `Examples/project/player/stn32h747_HQZY/CM7/CMakeLists.txt` 中的 `PLAYER_SCENARIO` 定义。
- `PLAYER_PROFILE` 目前仍保留兼容映射，但后续建议逐步退到兼容层。

## CLion / CMake Presets

- `stn32h747_HQZY/CM7/CMakePresets.json` 已提供独立场景预设，每个 profile 对应单独构建目录。
- 当前可选预设：`player-cm7-usb-audio`、`player-cm7-usb-self-msc`、`player-cm7-usb-storage`
- `player-cm7-usb-self-msc` 现在是只读自检入口，`player-cm7-usb-storage` 已切到可写 eMMC 导出入口。
- 在 CLion 中推荐直接选择这些 preset，而不是在同一个 `cmake-build-debug` 目录里反复切 target。
- 当前 MCU 侧真正的固件 target 统一为 `stn32h747_hqzy_CM7`；场景差异由 preset 对应的独立构建目录承载。
- 这样切换场景时会真正重新配置并生成独立 `elf`，不会继续复用上一份 profile 的构建缓存。
- CLion 具体使用建议见：`Examples/project/player/stn32h747_HQZY/CM7/CLION_WORKFLOW.md`

## 资源

- 示例音频：`Examples/project/player/assets/beautiful-trick.flac`

## 主机字体构建

- Windows / Host 侧 Player 需要 FreeType 才能走 TTF / OTF 字体链路。
- CMake 会优先使用 `Modules/thirdparty/freetype`；若仓库内未放置源码，会继续尝试：
  - `CHARM_FREETYPE_DIR` / `FREETYPE_DIR` 环境变量
  - Cargo 缓存中的 `freetype-sys-*/freetype2`
- 如需显式固定路径，仍可传入：`-DCHARM_FREETYPE_DIR=<path>`
