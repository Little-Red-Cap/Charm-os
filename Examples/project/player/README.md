# Player Project 示例

此目录用于音频播放器“项目化”验证，平台无关代码放在项目根目录，平台相关代码放在子目录（如 win、stm32）。

## 目录结构

```
Examples/project/player/
    CMakeLists.txt
    README.md
    player.app.cppm
    win/
        CMakeLists.txt
        main.cpp
```

## 说明

- `player.app.cppm`：平台无关的最小应用封装，便于后续扩展为完整播放器子项目。
- `win/`：PC 端实现（SDL3 / Windows）相关代码。
- 后续移植到 MCU 时，在 `stm32/` 下新增平台实现即可。

## 资源

- 示例音频：`Examples/project/player/assets/beautiful-trick.flac`
