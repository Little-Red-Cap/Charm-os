# Vivid Layer Runtime v0 设计草案

## 定位

本文定义 Vivid 下一阶段的 UI runtime 主线：

> 页面何时是 live tree，何时是 frozen surface，何时只是 compositor input。

这不是一项“截图缓存优化”，而是把 Vivid 从：

```text
Widget Tree -> DrawCmd -> Canvas
```

推进到：

```text
Widget Tree -> DrawCmd -> Render Artifact -> Layer Runtime -> Backend
```

v0 的目标很窄：让复杂页面在转场期间不再每帧参与完整 layout / record / execute，而是先被冻结成可合成的渲染产物。第一验证对象是 Player 的 `Now Playing -> Library` 返回转场。

## 当前地基

Vivid 已经具备适合承接 Layer Runtime 的基础：

- `DrawCmdBuffer` 是明确的渲染中间产物。
- `Scene::render()` 与 `Scene::render_tiles()` 已经分离 record / execute。
- `CmdStats`、`ExecStats`、`TileStats` 已经能观察命令、批处理、tile、alpha blend 与失败项。
- `vivid_replay_workflow.md` 已经定义 dump/replay 与 full/tile backend 回放。
- `page_layers.cppm` 已提供页面内 `Backdrop / Content / Chrome / Popup` 分区。
- `layer_runtime.cppm` 已开始提供 `SnapshotHandle`、`SnapshotStore`、`LayerStats` 等 v0 代码地基。

需要注意：现有 `PageLayers` 是页面内分区，不是本文定义的 runtime layer。v0 应新增 Layer Runtime 概念，避免把 show/hide、页面分区和 frozen surface 混为一层。

## 当前实现快照

截至当前阶段，v0 已落地的部分是“命令快照 + PixelSurface 快照 + PageLayer freeze/thaw + transition abort 收尾 + profile gate + budget 账本 + 观测链 + 双层最小合成路径”。Player 的 Now Playing 转场已经可以通过 PageLayer 把 source page 与 destination page 都冻结成 PixelSurface，并在真实 render loop 中按 LayerProfile 裁决结果合成 frozen source / destination，再绘制 transition overlay。

已实现：

- `layer_runtime.cppm` 提供 `SnapshotHandle`、`SnapshotRecord`、`SnapshotStore`、`LayerStats`、`LayerComposePlan`、`LayerBudgetResult`、`LayerProfile`、`LayerProfileCaps` 与 `LayerProfileDecision` 等基础类型。
- `Scene` 支持 `capture_command_snapshot_result()`、`capture_command_snapshot()`、`capture_pixel_snapshot_result()`、`capture_pixel_snapshot()`、`release_snapshot()`、`validate_snapshot()`、`compose_snapshot_dry_run()`、`make_snapshot_compose_plan()`、`check_layer_budget()`、`replay_command_snapshot()` 与 `compose_pixel_snapshot()`，其中 PixelSurface compose 已支持 `opacity == 255` fast blit、`opacity == 0` skip，以及中间 opacity 的逐像素 alpha composite。
- `PageLayer` 已提供 `state()`、`snapshot()`、`freeze()`、`thaw()`、`release_snapshot()`、`mark_transitioning()` 与 `mark_stale()`，用于把页面 live root 与 frozen artifact 的生命周期绑定起来。
- `CommandBuffer` snapshot 会复制当前 `DrawCmdBuffer` 到固定槽位 payload store；释放 snapshot 时同步释放 payload slot。
- `PixelSurface` snapshot 会复制当前 canvas 像素到固定槽位 payload store；释放 snapshot 时同步释放 payload slot。
- snapshot 已绑定 `LayerEpoch`，当前覆盖 `layout / style / theme`。`CommandBuffer` replay 仍严格拒绝 epoch stale；`PixelSurface` compose 表达 frozen pixel artifact，只在显式 stale 或 payload 缺失时拒绝。
- `LayerCaptureStatus` 与 `LayerReplayStatus` 已提供失败原因，避免上层只通过空 handle 或空统计猜测问题。
- Player 的 Now Playing 转场已从命令快照 dry-run 推进到 PixelSurface 双层路径：转场开始时通过 PageLayer 捕获 source page、隐藏 source live root；render loop 中预渲染 destination page 并通过 PageLayer 捕获 destination snapshot，然后隐藏 destination live root，转场期间合成 frozen source / destination，再绘制 transition overlay。
- Player 侧已有最小 `PageTransitionState`，外部页面跳转打断 active transition 时会进入 abort 收尾，统一释放 source / destination snapshot、恢复 live root 可见性、清理 transition overlay，并回到 `Idle`。
- Player 侧已有最小 LayerProfile 裁决链：页面请求 `Rich / Cheap / Static / Eink / None`，转场冻结 source / destination 后按 layer bytes 与 composite pixels 预算生成 effective profile；预算超限时记录 fallback reason，并让 slide / opacity 走 profile caps 与 `resolve_layer_opacity()`。
- Player debug 日志已输出 profile 与 budget 账本，例如 `[layer] profile requested=... effective=... reason=...` 与 `[budget] layer_bytes=.../... composite_pixels=.../...`。
- Win Player rich profile 已把全屏 layer cache slot 扩到 2，用于同时持有 source / destination PixelSurface。
- `--ui-ci` 已覆盖 snapshot 生命周期、PageLayer freeze/thaw、命令快照捕获、容量耗尽、stale epoch、compose dry-run、compose plan clip、budget gate、LayerProfile 裁决、命令回放、stale 回放拒绝、payload slot 复用、PixelSurface 捕获/合成/opacity 合成/全屏 profile、Player 转场 source / destination PixelSurface capture/compose，以及 transition interrupt abort。

