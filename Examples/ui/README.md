# UI 示例入口

本目录收纳 Vivid UI 相关的主机侧验证样例，重点覆盖 object-level widget、SceneAccess、主题、导航、fullframe/tile 渲染和移植模板。

如果你还没先看 UI 文档，建议先回到：

- [`../../docs/ui/README.md`](../../docs/ui/README.md)
- [`../cmake/README.md`](../cmake/README.md)

## 按任务进入

### 我想先看当前语义冻结点

先读：

- [`vivid/widget_state_demo/README.md`](vivid/widget_state_demo/README.md)
- [`vivid/scene_state_demo/README.md`](vivid/scene_state_demo/README.md)

### 我想看交互、布局与渲染路径

先看：

- [`vivid/fullframe_demo/`](vivid/fullframe_demo/)
- [`vivid/nav_demo/`](vivid/nav_demo/)
- [`vivid/text_demo/`](vivid/text_demo/)
- [`vivid/theme_demo/`](vivid/theme_demo/)
- [`vivid/tile_demo/`](vivid/tile_demo/)
- [`vivid/soa_demo/`](vivid/soa_demo/)

### 我想看移植模板与资源

先看：

- [`vivid/port_template/README.txt`](vivid/port_template/README.txt)
- [`vivid/assets/`](vivid/assets/)

## 使用提醒

- 大多数 Vivid 示例依赖 SDL3 与 [`../cmake/ExampleTemplate.cmake`](../cmake/ExampleTemplate.cmake)。
- 这里偏宿主侧 UI 验证，不直接替代最终产品化页面或板级渲染接线。
