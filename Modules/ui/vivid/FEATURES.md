# Charm Vivid 功能清单与优先级（草案）

说明：
- P0：短期必须补齐的基础能力
- P1：中期重点组件/能力
- P2：后续增强与扩展

## 1. 基础渲染与资源
- P0 渲染原语：线/矩形/圆/圆角矩形/图片/九宫格
- P0 脏矩形链路：标记 -> 合并 -> 分区刷新
- P0 分块渲染（tile）与可控遍历
- P0 像素格式：RGB565/RGB888/ARGB8888
- P1 渲染合批（同材质/同纹理）
- P1 GPU/DMA 适配层（可选）
- P2 2D 加速器专用后端

## 2. 字体与文本
- P0 UTF-8 解码基础
- P0 4bpp 字体渲染（默认）
- P0 文本测量：宽度/高度/基线
- P1 自动换行/省略号
- P1 多字体回退
- P2 复杂脚本 shaping

## 3. 事件与输入
- P0 鼠标/触摸：按下/抬起/移动/滚轮
- P0 点击/拖拽/长按
- P0 焦点与键盘导航
- P1 手势（滑动/捏合）
- P2 多指/压感

## 4. 布局与容器
- P0 基础布局：Flex/Anchor
- P0 容器：ScrollContainer、PopupLayer
- P1 栅格/流式布局
- P1 约束布局（Constraint）
- P2 虚拟列表布局

## 5. 基础控件（当前已有）
- P0 button / label / image / checkbox / switch
- P0 radio / radio_group
- P0 slider / dial / roller / dropdown
- P0 list / menu / menu_item
- P0 list_view（基础虚拟化入口）
- P0 progress / spinner / bar / gauge / arc
- P0 text / text_area
- P0 message_box / tabview / scroll_container / popup_layer

## 6. 需要补齐的基础控件
- P1 输入：text_input（单行）、number_input
- P1 选择：segmented_control、toggle_group
- P1 滚动条独立控件（scrollbar）

## 7. 高级组件
- P1 表格/表头（table）
- P1 树形列表（tree）
- P1 进度与状态：stepper、timeline
- P2 图表扩展（多曲线、交互）
- P2 代码编辑/富文本

## 8. 主题与样式
- P0 主题结构化（Theme/Style）
- P1 样式继承与局部覆盖
- P1 主题加载（配置/资源）
- P2 运行时 DSL/CSS

## 9. 动画与过渡
- P1 简单时间轴 + easing
- P1 属性插值（alpha/位置/尺寸）
- P2 复杂转场与滤镜

## 10. 诊断与可观测性
- P0 trace/日志/计数器（已接入）
- P1 性能 overlay（FPS/脏区/绘制时间）
- P2 可回放的渲染/输入记录
