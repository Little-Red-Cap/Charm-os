# Player MD3

## 定位

Player MD3 是 Charm 的 canonical 产品应用示例，用真实 UI、播放领域逻辑和资源策略验证：
同一应用源码只消费稳定行为，不描述 Host、QEMU、OS、MCU 或 board identity。

Player MD3 是独立产品事实，不定义 Charm Core，也不拥有 Host backend。当前唯一 canonical
应用模型是 `app-vivid-MaterialDesign3`；旧 Ink/Vivid、Win 和 H747 路径仅作为兼容实现保留。

## 四层边界

- **应用核心**：MD3 UI、播放命令、媒体模型和 Player 私有资源策略。
- **Player Port**：clock、borrowed raster、raw input 与外部 frame lifecycle 的消费投影。
- **平台适配**：把某个 execution environment 提供的行为投影为 Player Port，不定义应用语义。
- **产品工具与测试**：截图、raster digest、UI CI、资源与播放诊断，不进入 Host API。

Player MD3 canonical 应用与平台接缝的当前入口：

- [PLAYER_FILE_OWNERSHIP.md](PLAYER_FILE_OWNERSHIP.md)：当前文件四层归属与依赖红线。
- [PLAYER_PORT_V2.md](PLAYER_PORT_V2.md)：当前 Player-owned 消费契约、实例状态、失败与预算边界。
- [PLAYER_PORT_V1.md](PLAYER_PORT_V1.md)：`player.board_*` 迁移判决与 v1 历史。

当前首轮边界是 Player MD3 / Player Port。Host SDL backend 与 H747 Lab 分别由其它工作线维护，
本目录不定义它们的实现。旧 Vivid/Ink target 暂时只作为兼容基线，MD3 是唯一 canonical 应用模型。

`host/player.host_sdl3_adapter.cppm` 是 Player 对已存在 Host SDL 契约的薄投影：它复用 Host
`Clock`、`RunLoop`、`RawSinkRef` 和 `RasterDisplay`，只负责组装 `PlayerPort`。SDL 类型、事件轮询
和 texture 仍由 `Backends/host/sdl3` 拥有；Player 播放命令、资源策略和 UI 状态仍由本项目拥有。

当前离平台验收入口：

- `Examples/system/player_port_runtime_smoke`：最小 Port 生命周期与失败状态。
- `Examples/system/player_md3_runtime_smoke`：真实 MD3 controller/scene/raster runtime。
- `Examples/system/player_instance_isolation_smoke`：storage/cover 实例隔离与单次扫描。
- `Examples/system/player_audio_binding_smoke`：source/sink 注入、失败和 legacy constructor。
- `Examples/system/player_host_input_burst_smoke`：Host 输入突发、合并和丢弃统计。
- `Examples/system/player_memory_report`：Player/Vivid/audio 静态内存账本。

canonical source set 由 `cmake/player_md3_sources.cmake` 显式维护。当前应用模块不包含 SDL、Win32、
H747、QEMU 或 `CHARM_PLAYER_MCU/HOST/BOARD/PLATFORM` 条件；GDI 字体缓存与本地周历只存在于
legacy Win adapter。

portability gate 同时扫描 canonical `.cppm/.hpp` 及其本地 `.inc/.tmp` 实现片段，并拒绝
OS、RTOS、vendor header/identity。文本 include 不能成为绕过应用边界的后门。

## Canonical 构建入口

默认 Player 根工程只构建平台无关组件 `charm_player_md3`，并提供 alias `Charm::player-md3`：

```powershell
cmake --preset player-md3-canonical-debug
cmake --build --preset build-player-md3-canonical-debug
ctest --preset test-player-md3-canonical-debug
```

上述 preset 固定复用仓库根 `cmake-build-player`。canonical component、Port smoke 与
真实 MD3 runtime smoke 共用同一套 Charm 构建产物，不再创建平行构建目录。MD3 smoke
分别运行 RGB565、RGB888、ARGB8888 三种 borrowed raster 格式。

ARM freestanding compile-only 同样复用该目录；切换 toolchain 前必须清理目录：

```powershell
cmake --preset player-md3-arm-freestanding-debug
cmake --build --preset build-player-md3-arm-freestanding-debug -- -j1
```

该 preset 只生成 `libcharm_player_md3.a`，不链接 firmware、不运行 QEMU、不占用板子。

旧 Win Vivid/Ink 仅在显式 opt-in 后出现，不参与 canonical 验收：

```powershell
cmake --preset player-legacy-win-debug
cmake --build --preset build-player-legacy-win-md3-debug
```

