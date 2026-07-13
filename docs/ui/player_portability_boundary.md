# Player Portability Boundary

## 文档状态

- `status`: `supporting`
- `scope`: Player application、Port、adapter/provider 与 Host/Board 工具边界
- `authority`: [`PLAYER_PORT_V1.md`](../../Examples/project/player/PLAYER_PORT_V1.md)、
  [`PLAYER_FILE_OWNERSHIP.md`](../../Examples/project/player/PLAYER_FILE_OWNERSHIP.md)

本文不维护 CMake flag 默认值、H747 内存数字、迁移完成度或下一步任务。实际 source closure、target、
profile 和测试以当前仓库为准。

## Canonical 分层

```text
product tools/tests
    -> platform adapter
    -> Player Port
    -> Player application core
```

- **Application core**：MD3 UI、播放命令、媒体模型和 Player 产品资源策略；不依赖 SDL、Win32、H747、
  QEMU 或 board identity。
- **Player Port**：Player 自有的 clock、borrowed raster/display、raw input 与 frame lifecycle 消费契约。
- **Adapter/provider**：把某个 execution environment 的行为投影为 Player Port 或产品资源输入。
- **Product tools/tests**：截图、UI-CI、preview、font probe 和性能诊断；不进入应用语义或 Port。

Host、Linux、QEMU 和 MCU adapter 是同层实现，不能互相成为默认模型。

## Provider Ownership

| 能力 | Player 消费边界 | Adapter/provider 责任 | 禁止泄漏 |
|---|---|---|---|
| Display | borrowed surface、dirty、present 结果 | framebuffer/texture、format/stride、cache/flush/present | SDL texture、LTDC/DMA2D handle、窗口身份 |
| Input | raw pointer/button/axis/command 语义 | SDL/evdev/touch/encoder sampling 与坐标映射 | vendor event、HAL handle、页面直接读设备 |
| Clock | monotonic time/tick | Host clock、RTOS timer 或 board counter | wall-clock API、平台时间分支 |
| Audio | 播放命令、状态、PCM/sink 语义 | decoder/sink、I2S/DMA、backend DSP | CMSIS/host FFT workspace、codec handle |
| Storage/media | file/resource/scan 与错误语义 | VHD/FAT/eMMC/QSPI/file provider | host path、Store layout、filesystem handle |
| Font | builtin/file/package 资源记录与 typography result | package bytes、VFS/FreeType/provider lifetime | GDI/FreeType object、板级路径 |
| Cover/theme | resolved cover/theme product result | host decode、pre-decoded record 或 board resource | decoder buffer、album file policy |
| Diagnostics | stable counters/result | screenshot、日志、capture 与 board evidence | argv、输出文件名、UI-CI scratch |

Provider 只在真实 consumer 需要时建立。不存在 board consumer 时，不为对称性发明 registry、虚接口或
heap-owned provider graph。

## Host Boundary

Host shell 拥有 window/renderer/event pump、argv、preview profile、截图、GIF/PPM、UI-CI 和 file-backed
便利路径。它只能构造 Player Port/产品配置并驱动 canonical runtime，不能：

- 在页面/controller 中加入 `_WIN32`、SDL 或 host path 分支；
- 把 host cover/font/storage 实现写成产品默认语义；
- 绕过 Player input/frame lifecycle 直接调用 Scene；
- 用 host smoke 声明 board display/input/storage/audio 已可用。

## Board Boundary

Board adapter 拥有 startup、memory region、cache/DMA、framebuffer、touch/encoder、storage/audio peripheral
和中断事实。它向 Player 提供 Port/资源记录，不把 HAL 或 BSP 类型导入 application core。

外部 framebuffer 的 pointer、size、stride、format 和 ownership 必须显式；present 前后的 cache clean、
dirty flush、copy/flip 和 fence 由 adapter 闭合。input batch 使用固定容量，overflow/drop 有证据。

旧 `player.board_port`、`player.board_runtime` 和 H747-local bridge 是兼容 adapter，不是新平台入口；具体
淘汰判决见 `PLAYER_PORT_V1.md` 与当前 source consumer。

## Resource 与 Memory

下列状态仍由 Player 产品拥有，但可按具体 profile 选择固定容量实现：

- page/controller、queue、history、media index 和 playback state；
- resource path/key、cover/theme result、font/package metadata；
- render/runtime storage 与产品统计。

Host-only 可替换状态包括 window/texture、GDI/FreeType cache、decode scratch、screenshot/export、argv 和
UI-CI case scratch。它们不能因 Host 运行成功进入 canonical source closure。

Port/provider 必须说明：

- 谁拥有内存及其生命周期；
- 是否允许动态分配、阻塞、异常或线程切换；
- 固定容量、overflow、drop 和 partial failure；
- shutdown 后 callback/borrowed view 是否仍可能到达；
- Host/QEMU/Board 各自证明了什么。

## Portability Probe

Portability probe 不是硬件模拟。它用于证明：

- canonical Player 可在没有 Host window/decoder/font fallback 的条件下 materialize；
- frame 可写入 externally owned raster 并通过 Port present；
- input 通过同一 Port 进入产品语义；
- host-only diagnostics 和资源路径没有进入 application core；
- unsupported provider 能产生明确降级/错误，而不是隐藏 fallback。

真实板仍需独立证明 cache、DMA、display/input/storage/audio 和内存容量。推荐 smoke 与构建入口从
[`Examples/project/player/README.md`](../../Examples/project/player/README.md) 进入。

## 非目标

- 不定义 Backend/BSP 目录结构或跨仓 provider registry。
- 不把 Player 产品能力提升为 Charm Core。
- 不通过删除 rich Host 功能伪造 portability。
- 不在本文维护当前 feature flags、数值 baseline、issue 或迁移排期。