仍未实现：

- `CommandBuffer` replay 目前只验证回放边界和 clip，尚未支持平移、opacity 或真正 compositor 语义。
- `PixelSurface` compose 已支持 fixed payload + clip + x/y offset + opacity；后续仍需要 SIMD/scanline 优化与更完整 alpha format 策略。
- `TileSurface` snapshot 尚未落地。
- 当前 PageLayer 已能拥有一个 snapshot handle；更完整的多 overlay layer、transition recipe、取消动画与自动 stale propagation 仍待后续补齐。

## 核心概念

### PageLayer

页面生命周期容器，负责 live root、snapshot handle、状态、freeze/thaw hooks 与失效版本。

`freeze()` 不是 `hide()`：

- `hide()` 是可见性语义。
- `freeze()` 是渲染策略语义。

当前状态：

```cpp
enum class LayerState : std::uint8_t {
    Hidden,
    Live,
    Frozen,
    Transitioning,
    StaleSnapshot,
};

using PageLayerState = LayerState;
```

### SnapshotLayer

页面冻结后的渲染产物。它可以是像素表面，也可以是命令快照。

### LiveLayer

仍由 Widget Tree 驱动的页面。LiveLayer 可以响应输入、布局、状态刷新和 style 变化。

### OverlayLayer

Sheet、Popup、Toast、Focus ring 等覆盖层。v0 只定义术语，不强制实现。

### TransitionLayer

转场期间参与合成的临时层。它消费 snapshot，不直接驱动复杂页面刷新。

## Snapshot 类型

v0 至少定义两种实现，先不追求全部后端一次落地。

```cpp
enum class SnapshotKind : std::uint8_t {
    CommandBuffer,
    PixelSurface,
    EmptyFallback,
};
```

### CommandBuffer Snapshot

含义：

```text
冻结页面的 DrawCmdBuffer。
转场期间不再遍历 Widget Tree。
需要输出时重放冻结命令。
```

优点：

- 贴近现有 DrawCmd / dump / replay / tile 执行链。
- 内存压力低。
- 适合 small-mcu、e-ink 或低内存 profile。

代价：

- 每帧仍可能执行 DrawCmd，CPU 收益不如像素快照。
- transform 能力受限。v0 可以先支持 cut/fade，slide 需要裁剪和坐标偏移策略。

### PixelSurface Snapshot

含义：

```text
render once -> RGB565 / ARGB8888 surface -> transition blit/composite
```

v0 语义：

- `PixelSurface` 是 frozen pixel artifact，不等同于当前 live tree。
- 捕获后即使 live tree 的 `layout / style / theme` epoch 变化，也可以继续 compose，除非 snapshot 被显式标记 stale 或 payload 丢失。
- 这与 `CommandBuffer` snapshot 不同；命令快照依赖当前 scene 执行环境，回放仍必须拒绝 epoch stale。
- compose 支持三条 v0 路径：`opacity == 255` 直接 blit；`opacity == 0` 跳过；中间 opacity 逐像素 alpha composite，并在 replay stats 中记录 alpha blend 像素数。

优点：

- 转场期间最便宜。
- 最适合 PC、带 SDRAM 的 Player，以及 rich motion profile。

代价：

- 内存成本高。
- 需要后端提供 surface 分配、格式、stride 与 composite 能力。

