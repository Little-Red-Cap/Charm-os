# Player 文件四层归属清单

本文以当前 tracked files 和 CMake 接线为准，定义 Player 首轮收敛的文件所有权。
“归属”表示长期职责，不等于本轮立即搬动文件。标记为“混合”的文件必须先拆分，不能整文件改名掩盖依赖问题。

## 1. 应用核心

这些代码表达 Player 产品行为、MD3 UI、播放命令和资源策略。它们可以依赖 Charm 的稳定领域模块，
但不得依赖 SDL、Win32、H747 或 QEMU。

| 当前文件 | 判定 |
|---|---|
| `app-vivid-MaterialDesign3/player.controller.cppm` 与全部 `player.controller.*.inc` | MD3 控制器与播放命令，canonical 应用核心 |
| `app-vivid-MaterialDesign3/player.ui.cppm`、`player.ui_builder.cppm` 与全部 `player.ui_builder.*.inc` | MD3 scene/UI 构建，canonical 应用核心 |
| `app-vivid-MaterialDesign3/player.cover.cppm`、`player.cover_theme.cppm` | Player 封面与主题策略；文件解码策略后续通过产品配置注入 |
| `app-vivid-MaterialDesign3/player.ui_debug.cppm` | Player 私有调试 UI，不进入 Host API |
| `app-common/player.app.cppm`、`player.app_config.cppm` | Player 应用组合与产品配置 |
| `app-common/player.cover_resource.cppm`、`player.font_resource.cppm`、`player.font_resource_apply.cppm` | Player 资源策略 |
| `app-common/player.fixed_string.cppm`、`player.product_config.cppm` | Player 本地基础类型与容量策略 |
| `app-common/player.product_policy.hpp` | 平台无关的保守产品策略默认值；target/profile 只覆盖数值，不暴露平台身份 |
| `app-common/player.lyrics.cppm`、`player.media_library.cppm`、`player.media_scan.cppm` | Player 媒体领域逻辑 |
| `app-common/player.playback.cppm`、`player.playback_session.cppm`、`player.track_probe.cppm` | Player 播放领域逻辑 |
| `app-common/player.recent_history.cppm`、`player.stats_history.cppm` | Player 产品数据模型 |
| `app-common/player.storage.cppm`、`player.fs_utils.cppm` | 当前保留为 Player 私有资源/存储策略；首轮不平台化 |
| `app-common/player.scene_runtime.cppm` | Player 对 Vivid scene 的应用内使用边界 |
| `app-common/player.render_runtime.cppm` | 直接在 borrowed raster surface 上承载 Vivid scene/render，不含 board callback |
| `app-common/player.md3_runtime.cppm` | canonical MD3 应用 runtime，组合 controller、scene 与产品配置 |
| `app-common/player.input.cppm` | Player input model/translation；canonical 关闭旧 touch sample compatibility |
| `app-common/player.time_utils.cppm` | Player 私有 `WeekStampSource` 契约，不读取 OS 时间 API |

`player.font_cache.cppm` 现为 portable backend binding 与统计契约。GDI 实现已迁到
`win/player.font_cache_win32.cppm`，不属于 canonical application source set。

`app-common/player.host_features.cppm` 的文件名暂时保留，是为了不修改冻结中的 H747 显式 source
manifest；它当前实际导出的模块是 `player.product_policy`，源码不再包含 Host 条件。H747 迁移线解除
冻结后再做物理重命名。

canonical source gate 同时扫描 `player.product_policy.hpp`，并拒绝
`CHARM_PLAYER_MCU/HOST/BOARD/PLATFORM`。应用差异必须表达为产品事实，例如容量、资源探测方式或
是否要求 icon arena，不能再通过执行环境名称分支。

## 2. Player Port

这一层是 Player 拥有的消费契约和生命周期，不是 Charm Host API，也不包含具体平台实现。

| 当前文件 | 判定 |
|---|---|
| `app-common/player.port.cppm` | v1 消费契约：clock、raster surface/display、raw input |
| `app-common/player.port_runtime.cppm` | v1 生命周期：bootstrap、bounded input、update、render、shutdown |
| `app-common/player.raster.cppm` | 无 UI/平台依赖的 Port raster 投影、present sink 与 memory test sink |
| `app-vivid-MaterialDesign3/player.md3_port.cppm` | canonical MD3 到 `PlayerRuntimeEndpoint` 的 materializer |

Port v1 的规范见 [PLAYER_PORT_V1.md](PLAYER_PORT_V1.md)。

## 3. 平台适配

这些文件把某个执行环境接到 Player Port。平台适配可以依赖 SDL、Win32、板级事实或具体驱动，
但应用核心不得反向 import 它们。

