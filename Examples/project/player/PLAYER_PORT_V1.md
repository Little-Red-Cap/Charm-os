# Player Port v1 契约

## 定位

Player Port 是 Player 拥有的消费端窄口。它回答“Player 运行需要哪些最小事实”，不定义 Host、
SDL、Win32、H747 或 QEMU 如何提供这些事实，也不把 Player 产品功能提升为 Charm Host API。

v1 只消费四类能力：

- monotonic clock；
- borrowed raster surface 与 present sink；
- raw input source；
- 外部 run loop 对 Player frame 的调度。

音频、存储、字体文件、封面解码、截图、播放命令和 UI CI 不属于 v1 Port。
Player 的字体缓存与周统计时间分别通过 Player 私有 backend/source 注入；它们也不扩大 Host API。

## 接口

`player.port` 定义：

- `PlayerPort`
  - `clock`：Player-owned 单函数 monotonic-clock 投影；Player 不拥有时钟，adapter 从 Host clock 映射。
  - `raster_surface`：借用的像素内存、尺寸、stride 与 pixel format。
  - `raster_display`：present sink；adapter 决定上传、flush 或窗口呈现。
  - `raw_input`：有界轮询 `input::RawInputEvent`，没有 SDL event 语义。
- `PlayerRuntimeEndpoint`
  - 把具体 MD3 runtime materialize 为 Port 可驱动的生命周期回调。
- `PlayerPortRuntime`
  - 校验 Port，按 budget 排空 raw input，执行 update/render，并保证 shutdown 幂等。
- `PlayerMd3PortApplication`
  - materialize 真实 `PlayerMd3Runtime<PlayerController, PlayerPage>`；
  - 将 Port clock/raster/raw input 投影到现有 MD3 App/Scene runtime；
  - `has_track` 只表示曲目是否就绪，不再被误作 runtime bootstrap 状态。

Port 不暴露 `flush_cache`、`touch_sample`、`SDL_Window` 或产品资源路径。cache maintenance 和 touch
采样都属于具体 adapter；Player 只观察最终 raster present 与 raw input。

## 资源与调用约束

- `PlayerPort` 是对外部资源的借用投影。`clock/raw_input/raster_display` 的 `ctx`、像素内存和
  callback 从 `bootstrap()` 开始到 `shutdown()` 返回前都必须有效。
- raster surface 的尺寸、stride、格式和地址在一次 runtime 生命周期内保持不变。窗口或设备
  resize 需要先 shutdown，再以新 surface bootstrap；v1 不包含动态 resize 事件。
- raster 坐标原点固定为左上角，`x` 向右、`y` 向下，像素按 row-major 排列；`stride_bytes`
  是相邻两行起点之间的字节数，可以大于 `width * bytes_per_pixel`。Player 不得写每行末尾的
  padding，adapter 必须保证从 `pixels` 开始至少有 `stride_bytes * height` 个可访问字节。
- `RGB565` 是 native-endian `uint16_t`，数值位布局为 `RRRRRGGGGGGBBBBB`；`RGB888` 的内存
  字节顺序固定为 `R,G,B`；`ARGB8888` 是 native-endian `uint32_t 0xAARRGGBB`，alpha 为 straight
  alpha。native-endian word 规则与当前 Vivid raster 实现一致；跨字节序传输或远程显示必须由
  adapter 转换，不能让应用核心感知 transport byte order。
- dirty region 使用左闭右开矩形 `[x, x+w) x [y, y+h)`。`PlayerRasterDisplay::present()` 在调用
  adapter 前裁剪到 surface；完全为空的区域是成功的 no-op，不调用 adapter callback。adapter
  收到的 region 一定非空且在 surface 内。
- `present()` 是同步调用。返回时 Player 才认为当前帧已被消费；返回 `false` 会令 runtime 进入
  `Failed`。异步上传所需的拷贝、fence 或队列由 adapter 私有实现。
