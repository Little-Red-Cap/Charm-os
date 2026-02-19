# Charm Vivid 架构文档（工作版）

## 1. 定位与目标
- 定位：资源受限平台上的“富 UI”实现，强调可组合控件与可维护渲染管线。
- 目标：零动态分配、可预测性能、可渐进演进的 UI 系统。
- 约束：不依赖异常与 RTTI，默认 4bpp 字体灰度。

## 2. 目录结构与模块分层
```
Modules/ui/vivid/
  core/      # GUI 核心：对象树、事件、布局、工厂、样式
  gfx/       # 渲染基础：像素格式、画布、framebuffer、图像
  font/      # 字体与排版：Font/typography + lvgl 转换
  widgets/   # 具体控件集合
  gfx/assets # 资源注册与示例资源
```

### 2.1 core
- `object.cppm`：ObjectBase（Rect/State/children/anchor/flex）
- `handle.cppm`：WidgetHandle（kind/index/generation）
- `factory.cppm`：UiFactory（对象池创建、树连接、销毁）
- `gui.cppm`：Gui（事件派发、焦点、渲染递归）
- `layout.cppm`：Flex/Anchor 布局
- `style.cppm`：Style/Theme 与颜色解析
- `event.cppm`：Event 类型与输入结果

### 2.2 gfx
- `canvas.cppm`：Canvas 封装 framebuffer 与裁剪
- `render.cppm`：绘制原语（线/矩形/圆/图片/9-slice）
- `pixel_format.cppm`：RGB565/RGB888/ARGB8888
- `image.cppm`：ImageView 与视图构造
- `framebuffer.cppm`：像素缓冲承载

### 2.3 font
- `font.cppm`：Glyph/Font/查找与 kern
- `typography.cppm`：FontId 与字体选择
- `font_12.cppm`：示例 1bpp 字体
- `lvgl/`：LVGL 字体转换与样本（4bpp）

### 2.4 widgets（现有控件清单）
- 基础：button、label、image、checkbox、switch、radio/radio_group
- 输入：slider、dial、roller、dropdown
- 布局/容器：list、menu/menu_item、tabview、scroll_container、popup_layer
- 显示：progress、spinner、bar、gauge、arc、chart
- 文本：text、text_area
- 其它：message_box、primitives_canvas、list_view、scrollbar、text_input、number_input、segmented_control、toggle_group、table_view、tree_view

## 3. 核心数据结构
### 3.1 WidgetHandle
- 由 `(kind, index, generation)` 组成。
- UiFactory 通过 generation 防止悬空引用。

### 3.2 ObjectBase
- 几何：Rect/位置/尺寸
- 状态：Hovered/Pressed/Focused/Disabled
- 结构：parent/children
- 布局：flex/anchor/percent/min/max/align

### 3.3 Style/Theme
- Style 是控件样式容器（bg/border/font）。
- Theme 模板化保存每个控件的 Style。

## 4. 渲染管线
1. Gui::render 触发树遍历与布局应用
2. Layout：优先 flex，其次 anchor
3. 控件 draw 使用 gfx::render 原语绘制
4. 文本渲染基于 `Glyph` + `bpp` 灰度混合

当前绘制为逐像素绘制，不含合批或脏矩形分区渲染。

## 5. 事件与输入
### 5.1 Event 结构
- MouseDown/Up/Move/Wheel
- Click/DragStart/DragMove/DragEnd
- KeyDown/KeyUp

### 5.2 Gui 事件派发
- Hover/Pressed/Captured/Focused 状态机
- overlay 优先派发（弹层）
- 拖拽阈值 + 事件转换

## 6. 字体与文本
### 6.1 默认策略
- Vivid 默认 4bpp 字体灰度渲染（兼顾质量与成本）。
- 现存 `font_12` 为 1bpp，可保留作为 Mono。

### 6.2 文本渲染
- UTF-8 解码（基础）
- Glyph 查找与 kern
- bpp = 1/2/4/8 灰度混合

## 7. 资源与资产
- `gfx/assets/registry.cppm`：资源注册表
- `gfx/assets/render_images.cppm`：渲染示例
- `gfx/assets/benchmark_images.cppm`：性能样本

## 8. 当前缺口与风险
### 8.1 渲染与性能
- 缺少脏矩形分区渲染的完整闭环
- Canvas 逐像素绘制成本高，需块渲染与批处理
- GPU/DMA 适配层缺失

### 8.2 文本与排版
- 无复杂 shaping（阿拉伯/印度语等）
- 断行/测量能力偏基础

### 8.3 样式系统
- 缺少样式继承/主题加载
- 运行时 CSS/DSL 尚未支持

### 8.4 组件生态
- 高级组件与状态管理较少
- 动画/过渡系统缺失

## 9. 演进建议（优先级）

1. 字体流水线稳定：4bpp 作为默认输出
2. 渲染优化：脏矩形 + 轻量缓存
3. 样式系统：主题配置文件与运行时切换
4. 高级控件：表格/树/虚拟列表
5. 动画系统：简单时间轴 + easing

## 10. Reference Migration Notes (SGL/LVGL/ARM-2D)

- SGL: small widgets (led/msgbox/keyboard) are good fits for Vivid/Ink migration.
- LVGL: style/layout rules map cleanly to Vivid Theme/Style; keep layout logic in core.
- ARM-2D: favor tile/dirty-rect thinking; keep render primitives stateless and cache-friendly.

