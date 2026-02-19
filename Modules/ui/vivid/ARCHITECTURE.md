# Charm Vivid 架构说明

本文件用于描述 Vivid 的当前架构、边界与主要模块，便于后续补齐能力与迁移控件。

## 1. 分层结构

- core：数据结构、主题/样式、诊断/trace、配置等基础设施。
- gfx：渲染 API 与几何/像素格式抽象。
- widgets：控件与容器实现，尽量保持薄封装，不重复基础设施。
- font：字体数据与生成脚本产物（4bpp 为默认）。

## 2. 渲染与更新

- 渲染入口由 UI 主循环驱动，控件在更新阶段提交绘制。
- 使用脏矩形/脏区域机制减少刷新面积，保持与底层驱动解耦。
- 绘制 API 统一走 gfx 层，避免控件直接绑定平台细节。

## 3. 布局与容器

- 基础布局能力为 Anchor/Flex/Flow/Grid，容器负责子节点的布局与裁剪。
- ScrollContainer/ScrollBar 负责滚动与可视区域同步。
- ListView 支持虚拟化与固定行缓存槽位复用，提升滚动性能。

## 4. 输入与事件

- 输入链路在 UI 层进行语义化处理，控件只关注高层意图。
- 焦点与键盘导航由通用逻辑维护，控件实现自身行为。
- 手势事件提供 Swipe/Pinch 的接口占位，按需由控件接入。

## 5. 文本与字体

- 文本渲染默认 4bpp 字体数据。
- 支持 UTF-8 解码、测量、换行与截断，渲染与排版逻辑集中在 text 组件。
- 字体数据由 `font/font_builder.py` 生成，输出模块化字体数据。
- RichText/CodeBlock 走独立控件，避免复杂样式侵入基础文本。

## 6. 主题与样式

- 主题定义在 core/style 中，通过 `Theme::inherit` 与 `StylePatch` 支持局部覆盖。
- 提供 `ThemePreset` 作为配置入口，便于集中加载主题。
- 控件以 theme token 作为样式入口，避免散落硬编码。

## 7. 诊断与可观测性

- 统一接入 trace_core 做机器可读事件输出。
- 日志统一通过 out.logger。

## 8. 示例与验证

- 示例工程用于验证控件行为与性能路径，避免独立测试与真实场景脱节。
- 当前示例含 ListView/ScrollBar/TableView/TreeView、Stepper/Timeline、MenuTree、RichText/CodeBlock、Image 变换等最小配置。
