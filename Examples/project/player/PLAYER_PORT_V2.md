# Player Port v2 契约

## 定位

Player Port v2 是 Player 产品拥有的可移植消费边界，不是 Charm Core，也不是 Host backend API。
平台只提供 clock、borrowed raster、raw input 和外部 run loop；Player 自己保留播放命令、资源策略、
截图与 UI CI。Host、QEMU、Linux 和真实板都只能绑定同一个 `Charm::player-md3`。

v1 的 `player.board_*` 判决和兼容迁移记录继续保存在
[PLAYER_PORT_V1.md](PLAYER_PORT_V1.md)。v2 不删除 H747/legacy 模块，但 canonical source gate
禁止新增消费者。

## Port 接口

- `PlayerRasterSurface::pixels` 是 `std::span<std::byte>`，容量必须至少为
  `stride_bytes * height`。`required_size_bytes()` 是唯一容量计算入口。
- `PlayerRawInputSource::poll()` 返回 `event/empty/failed` 三态；无事件不是故障。
- endpoint 的 `bootstrap/input/update/render` 返回 `PlayerPortErrc`，`shutdown` 幂等且
  `noexcept`。
- `PlayerPortFailure { stage, code }` 把失败固定到
  `validate/bootstrap/input/update/render`，失败后和 shutdown 后仍可查询。
- runtime 固定记录 frame、input 和 present counters；诊断不依赖平台日志文本。

Port 借用的 clock、surface、input 和 display 从 bootstrap 开始到 shutdown 返回前必须有效。
surface 尺寸、stride、像素格式和内存地址在一次生命周期内不可变化。resize 或设备重建必须先
shutdown，再用新 Port bootstrap。

## 生命周期

```text
construct
  -> bootstrap(port)
  -> update_frame(now_us, dt_us)
       -> bounded poll
       -> endpoint input
       -> endpoint update
  -> render_frame()
       -> endpoint render
       -> present
  -> shutdown()
```

- `frame()` 只是 update/render 的顺序 convenience 入口。
- 两次 update 之间必须完成一次 render；重复阶段返回 `invalid_state`。
- clock 回退返回 `clock_regressed`，不得产生无符号下溢。
- input 的 `empty` 正常结束本帧排空；`failed` 进入 terminal failure。
- present 拒绝返回 `present_failed`；endpoint 错误保持它返回的具体 code。
- endpoint bootstrap 一旦被调用，后续 shutdown 最多调用一次 endpoint shutdown。

## 实例状态

canonical Player 不依赖进程级 storage 或 cover 全局配置：

- `StorageBinding { ctx, mount_fn, path }` 随实例进入 runtime；`StorageSession` 在一次 bootstrap
  中最多 mount/scan 一次。
- runtime 启动成功与“存在可播放曲目”是两个结果；空媒体库不是 bootstrap failure。
- `PlayerCoverResourceProviderBinding` 随 `PlayerMd3RuntimeConfig` 进入 controller，所有 canonical
  resolve 都显式使用实例 binding。
- `audio::PlayerBindings` 同时注入 playback source 与 sink；文件大小和 duration probe 使用
  独立的 `probe_source_binding`，避免探测时重开正在播放的有状态 source。两者应来自同一种
  provider 语义，但必须是不同实例。H747 未迁移前仍可落到 legacy probe bridge。

legacy storage/cover 全局 API 和 `AudioPlayer(config, clock)` 暂时保留给 H747/旧 Win。CMake source
gate 限制兼容桥数量，并禁止 canonical 新调用；H747 迁移解除冻结后再删除。

## Host 薄适配

`player.host_sdl3_adapter` 只投影既有 Host `Clock/RunLoop/RawInput/RasterDisplay`。它不定义 SDL
契约，也不把 Player 工具提升为 Host API。