- `raw_input` 是可选输入源；空 callback 表示当前没有输入能力。非空 source 每次返回一个
  `input::RawInputEvent`，Player 只在 frame 的预算内轮询。pointer 坐标使用同一左上角 raster
  逻辑像素空间；adapter 负责窗口缩放、旋转、触摸校准和设备坐标转换。
- `now_us` 和 `dt_us` 单位固定为微秒。正式 run loop 必须提供单调不回退的时间；Player 不读取
  wall clock，也不推导平台 tick 频率。`0us` 是合法起点，runtime 不使用时间值充当初始化哨兵；
  runtime 检测到时间回退时进入 `Failed`，不会产生无符号下溢的超大 `dt_us`。
- Port clock 是 Player 实例的唯一 monotonic time source。MD3 materializer 可以把它投影为应用内部
  使用的实例时钟，但不得绑定或读取进程级全局 time source；多个 Player 实例不能互相覆盖时钟。
- v1 的 bootstrap、poll、update、render、present、shutdown 都在 run-loop 所在线程顺序执行，
  callback 不得重入同一个 `PlayerPortRuntime`。
- Port 不拥有关闭窗口、退出进程或设备热插拔语义。外部 run loop 决定何时停止调用 frame，
  然后显式调用 shutdown。

## 生命周期

```text
construct
  -> bootstrap(port)
  -> frame(now_us, dt_us) * N
       -> poll raw input (bounded)
       -> dispatch raw input
       -> update
       -> render raster
  -> shutdown
```

- Host/board/QEMU run loop 拥有主循环；Player 不调用 `while (running)`。
- adapter 把 run-loop 的 `now_us/dt_us` 传给 `frame(now_us, dt_us)`。
- `frame()` 的 clock convenience 入口只用于 smoke/简单宿主；正式 adapter 优先传入 run-loop 时间。
- 分阶段 run loop 使用 `update_frame(now_us, dt_us)` 与 `render_frame()`；两次 update 之间必须完成
  一次 render，重复 update 或重复 render 都会被拒绝。`frame()` 只是顺序调用二者的 convenience 入口。
- bootstrap 失败或 render 失败后状态为 `Failed`，不得继续 frame。
- endpoint bootstrap 已尝试后，shutdown 对 `Running`/`Failed` 最多调用一次 endpoint shutdown；
  Port 或 endpoint 校验失败而从未进入 endpoint bootstrap 时不调用它。
- raw input 每 frame 有 budget，防止输入洪水饿死 update/render。

## `player.board_*` 判决

| 现有接缝 | v1 判决 | 迁移去向 |
|---|---|---|
| `PlayerBoardFramebuffer` | 重命名语义，不保留 board 概念 | `PlayerDisplaySurface` / `PlayerPort::raster_surface` |
| `PlayerBoardDisplayCallbacks` | 从 Port 淘汰 | H747/Host adapter 私有 cache/flush/present 实现 |
| `PlayerTouchSampleSource` | 从 Port 淘汰 | adapter 输出 `input::RawInputEvent` |
| font package / cover provider / audio config | 从 Port 淘汰 | Player product config 或应用核心资源策略 |
| `make_player_board_app_config` | 淘汰 | 显式 Player product config assembly |
| `make_player_board_port_bindings` | 淘汰 | adapter 直接构造 `PlayerPort` |
| `read_player_board_touch_events` | 淘汰 | adapter raw input source |
| `player.board_runtime` | 兼容冻结 | `player.port_runtime` + MD3 endpoint |
| `player.platform` | canonical 淘汰、legacy 冻结 | canonical 已使用 `player.render_runtime`；旧模块只服务兼容清单 |

旧 `player.display/platform/runtime` 不删除，因为 H747 迁移仍引用它们；canonical source gate
禁止 import，且禁止新增消费者。

