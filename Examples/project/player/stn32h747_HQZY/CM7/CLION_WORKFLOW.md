# CLion 工作流

本文档用于 `Examples/project/player/stn32h747_HQZY/CM7` 的日常开发，目标是避免通过改入口文件切换场景。

## 推荐做法

- 使用 `CMakePresets.json` 中的独立场景 preset。
- 不要在同一个 `cmake-build-debug` 目录里反复切换 `PLAYER_PROFILE`。
- `Target` 只负责构建当前 preset 下的产物，真正决定场景的是 preset 对应的独立构建目录。

## 当前可用 preset

- `player-cm7-usb-audio`
- `player-cm7-usb-self-msc`
- `player-cm7-usb-storage`

## 在 CLion 中使用

- 打开 `Examples/project/player/stn32h747_HQZY/CM7`
- 让 CLion 识别 `CMakePresets.json`
- 在 CMake Profile / Preset 中选择 `player-cm7-usb-audio`
- 构建目标 `stn32h747_hqzy_CM7`

此时产物会生成到独立目录：

- `Examples/project/player/stn32h747_HQZY/CM7/cmake-build-player-usb-audio`

对应 `elf` 一般位于：

- `Examples/project/player/stn32h747_HQZY/CM7/cmake-build-player-usb-audio/stn32h747_hqzy_CM7.elf`

## OpenOCD 调试 / 烧录

仓库内已有板级 OpenOCD 配置：

- `Examples/project/player/stn32h747_HQZY/openocd_swd.cfg`

在 CLion 中新建 Embedded GDB Server / OpenOCD 配置时建议：

- Board config 指向 `Examples/project/player/stn32h747_HQZY/openocd_swd.cfg`
- Target 选择当前 preset 产出的 `stn32h747_hqzy_CM7`
- 对于听歌场景，推荐配套使用 `player-cm7-usb-audio`

## 一键构建与烧录脚本

- 仓库已提供脚本：`scripts/player_usb_audio_flash.ps1`
- 仅配置：`powershell -ExecutionPolicy Bypass -File scripts/player_usb_audio_flash.ps1 -ConfigureOnly`
- 仅构建：`powershell -ExecutionPolicy Bypass -File scripts/player_usb_audio_flash.ps1 -BuildOnly`
- 构建并烧录：`powershell -ExecutionPolicy Bypass -File scripts/player_usb_audio_flash.ps1`
- 仅烧录已有产物：`powershell -ExecutionPolicy Bypass -File scripts/player_usb_audio_flash.ps1 -FlashOnly`

## 推荐配对

- 听歌：`player-cm7-usb-audio`
- 自研 MSC：`player-cm7-usb-self-msc`
- 存储导出：`player-cm7-usb-storage`

## 原则

- 场景切换走 preset，不走源码入口切换。
- 功能特性开关可以用编译定义；运行场景选择不要退回宏分支。