| 当前文件 | 判定 |
|---|---|
| `app-common/player.board_port.cppm` | 兼容适配，停止扩展；拆分后淘汰 |
| `app-common/player.board_runtime.cppm` | 兼容适配，停止扩展；由 `player.port_runtime` 替代 |
| `app-common/player.mcu_policy.cppm` | MCU 特定策略，迁出 `app-common` |
| `app-common/player.display.cppm` | 旧 display + board callback 混合模块；canonical 已退出 |
| `app-common/player.platform.cppm` | 旧 Vivid render 包装；canonical 已由 `player.render_runtime` 替代 |
| `app-common/player.runtime.cppm` | 旧 MD3 runtime；canonical 已由 `player.md3_runtime` 替代 |
| `app-common/player.runtime_shell.cppm` | 旧 frame/run-loop 外壳；功能已由 Port runtime 接管，等待兼容消费者退出后淘汰 |
| `profiles/hqzy_cm7_usb_storage*` | H747/HQZY 产品 profile；冻结，等待 H747 迁移线认领 |
| `runtime/hqzy_cm7/player_ui_port_bridge.cppm` | H747 adapter；冻结，等待 H747 迁移线认领 |
| `stn32common/audio_sink_i2s.cppm` | STM32 音频 adapter；不属于首轮 Port |
| `win/main.cpp`、`main.host_module.cppm` | 当前过厚的 SDL/Win32 adapter 与产品工具混合体，后续拆薄 |
| `win/main.display_sdl.inc`、`main.input_sdl.inc`、`main.host_loop.inc`、`main.host_runtime.inc` | Host adapter 候选；等待通用 Host SDL 契约稳定后再改 |
| `win/main_ink.cpp` | 旧 Ink + SDL 独立入口，仅兼容，不参与首轮验收 |
| `win/player.font_cache_win32.cppm` | GDI glyph-cache provider，只由 legacy Win adapter bind |
| `win/player.time_utils_win32.cppm` | 本地周历 provider，属于 Player 产品适配，不进入 Host API |
| `win/player.host_features_compat.cppm` | 旧 Win 工具输出的兼容投影，不进入 canonical source set |
| `host/player.host_sdl3_adapter.cppm` | canonical Host SDL 到 `PlayerPort` 的薄投影；不拥有 Host 契约 |
| `host/main.cpp` | canonical Player-on-Host executable 入口与 Host 产品 frame pacing |

Player Host adapter 只从 `Backends/host/sdl3` 取得 clock、raster display、raw input 和 run loop，
并把它们投影到 `PlayerPort`；Host backend 不定义 Player 页面、命令、资源或生命周期。

`player.mcu_policy` 已不再被 MD3 controller import，也不属于 canonical source manifest；文件暂留仅供
旧显式板级清单兼容。

`player.display/platform/runtime` 同样只保留给当前 H747/legacy 显式清单。canonical target 不编译
它们，新平台不得以这些模块作为接入点。

平台适配按执行环境独立存在，但都只能构造同一个 `PlayerPort`：Host 可映射 SDL，QEMU 可映射
虚拟 framebuffer/input，Linux 可映射 DRM/Wayland/evdev，MCU 可映射 framebuffer 与板级输入。
这些名字、handle、线程模型、cache/flush 规则不得进入应用核心、Player Port 或 canonical target。

## 4. 产品工具与测试

这些能力服务 Player 产品开发和视觉证据，不得上升为通用 Host 能力。

| 当前文件 | 判定 |
|---|---|
| `win/main.screenshot.inc` | Player 截图工具 |
| `win/main.ui_ci*.inc`、`main.ui_ci.object_tree.cpp`、`main.ui_ci_shared.hpp` | Player UI CI 与证据 |
| `win/main.font_probe.inc` | Player 字体诊断 |
| `win/main.overlay_fx.inc`、`main.host_preview.inc` | Player preview/诊断策略 |
| `win/cmake/product_player_host_profiles.cmake` | 当前 Host 命名失真；后续改为 Player product profile |
| `win/cmake/product_player_host_resources.cmake` | Player 私有资源配置；必须移除本机绝对路径默认值 |
| `win/cmake/product_player_host_scenarios.cmake`、`product_player_identity.cmake` | 旧场景与 target 兼容配置 |
| `player.product_policy`（当前物理文件仍为 `app-common/player.host_features.cppm`） | Player product/test 编译策略；禁止新增 Host 命名 |
| `Examples/system/player_*_smoke` | Player 产品语义 smoke |
| `Examples/system/player_port_runtime_smoke` | Player Port v1 无平台 runtime smoke |
| `Examples/system/player_md3_runtime_smoke` | 真实 MD3 controller/scene/render 的无 SDL canonical smoke |
| `app-common/player.runtime_probe.cppm` | Player runtime 私有证据，不包含平台探测 |
| `cmake/player_md3_sources.cmake` | canonical MD3 显式 source manifest，供 smoke 与后续 adapter 共用 |
| `cmake/player_md3_vivid_product.cmake` | Player-owned Vivid PRODUCT 模块闭包与容量；平台只消费，不重新定义 |
| `cmake/player_md3_target.cmake`、`CMakePresets.json` | canonical component target 与推荐构建入口 |
| `README.md`、`PLAYER_SYSTEM_CAPABILITY_MAP.md`、`ARCHITECTURE_CONVERGENCE.md` | 产品文档；旧板级叙述需要逐步归档 |
| `app-vivid-MaterialDesign3/*.md` | MD3 设计、维护性与性能证据 |

`app-vivid-MaterialDesign3/*.tmp` 当前是 tracked include 片段，不应继续用 `.tmp` 表达正式源码。
后续先确认与对应 `.inc` 的生成关系，再改为 generated artifact 或正式 `.inc`；本轮不做机械删除。

## 依赖红线

```text
产品工具与测试 -> 平台适配 -> Player Port -> 应用核心
                            \-> Charm stable domain modules
```

- 应用核心不得 import 平台适配。
- Player Port 不拥有 SDL event pump、window、线程、截图、文件选择器或 UI CI。
- 平台适配不得定义 Player 播放命令、资源策略或页面状态。
- 任何需要 `_WIN32`、`SDL_*`、H747 HAL 或 QEMU 设备细节才能成立的代码，都没有资格进入应用核心或 Port。
