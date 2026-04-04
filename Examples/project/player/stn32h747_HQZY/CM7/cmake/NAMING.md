# CMake 模块命名规则

本目录中的 CMake 模块文件应尽量只表达一个主要对象层级，避免把产品、板级、平台、场景混在同一个名字里。

## 命名原则

- 产品层使用 `product_` 前缀
- 平台层使用 `platform_` 前缀
- 板级层使用 `board_` 前缀
- 场景层使用 `scenario_` 前缀
- 工作流层使用 `workflow_` 前缀
- 通用目标装配层使用 `target_` 前缀

## 示例

- `product_player_scenarios.cmake`
- `platform_stm32h747_hal.cmake`
- `board_hqzy_cm7_modules.cmake`
- `scenario_usb_audio.cmake`
- `workflow_flash_openocd.cmake`
- `target_player_base.cmake`

## 当前目录的建议方向

- `player_identity.cmake` 后续应演进为产品/平台/板级身份相关命名
- `player_scenarios.cmake` 后续应演进为产品场景相关命名
- `player_modules.cmake` 后续应逐步按板级/平台归属拆分
- `player_target_base.cmake` 后续应演进为目标装配层命名