legacy preset 也复用同一个 `cmake-build-player`；切回 canonical 时重新运行对应 configure preset。
legacy storage VHD 没有仓库内默认路径；需要时显式传
`-DPLAYER_HOST_STORAGE_VHD_PATH=<path>` 或设置同名环境变量。

canonical Player-on-Host 执行端使用同一个 `Charm::player-md3`，只在显式 Host preset 中启用：

```powershell
cmake --preset player-md3-host-debug
cmake --build --preset build-player-md3-host-debug -- -j1
ctest --preset test-player-md3-host-debug
```

运行 `cmake-build-player/charm-player-md3-host.exe` 可打开实际 SDL 窗口。CTest 使用隐藏窗口和固定
三帧，只验证 Host clock/input/present/run-loop 到 Player Port 的接线，不接管 Player 截图或 UI CI。

旧架构收敛与能力地图已合并为
[归档保留笔记](../../../docs/archive/project-guidance-and-tracking-v0/player_architecture_retained_notes.md)，
不覆盖本 README、`PLAYER_FILE_OWNERSHIP.md` 或 `PLAYER_PORT_V2.md` 的当前边界。
当前 Port 契约以 `PLAYER_PORT_V2.md` 为准，V1 只保留迁移历史。

## 当前目录结构

```
Examples/project/player/
    CMakeLists.txt
    README.md
    CMakePresets.json
    cmake/
        player_md3_sources.cmake
        player_md3_target.cmake
        player_md3_vivid_product.cmake
    app-common/
        player.port.cppm
        player.port_runtime.cppm
        player.raster.cppm
        player.render_runtime.cppm
        player.md3_runtime.cppm
        player.*.cppm
    app-vivid-MaterialDesign3/
        player.md3_port.cppm
        player.controller.cppm
        player.ui.cppm
        player.ui_builder.cppm
    host/
        player.host_sdl3_adapter.cppm
        main.cpp
    win/
        CMakeLists.txt
        main.cpp
```

`app-common/` 中的 Player 模块和 `app-vivid-MaterialDesign3/` 中的 MD3 模块组成
`Charm::player-md3`。它们不得依赖 SDL、Win32、H747 或 QEMU。

`player.port*` 与 `player.raster` 定义 Player-owned Port 和生命周期。具体 Host、QEMU、
H747 adapter 不属于 canonical source set。`win/` 只在显式开启
`CHARM_PLAYER_BUILD_LEGACY_VARIANTS=ON` 时构建。

canonical render 闭包直接使用 borrowed `PlayerRasterSurface`：
`player.render_runtime -> player.md3_runtime -> player.md3_port`。旧
`player.display/platform/runtime` 仅供 H747/Win 兼容清单继续使用，不进入 `Charm::player-md3`。

canonical 固定 `CHARM_PLAYER_LEGACY_TOUCH_INPUT=0`，输入只从 Port 的
`input::RawInputEvent` 进入。旧 touch sample/source 仅供冻结兼容路径使用。

`cmake/player_md3_vivid_product.cmake` 由 Player 拥有，固定 MD3 使用的 Vivid PRODUCT
模块闭包和容量。平台 adapter 消费该配置，不得在板级重新定义应用的 UI 组成。

Player 产品 compile definitions 只施加到 `Charm::player-md3`；Charm runtime 只接收自身需要的
audio sink 选择。修改 Player 容量或资源策略不得触发整套 Charm runtime 重编。

canonical 构建使用 `cmake/player_charm_closure.cmake` 与通用 leaf helper，不链接聚合
`Charm-os`。当前干净 Host evidence 为 485 steps / 0.967 GiB；应用对象 5,352,744 bytes，
应用加 ARGB framebuffer 8,101,864 bytes。详细口径见 [PLAYER_PORT_V2.md](PLAYER_PORT_V2.md)。

旧板级目录、profile 和 runtime glue 当前仍保留，是迁移兼容物，不是 canonical Player
入口，也不参与本轮验收。H747 侧装配由独立工作线维护。

## 资源

- 示例音频：`Examples/project/player/assets/beautiful-trick.flac`

## 兼容主机字体构建

- 旧 Windows / Host adapter 需要 FreeType 才能走 TTF / OTF 字体链路；这不是 canonical Player Port 的必需依赖。
- CMake 会优先使用 `Modules/thirdparty/freetype`；若仓库内未放置源码，会继续尝试：
  - `CHARM_FREETYPE_DIR` / `FREETYPE_DIR` 环境变量
  - Cargo 缓存中的 `freetype-sys-*/freetype2`
- 如需显式固定路径，仍可传入：`-DCHARM_FREETYPE_DIR=<path>`
