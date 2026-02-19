# Vivid 架构演进草案（Layout Assistant / CachePolicy）

本文件记录可落地的最小演进方案，避免大改与空转。

## 1. Layout Assistant 原型（最小接口）

目标：让控件只关心“区域 + 尺寸 + 间距”，避免手写坐标计算。

### 1.1 基础概念

- 输入：目标区域 Rect，元素尺寸（固定/可变），间距/边距
- 输出：一组 Rect（按顺序迭代）
- 不依赖控件树，可独立用于渲染/布局

### 1.2 API 草案

```cpp
struct LayoutCursor {
    Rect region{};
    int gap{0};
    int line_gap{0};
    int cursor_x{0};
    int cursor_y{0};
    int line_h{0};
};

// 初始化
LayoutCursor layout_begin(Rect region, int gap, int line_gap);

// 线性流：横向排列
bool layout_next_h(LayoutCursor& c, int w, int h, Rect& out);

// 线性流：纵向排列
bool layout_next_v(LayoutCursor& c, int w, int h, Rect& out);

// 换行流：超宽自动换行
bool layout_next_wrap(LayoutCursor& c, int w, int h, Rect& out);
```

### 1.3 最小实现建议

- 基于 `core/layout` 实现，无需改动控件类
- 先以 Header-only 形式放在 `Modules/ui/vivid/core/layout_assistant.cppm`
- 以 “流式布局 / 换行布局” 作为第一阶段

## 2. CachePolicy 草案（渲染缓存策略）

目标：让控件/容器声明是否可缓存，由渲染阶段统一决策。

### 2.1 API 草案

```cpp
enum class CachePolicy : std::uint8_t {
    None,          // 不缓存
    Auto,          // 由渲染器决定
    Force,         // 强制缓存
};

class ObjectBase {
public:
    void set_cache_policy(CachePolicy p) noexcept;
    CachePolicy cache_policy() const noexcept;
};
```

### 2.2 渲染侧策略

- 规则示例：
  - 透明度 < 255 或变换时优先缓存
  - 子树过大时避免缓存（上限大小）
  - 多次重绘的静态控件优先缓存

### 2.3 可配置项

- `cache_max_bytes`
- `cache_chunk_size`
- `cache_min_area`
- `cache_debug_overlay`

## 3. 最小 PoC 顺序

1) 增加 Layout Assistant API（不改控件，只提供工具）
2) 增加 CachePolicy 字段（不改变渲染行为）
3) 在 Gui::render 中加入 CachePolicy 处理分支（仅 1~2 个控件试点）

## 4. 风险与回退

- Layout Assistant 不影响现有布局引擎，可与 LayoutSpec 并存
- CachePolicy 先以“统计 + trace”方式观察，再启用缓存行为
