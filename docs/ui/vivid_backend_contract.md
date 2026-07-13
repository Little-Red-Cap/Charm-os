# Vivid Backend Contract

## 文档状态

- `status`: `contract`
- `scope`: Vivid DrawCmd、framebuffer 与 platform render backend ownership
- `authority`: [`ui.render_backend.cppm`](../../Modules/ui/common/ui.render_backend.cppm)、
  [`framebuffer.cppm`](../../Modules/ui/vivid/gfx/framebuffer.cppm)

## 公共边界

UI/widget 只提交 layout、state、invalidation 和 DrawCmd，不感知具体 display controller、传输总线或刷新
策略。`RenderBackend` 只要求：

```text
width / height
begin_frame / end_frame
blit_span
mark_dirty
```

`FrameBufferView` 描述 pixel format、data、width、height 和 stride。字段与受支持格式以当前 source 为准，
文档不复制 enum 或 traits 表。

## Ownership

| 层 | 负责 | 不负责 |
|---|---|---|
| UI / Scene | state、layout、dirty intent、DrawCmd | display 总线、panel refresh、物理 pixel transport |
| DrawCmd executor/canvas | command execution、pixel conversion/composition、span output | 产品 navigation、backend device policy |
| RenderBackend | frame lifetime、span 写入、dirty 接收、platform flush/present 接线 | widget truth、theme、layout、semantic action |
| Display policy | e-ink partial/full refresh、ghosting、device-specific pacing | 改写 Vivid UI 语义 |

`mark_dirty` 提交的是 dirty region，不自动定义 e-ink refresh decision。EInk policy 见
[`eink_refresh_policy.md`](eink_refresh_policy.md)。

## 新 Backend / Pixel Format 准入

新增 backend 或 1bit/2bit 等格式时必须同时证明：

- `PixelFormat`/traits、framebuffer stride 与 span byte contract 一致；
- executor/canvas 对目标格式有显式 conversion 或明确拒绝，不静默按 RGB 路径解释；
- FullFrame 与 Tile/PFB 消费相同 DrawCmd semantics；
- dirty clipping、bounds、unsupported command 和 overflow 行为可诊断；
- host fixture 只证明 backend semantics，真实 panel refresh/cache/timing 仍需对应环境证据。

颜色映射、dithering、alpha threshold 和 font/image downgrade 应由 format conversion 或 backend/display
policy 明确拥有，不能散落到 widget/page code。

## 非目标

- 不承诺尚未在 source 中实现的 1bit、2bit、GPU、DMA 或 e-ink backend；
- 不定义 backend 开发排期或推荐实现顺序；
- 不让 platform backend 反向拥有 Vivid state、layout 或 semantic model；
- 不用一个 host framebuffer 结果替代真实 display evidence。