参考内存量级：

```text
480 x 800 x RGB565   ~= 750 KB
568 x 1210 x RGB565  ~= 1.31 MB
```

### EmptyFallback

内存不足、snapshot stale 且不能重建、或后端不支持时的退化形态。

行为：

- rich profile 可尝试重建。
- cheap profile 可降级为 cut。
- e-ink profile 可跳过动画并直接重绘目标页。

## 最小 API 草案

```cpp
struct LayerEpoch {
    std::uint32_t layout{0};
    std::uint32_t style{0};
    std::uint32_t theme{0};
    std::uint32_t density{0};
    std::uint32_t font{0};
    std::uint32_t content{0};
    std::uint32_t image{0};
};

struct SnapshotSpec {
    Rect bounds{};
    SnapshotKind preferred_kind{SnapshotKind::CommandBuffer};
    PixelFormat preferred_format{};
    bool allow_alpha{false};
    bool allow_partial{false};
};

struct SnapshotHandle {
    std::uint16_t slot{0xFFFF};
    std::uint16_t generation{0};
};

struct LayerTransform {
    std::int16_t x{0};
    std::int16_t y{0};
    std::uint8_t opacity{255};
};

class PageLayer {
public:
    void set_root(WidgetHandle root) noexcept;

    void show(SceneAccess& access) noexcept;
    void hide(SceneAccess& access) noexcept;

    LayerCaptureResult freeze(Scene& scene, const SnapshotSpec& spec) noexcept;
    LayerCaptureResult freeze(Scene& scene,
                              SceneAccess access,
                              const SnapshotSpec& spec,
                              bool hide_live_root) noexcept;
    void thaw(Scene& scene, SceneAccess access, bool show_live_root = true) noexcept;
    bool release_snapshot(Scene& scene) noexcept;
    bool mark_stale(Scene& scene) noexcept;
    void mark_transitioning() noexcept;

    PageLayerState state() const noexcept;
    SnapshotHandle snapshot() const noexcept;
};
```

v0 里 `freeze()` 会按 `SnapshotSpec::preferred_kind` 选择 `CommandBuffer` 或 `PixelSurface` capture。`freeze()` 只表达渲染策略；是否隐藏 live root 由调用方通过 `hide_live_root` 显式决定。

## Layer Runtime 最小闭环

v0 只做四个能力：

```text
capture
release
compose
invalidate
```

推荐链路：

```text
source page live
  -> freeze source as snapshot

target page render once
  -> freeze target as snapshot 或 keep live hidden

transition frames
  -> composite source snapshot + target snapshot

transition end
  -> release source snapshot
  -> thaw target page as live
```

Player 的目标链路：

```text
NowPlaying live
  -> freeze(NowPlaying)

Library prepare
  -> freeze(Library)

Transition
  -> composite two layers

End
  -> release(NowPlaying snapshot)
  -> thaw(Library)
```

## 失效规则

Snapshot 最大风险是缓存地狱，因此 v0 必须绑定 epoch。

Snapshot 仅在以下版本一致时有效：

- layout
- style
- theme
- density
- font
- content
- image

建议规则：

```text
freeze() 记录 LayerEpoch
compose() 前检查 LayerEpoch
不一致时标记 stale_snapshot
```

stale 行为由 profile 决定：

```text
desktop / rich: rebuild snapshot
player-sdram: rebuild or cut
small-mcu: cut or command fallback
e-ink: skip animation, direct redraw
```

Reactive source 接入后，也必须映射到明确失效级别：

```text
theme.changed    -> style invalidation + repaint + snapshot stale
density.changed  -> layout invalidation + snapshot stale
language.changed -> text invalidation + layout maybe + snapshot stale
playback.changed -> content update + repaint
focus.changed    -> repaint only
```

## Motion 边界

Motion System 应踩在 Layer Runtime 上，而不是散落在页面里。

v0 只支持：

- `cut`
- `fade`
- `slide_x`
- `slide_y`
- `fade_slide`

v0 暂缓：

- shared element
- irregular mask
- per-widget snapshot
- cover 子层转场

Motion tier 草案：

```cpp
enum class MotionTier : std::uint8_t {
    rich_60fps,
    cheap_30fps,
    static_cut,
    eink_dissolve,
    none,
};
```

不同 tier 选择不同执行策略：

```text
fade:
  rich      alpha blend
  cheap     4-step opacity
  eink      dissolve / direct cut
  none      immediate swap

slide:
  rich      per-frame transform
  cheap     2~4 keyframes
  eink      disabled
```

