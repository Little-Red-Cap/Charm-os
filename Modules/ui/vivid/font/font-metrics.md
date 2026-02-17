# Font Metrics Contract（不可破坏的字体度量契约）

> 本文定义 Charm GUI 中 **Font / Glyph 的几何语义与数学不变量**。
>
> 该契约同时约束：
>
> * 字体生成器（Python / 工具链）
> * 字体数据结构（C++ Font / Glyph）
> * 文本排版与渲染逻辑（Label / Text / Layout）
>
> **任何一方不得私自重新解释这些字段的含义。**

---

## 1. 设计目标

* 明确字体排版所依赖的几何模型
* 消除“看起来能用但语义错误”的隐患
* 保证不同字体、不同字符之间可以稳定组合
* 支撑未来的：多字体 fallback、多行文本、对齐、裁剪、缩放

---

## 2. 坐标系与基本概念

### 2.1 坐标系约定

* 屏幕坐标：

  * 原点在左上角 `(0,0)`
  * x 向右增长
  * y 向下增长

* 文本排版采用 **基线（baseline）模型**

---

### 2.2 Baseline（基线）

**Baseline 是字体系统中唯一的垂直参考线。**

* 所有字符都必须能够对齐到同一条 baseline
* Baseline 不随字符变化
* Baseline 由 Font 级别定义

```text
baseline_y = text_top + font.baseline
```

---

## 3. Font 级别契约（全局不变量）

### 3.1 Font.baseline

```cpp
int Font::baseline;
```

语义：

> 字体基线距离“文本行顶部”的像素距离

约束：

```text
0 <= baseline <= line_height
```

---

### 3.2 Font.line_height

```cpp
int Font::line_height;
```

语义：

> 一行文本所占用的完整垂直空间

* 包含 ascent + descent
* 用于：

  * Label / Button 的高度计算
  * 多行文本的行距

---

## 4. Glyph 级别契约（核心）

```cpp
struct Glyph {
    const uint8_t* bitmap;
    int width, height;

    int x_advance;
    int x_offset;
    int y_offset;
};
```

### 4.1 Glyph.bitmap / width / height

语义：

* 紧包（tight）位图
* 位图左上角视为 `(0,0)`
* **不携带任何排版语义**

> 位图只负责“画什么”，不负责“画在哪里”

---

### 4.2 Glyph.x_advance（水平推进量）

```cpp
int Glyph::x_advance;
```

语义：

> 当前 glyph 绘制完成后，光标在 x 方向推进的距离

约束：

```text
x_advance > 0
```

规则：

```text
pen_x += x_advance
```

* 唯一影响文本宽度的字段
* bitmap.width 不参与排版

---

### 4.3 Glyph.x_offset（Left Bearing）

```cpp
int Glyph::x_offset;
```

语义：

> glyph 位图左边缘相对 pen_x 的偏移量

渲染公式：

```text
render_x = pen_x + x_offset
```

---

### 4.4 Glyph.y_offset（最关键）

```cpp
int Glyph::y_offset;
```

**语义定义（不可更改）：**

> glyph 位图【顶部】到字体 baseline 的垂直距离（像素）

渲染公式：

```text
render_y = baseline_y - y_offset
```

约束：

```text
0 <= y_offset <= font.line_height
```

---

## 5. 标准文本渲染公式（唯一合法方式）

```cpp
int pen_x = start_x;
int baseline_y = text_top + font.baseline;

for (glyph : text) {
    render_x = pen_x + glyph.x_offset;
    render_y = baseline_y - glyph.y_offset;

    draw_bitmap(render_x, render_y, glyph.bitmap);

    pen_x += glyph.x_advance;
}
```

任何偏离该公式的实现都视为 **违反契约**。

---

## 6. Python 字体生成器契约

### 6.1 baseline 计算

```python
ascent, descent = font.getmetrics()
baseline = ascent
line_height = ascent + descent
```

---

### 6.2 y_offset 计算（强制）

```python
bbox = font.getbbox(ch)
# CONTRACT: y_offset = distance from glyph top to baseline
y_offset = baseline - bbox[1]
```

---

### 6.3 advance 计算

```python
advance = int(round(font.getlength(ch)))
```

---

### 6.4 生成阶段校验（建议）

```python
assert 0 <= y_offset <= line_height
assert advance > 0
```

---

## 7. C++ 侧契约校验（推荐）

```cpp
constexpr bool validate_font(const Font& f) {
    if (f.baseline < 0 || f.baseline > f.line_height)
        return false;

    for (const auto& g : f.table) {
        if (g.y_offset < 0 || g.y_offset > f.line_height)
            return false;
        if (g.x_advance <= 0)
            return false;
    }
    return true;
}

static_assert(validate_font(font_12));
```

---

## 8. 契约的意义

* UI / Layout 层无需理解字体细节
* Font 数据可被安全复用、组合、fallback
* 多字体、多行、多对齐逻辑可自然扩展
* 字体生成器与渲染引擎完全解耦

> **只要契约成立，字体系统就永远不会“看起来差不多但其实是错的”。**

---

## 9. 结语

这份契约定义的不是“实现方式”，而是 **字体排版的几何学基础**。

任何未来功能（缩放、DPI、富文本、emoji、icon font）
都必须建立在本契约之上，而不是修改它。
