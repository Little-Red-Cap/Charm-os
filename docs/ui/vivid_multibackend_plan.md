# Vivid 多后端规划（最小落地）

目标：保持 UI 层不变，仅通过 **Executor/Backend** 支持多像素格式与显示设备。

## 1. 最小接口清单（不可变）

`RenderBackend` 仍只提供最小像素输出能力：

- `width()/height()`
- `begin_frame()/end_frame()`
- `blit_span(x, y, src, bytes)`
- `mark_dirty(x, y, w, h)`

`FrameBufferView` 继续提供：

- `format / stride / width / height / data`

`DrawCmdExecutor` 仅消费 DrawCmd，不感知后端类型。

## 2. 不可变边界（UI 不可知）

- UI 只输出 `DrawCmd`（语义色/文本/图片），不感知像素格式。
- UI 不负责色彩映射、不负责抖动策略。
- UI 不感知 e‑ink 局部刷新策略。

## 3. 后端能力矩阵（后续定义）

- 像素格式：RGB565 / RGB888 / ARGB8888 / 1bit / 2bit
- 颜色映射：semantic → palette
- 抖动：无 / ordered / error diffusion
- 透明度：硬阈值 / coverage map / ignore
- 字体：4bpp → 1bit/2bit 适配
- 图像：缩放/九宫格 fallback 策略

## 4. 需要提前拍板的问题

1) 1bit/2bit 是否允许 alpha？
   - 若否：alpha 由 executor 侧阈值/抖动处理。
2) 颜色映射放在哪一层？
   - 建议：backend 侧（保持 DrawCmd 语义不变）。
3) 是否需要 pattern brush（抖动纹理）？
4) e‑ink refresh policy 是否作为 backend 选项？

## 5. 推荐落地顺序

1) 1bit BW backend（tile 路径优先）
2) 2bit gray backend（调色板 + 抖动）
3) e‑ink backend（局部刷新 + policy）

