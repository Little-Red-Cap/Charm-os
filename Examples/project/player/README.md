# Player Project 示例

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

- `stn32h747_HQZY/CM7` 现已改为通过 `PLAYER_PROFILE` 选择启动场景，不再通过注释切换 `main`。
- 示例：`cmake -S Examples/project/player/stn32h747_HQZY/CM7 -B build/player-cm7 -G Ninja -DPLAYER_PROFILE=USB_SELF_MSC`
- 当前可选值见 `Examples/project/player/stn32h747_HQZY/CM7/CMakeLists.txt` 中的 `PLAYER_PROFILE` 定义。

## 资源

- 示例音频：`Examples/project/player/assets/beautiful-trick.flac`