## Render Budget

Layer Runtime 必须与 Render Budget 同步推进，否则无法判断 snapshot 是收益还是负担。

当前 v0 已先落地转场账本：Player 在 capture 后、motion compose 前计算 layer bytes 与预计 composite pixels，再把结果交给 `decide_layer_profile()`。这让页面只表达 motion 愿望，runtime 用 budget 与 profile caps 裁决有效执行形态：

```text
Motion 是愿望
Budget 是法律
Profile 是裁决
Backend 是执行
```

v0 需要新增 layer 维度统计：

```cpp
struct LayerStats {
    std::uint16_t snapshot_count{0};
    std::uint16_t snapshot_rebuild_count{0};
    std::uint16_t stale_snapshot_count{0};
    std::uint32_t layer_bytes{0};
    std::uint32_t composite_pixels{0};
};
```

建议统一观测字段：

```text
record:
  cmd_count
  cmd_bytes
  text_used
  blob_used

execute:
  dispatch_groups
  batch_flushes
  alpha_blend_count
  failed_cmds

tile:
  tiles_total
  tiles_drawn
  tile_flush_count

layer:
  snapshot_count
  layer_bytes
  composite_pixels
  snapshot_rebuild_count
  stale_snapshot_count
```

页面预算草案：

```cpp
struct PageBudget {
    std::uint16_t max_cmd_count{0};
    std::uint32_t max_cmd_bytes{0};
    std::uint32_t max_alpha_pixels{0};
    std::uint32_t max_layer_bytes{0};
};
```

日志目标：

```text
[budget] page=Library cmd=812/900 ok=1
[budget] page=Library alpha_pixels=42000/30000 ok=0
[budget] page=Library layer_bytes=768000/524288 ok=0
```

## v0 验收标准

以 Player `now_to_library` 转场为第一验收目标：

```text
target Library 不参与逐帧完整 layout
transition frame cmd_count 明显下降
snapshot bytes 可见
snapshot kind 可见
stale/rebuild 次数可见
final Library 可交互
ui-ci now_to_library 仍通过
```

建议增加日志：

```text
[layer] freeze page=NowPlaying kind=PixelSurface bytes=...
[layer] freeze page=Library kind=CommandBuffer cmds=...
[layer] compose page=Library opacity=...
[layer] release page=NowPlaying
[budget] page=Library layer_bytes=...
```

## 落地路线

### Step 1：文档与术语收口

- 本文定义 Layer Runtime v0。
- `vivid_page_layer_style_patch.md` 继续保留现有 `PageLayer` show/hide 使用说明。
- 后续代码中避免把页面内 `PageLayers` 与 runtime layer 混名。

### Step 2：新增 SnapshotStore 与 LayerStats

- 先实现 `CommandBuffer` snapshot。
- 暴露 snapshot kind、bytes、cmd_count。
- 不先做复杂 compositor。

### Step 3：接入 PixelSurface Snapshot

- PC / SDRAM profile 优先；Win Player 已启用 2 个全屏 layer cache slot。
- v0 已支持 full PixelSurface capture / compose，并在 Player Now Playing source / destination 双层转场中进入真实 render path。
- 后续再考虑 TileSurface。

### Step 4：最小 compose

- opacity
- x/y offset
- dirty rect

v0 可以先只在 win/sim backend 证明收益。

### Step 5：Player now_to_library 验证

- source page 已可冻结为 PixelSurface 并参与 compose。
- destination page 已可通过 render loop 预渲染后冻结为 PixelSurface。
- `NowPlaying -> Library` 这类复杂页回退路径已进入 source / destination 双层合成闭环；结束后仍回到 live tree。

### Step 6：Motion recipe 与 Component Lab

- motion 先基于 Layer。
- component lab 可复用 dump/replay，不另起体系。

## 非目标

v0 不做：

- 完整窗口管理器
- 任意层级 compositor
- shared element
- irregular clipping/mask
- 多窗口 scene graph
- GPU backend
- 自动 layout token 化

这些可以在 Layer Runtime v0 成立后再推进。

## 总结

Vivid Layer Runtime v0 的关键不是让某个页面画得更快，而是建立一个长期可用的问题分解：

```text
live tree       负责交互与状态
snapshot        负责冻结页面渲染产物
layer runtime   负责转场期间的合成与失效
backend         负责像素输出与设备差异
budget          负责证明收益和暴露代价
```

这一步完成后，Vivid 才真正开始拥有产品级 UI runtime 的骨架。