- 私有队列固定 256 项。
- 连续 pointer move、axis、encoder 可合并。
- 队列压力只增加 `coalesced/dropped`，不终止 Player。
- 每帧最多 dispatch 64 项；真实 Host pump error 才进入 terminal failure。
- 512 pointer events 加离散 key 的 smoke 固定验证 `received=514/coalesced=511/dropped=0`。

## Media bindings

`audio::AudioSourceBinding`、`AudioSinkBinding` 和 `PlayerBindings` 是 media 局部实现边界，不是
Charm Core。provider 拥有 source/sink 对象和存储；binding 只携带 context 与操作表。

source 提供 open/close 和现有 `StreamSourceRef`。sink 提供 clock、open/start/stop/close、fill
callback、format、period 与 underrun 观察。SDL、I2S、file、VFS、null 和 pumped-null 都只是
adapter。实例隔离 smoke 使用两个独立 provider 验证无状态串扰。

## 构建与预算

canonical target 唯一为 `Charm::player-md3`。`player_charm_closure.cmake` 显式列出 Player 所需
模块；不链接聚合 `Charm-os`，也不生成 net、CANopen、USB、kernel、boot、ModuleX 或 board-stub
对象。Player profile 关闭 audio demo/simulation facade、真实 spectrum backend 和 audio debug
模块，但保留 UI `SpectrumView` 与完整播放格式。

2026-07-14 的 Clang 18 干净 Host + SDL evidence：

- 491 Ninja steps，基线为 1221；
- `cmake-build-player` 为 1,016,225,251 bytes（0.946 GiB）；
- application object 5,767,168 bytes；
- ARGB framebuffer 2,749,120 bytes；
- application + framebuffer 8,516,288 bytes；
- Vivid resident upper bound 5,453,856 bytes；
- audio workspace 423,944 bytes；
- Host canonical/SDL tests 13/13，Vivid 最大 stack usage 3,624/4,096 bytes。

ARM preset 只编译 static component，不链接 firmware。它要求 Cortex-M7/Thumb/hard-float，
保持无 exceptions、RTTI 和 thread-safe statics；由于 Player/Vivid 需要 `<cmath>` 和容器，编译时
保持 `-ffreestanding`，并仅为 embedded libstdc++ headers 把 `__STDC_HOSTED__` 设为 `1`。该门禁
通过时只证明源码/ABI 可面向 bare-metal ARM 编译，不构成 strict-freestanding 标准库、板端内存
布局或运行证据。

`player.md3_port.cppm` 只导出固定容量 application facade、endpoint 和窄状态操作；5 MiB 级
Controller/Scene/App materialization 位于 `player.md3_port.cpp`，render/runtime helper 位于私有
`.inc`。该边界不使用 heap PIMPL，公开对象仍可静态实例化，并由 implementation unit 的
`static_assert` 验证真实状态不超过 5.5 MiB storage。`PlayerPage` 位于轻量
`player.md3_types`，不再迫使 Port 消费方反序列化完整 controller BMI。

这次拆分解除 w64devkit GCC 16.1 与 ARM GCC 17 对累计 module imports 的 compiler ICE：

- GCC 16.1 canonical component 冷构建为 477 Ninja steps，11/11 无 SDL tests 通过；
- ARM GCC 17 compile-only 为 439 Ninja steps，`libcharm_player_md3.a` 为 8,952,168 bytes；
- ARM Vivid 最大 stack usage 1,288/4,096 bytes。

普通 smoke 翻译单元固定先 include 标准库头、再 import Player modules，以避开 GCC Modules 对
重复 libstdc++ 声明的工具链缺陷；该顺序要求不改变 Player 契约。

## 非目标

- 不修改 `Backends/host/sdl3` API。
- 不修改或验证 H747 Lab。
- 不接真实 Host storage/audio，不新增 QEMU adapter。
- 不把 RasterDisplay candidate、media bindings 或 Player Port 提升为 Charm Core。
- 不删除 legacy H747/Win bridge，直到对应迁移工作解除冻结。