`player.input` 中的旧 touch sample/source 受 `CHARM_PLAYER_LEGACY_TOUCH_INPUT` 保护；canonical
固定为 `0` 并由 smoke 静态验证。新平台只能向 Port 提供 `input::RawInputEvent`。

## Canonical 构建方案

目标结构分两阶段落地：

1. 当前阶段：
   - `player-port-runtime-smoke` 验收最小 Port 状态机与失败边界；
   - `player-md3-runtime-smoke` 验收真实 MD3 controller/scene/render，不使用 SDL/Win32 API；
   - 同一 MD3 smoke 分别验收 RGB565、RGB888、ARGB8888 borrowed raster surface；
   - 三种格式各自固定 Player 私有 raster digest，UI 漂移不依赖 Host 截图 API 才能被发现；
   - `cmake/player_md3_sources.cmake` 是 canonical source manifest，禁止 smoke/adapter 各复制一份清单；
   - `charm_player_md3`（alias `Charm::player-md3`）是唯一 canonical CMake component target；
   - manifest 中的应用模块不得包含 SDL、Win32、H747、QEMU 或 `CHARM_PLAYER_HOST_*` 条件；
   - source manifest 在 CMake configure 阶段检查文件存在性与平台泄漏，违规直接失败；
   - 当前 `charm-player-win-vivid-md3` 保留可运行基线，但只在
     `CHARM_PLAYER_BUILD_LEGACY_VARIANTS=ON` 时出现，不代表最终 Host 契约；
   - 不改 `Backends/host/sdl3`，不改 H747 target。
2. Host SDL adapter：
   - 继续复用 `charm_player_md3`，不复制应用源码；
   - 薄 executable `charm-player-md3-host` 只负责 Host capability 到 `PlayerPort` 的投影；
   - `charm-player-win-vivid` 与 `charm-player-win-ink` 移入默认关闭的
     `CHARM_PLAYER_BUILD_LEGACY_VARIANTS`，只做兼容构建；
   - canonical CI 只验收 MD3 runtime smoke、Port smoke、Player-on-Host 与 Player 私有 UI CI。

这里的 canonical 指应用模型唯一，不要求所有执行环境共享同一个 executable target 名。未来 H747、QEMU
或 Linux adapter 都链接同一个 `charm_player_md3`，不得复制 MD3 核心。

## 跨平台适配判据

Host、QEMU、Linux、RTOS 和裸机 MCU 都是 Port 外部的执行环境，不形成不同 Player 模型。每个平台
只允许提供以下投影：

| 外部事实 | 投影到 Player | 不得泄漏的实现细节 |
|---|---|---|
| monotonic timer / run loop | `clock` 与 `frame(now_us, dt_us)` | OS tick、HAL timer、线程或 event-loop 类型 |
| framebuffer / texture / scanout | borrowed `raster_surface` 与同步 `present` | SDL texture、DRM handle、LTDC layer、cache API |
| mouse / touch / keys / encoder | `input::RawInputEvent` | SDL event、evdev struct、HAL GPIO/ADC handle |
| process/device lifecycle | `bootstrap/frame/shutdown` 的外部调度 | `main()` 形态、RTOS task、QEMU machine、板级启动顺序 |

一个新平台只有在不修改 `charm_player_md3` source manifest、不新增平台宏、不 import 旧
`player.board_*`，并通过同一 Port 与 MD3 runtime smoke 后，才算完成 Player 接入。没有真实板时，
Host smoke 仍能闭合应用/Port 语义；真实板只补充 cache、scanout、输入校准和时序等平台证据。

## 非目标

- 不定义通用 Host API。
- 不设计 SDL backend。
- 不修改 H747 Lab。
- 不平台化音频或存储。
- 不保留第二套 Ink/Vivid 应用模型。
- 不把 Player screenshot、resource policy、playback command 或 UI CI 变成 Charm capability。
- 不把 GDI font cache 或本地日历提升成通用 Host capability。
