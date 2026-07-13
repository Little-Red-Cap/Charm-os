# Font Metrics Contract

## 文档状态

- `status`: `contract`
- `scope`: `Font` / `Glyph` 几何语义、生成与渲染不变量
- `authority`: [`charm.font`](../../../gfx/font/font.cppm)、
  [`font_builder.py`](font_builder.py)、[`text_box.cppm`](../gfx/text_box.cppm)

生成器、字体数据和 renderer 必须使用同一 baseline model。bitmap 只描述 coverage，不能自行携带另一套
layout 语义。

## 坐标与行模型

屏幕原点在左上，x 向右、y 向下。文本行以统一 baseline 定位：

```text
baseline_y = text_top + font.baseline
```

`Font::baseline` 是行顶部到 baseline 的距离；`Font::line_height` 是完整行高。基本约束：

```text
0 <= baseline <= line_height
```

换行推进使用 line height。不同 glyph 不得改变该行 baseline；fallback glyph 的 `y_offset` 仍相对同一
baseline 解释。

## Glyph 几何

| field | contract |
|---|---|
| `bitmap`、`width`、`height` | tight coverage bitmap；左上角是 bitmap `(0,0)` |
| `bpp` | coverage packing；允许值由 `validate_font()` 冻结 |
| `x_advance` | glyph 完成后的 pen 推进量 |
| `x_offset` | bitmap 左边缘相对当前 pen 的偏移 |
| `y_offset` | bitmap 顶部到 baseline 的距离，正值向上 |

核心公式：

```text
render_x = pen_x + glyph.x_offset
render_y = baseline_y - glyph.y_offset
pen_x   += glyph.x_advance
```

`bitmap.width` 不替代 advance。kerning 只在相邻 glyph 由同一 resolved font 提供时，在当前 pen 上增加
该 font 的 kerning adjustment；它不改变 glyph 字段语义。

## 数据准入

`validate_font()` 至少保证：

- baseline 位于 `[0, line_height]`；
- glyph `y_offset` 位于 `[0, line_height]`；
- 非空 glyph 必须有正 advance；空 bitmap glyph 可以不推进；
- bpp 属于 renderer 支持集合；
- fallback glyph 指向当前 table；
- sparse code/id 表长度一致。

生成字体模块必须在编译期调用 validator。具体 table、range、fallback、sparse 和 kerning 字段以
`charm.font` 为准，本文不复制 struct layout。

## Builder 约束

当前 Pillow builder 使用：

```text
ascent, descent = getmetrics()
baseline        = ascent
line_height     = ascent + abs(descent)
x_offset        = bbox.left
y_offset        = baseline - bbox.top
x_advance       = round(getlength(character))
```

如果 font API 不提供 metrics/advance，fallback 结果仍必须通过 `validate_font()`；warning 不能替代验证。
生成器的 packing、gamma、range 和 fallback 选项属于工具实现，不进入几何契约。

## Renderer 约束

renderer 必须：

- 先 resolve glyph/fallback font，再应用同 font kerning；
- 按上述公式定位 bitmap；
- 按 glyph bpp 解码 coverage，并遵守 clip；
- 使用 advance 推进 pen，不按 bitmap width 猜测；
- measurement、layout 和 raster path 对 newline/fallback/kerning 保持同一几何解释。

## 非目标

- 不定义字体选择、package/provider 生命周期或产品 typography token；
- 不承诺所有字体共享同一 line height；
- 不把 Python/Pillow API 形状冻结为 runtime ABI；
- 不以“视觉上差不多”替代 validator、measurement 与 render 一致性证据。
