# Player Project 示例

此目录用于音频播放器“项目化”验证，共用逻辑与 UI 变体拆分为 app-common / app-ink / app-vivid。

## 目录结构

```
Examples/project/player/
    CMakeLists.txt
    README.md
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
- `win/`：PC 端实现（SDL3 / Windows）相关代码。
- 后续移植到 MCU 时，在 `stm32/` 下新增平台实现即可。

## 资源

- 示例音频：`Examples/project/player/assets/beautiful-trick.flac`
