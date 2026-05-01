# Vivid PageLayer 与 StylePatch 使用指南

本说明描述两项“轻量但高收益”的 UI 能力：
1) **StylePatch**：按控件局部覆盖样式（不改全局 Theme/StyleSheet）。  
2) **PageLayer**：页面容器 + 显式 show/hide + 进入/退出 hook。

---

## 1. StylePatch（控件级样式覆盖）

适用场景：
- 某个按钮/卡片需要单独改圆角、填充、描边。
- 局部视觉强化（例如主播放按钮、列表卡片）。

### 最小用法（SceneBuilder）

```cpp
StylePatch patch{};
patch.has_bg_color = true;
patch.bg_color = kUiButtonBg;
patch.has_border_color = true;
patch.border_color = kUiButtonBorder;
patch.has_corner_radius = true;
patch.corner_radius = 12;
builder.set_style_patch(handle, patch);
```

### 运行期覆盖（SceneAccess）

```cpp
access.set_style_patch(handle, patch);
```

### 清除覆盖

```cpp
builder.clear_style_patch(handle);
// 或
access.clear_style_patch(handle);
```

### 规则说明

- StylePatch 在 StyleSheet 结果上**叠加**，仅影响当前控件。
- 有状态字段会按当前状态覆盖：
  - `bg_hover/bg_pressed/bg_disabled`
  - `border_hover/border_pressed/border_disabled`
  - `font_color_disabled`
  - `accent_*`
- metrics（圆角/边框/内边距/字体）会直接覆盖。

---

## 2. PageLayer（页面容器）

说明：这里的 PageLayer 指页面容器与 show/hide 收口，不等同于
[`vivid_layer_runtime_v0.md`](vivid_layer_runtime_v0.md) 中的 frozen surface / snapshot runtime。

适用场景：
- 页面切换时统一刷新 UI。
- 避免散落的 set_visible/状态恢复逻辑。

### 最小用法

```cpp
// 初始化阶段
page_layer.set_root(page_root);
page_layer.set_hooks(::ui::scene::PageHooks{
    .on_show = &on_show_fn,
    .on_hide = &on_hide_fn,
    .ctx = this,
});
page_layer.set_visible(access, false);

// 切页
page_layer.show(access);
page_layer.hide(access);
```

### Hook 形态

```cpp
static void on_show_fn(::ui::scene::SceneAccess& access,
                       WidgetHandle root,
                       void* ctx) noexcept {
    (void)access;
    (void)root;
    auto* self = static_cast<MyController*>(ctx);
    if (!self) return;
    self->refresh_page();
}
```

### 行为说明

- `show/hide` 会调用 `set_visible(root, on)`。
- hook 只在 visible 真正变化时触发（避免重复调用）。

---

## 3. 推荐落地顺序

1) 先用 PageLayer 收口页面切换逻辑。  
2) 再用 StylePatch 精修按钮/卡片层级感。  

这两项在 MCU/PC 路径均通用，不依赖平台特性。
