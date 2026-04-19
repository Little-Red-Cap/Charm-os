# Vivid Widget State/Observe 边界

这份说明只回答一个问题：

> Vivid 里的 object-level widget `observe_*`，和 SoA `SceneAccess` / `SceneBuilder` 到底是什么关系？

短答案：

- 不是同一层
- 不该自动镜像
- 不该混用语义

## 1. 两层表面

### 1.1 object-level widget 表面

典型 API：

- `Checkbox::observe_checked()`
- `Dropdown::observe_selected()`
- `Slider::observe_value()`
- `ProgressBarSimple::observe_value()`
- `Arc::observe_value()`

这层的本质是：

- widget 内部真相单元由 `service::state<T, N>` 承担
- `observe_*` 是对象级、同执行域、同步观察面
- 旧 `set_on_change()` 之类接口如果还存在，只是兼容面，不再自动等价于“所有真相变化”

适合场景：

- 直接实例化 widget 对象的小系统
- object-level smoke / demo
- 局部 widget 组合验证

### 1.2 SoA `SceneBuilder / SceneAccess` 表面

典型 API：

- `builder.set_value(handle, value)`
- `builder.set_checked(handle, on)`
- `access.set_value(handle, value)`
- `access.set_checked(handle, on)`

这层的本质是：

- 句柄驱动的 scene/runtime 更新入口
- 服务于 SoA kernel、布局、输入、record/execute 边界
- 关注的是 scene graph / handle / runtime 行为，不是对象级 widget 连接语义

适合场景：

- SoA 页面构建
- 运行期 page/controller 更新
- 句柄化 UI 状态推进

### 1.3 SoA-backed helper 表面

典型 API：

- `DropdownPopup::observe_selected()`
- `DropdownPopup::observe_select()`
- `MenuTree::set_selection_model(...)`
- `MenuTree::observe_select()`

这一层的本质是：

- helper 自己可以把 committed truth 和 confirm edge 拆开
- helper 也可以把 highlight truth 外置给 caller，再只保留 confirm edge
- 它可以依赖 `SoaFactory / SoaKernel` 落地，但这仍然是 helper 级本地语义
- 它不是 `SceneAccess` 的自动镜像，更不是整个 scene/runtime 的统一观察总线

适合场景：

- popup / menu / picker 这类局部交互 helper
- 需要在 SoA-backed 组件里显式拆开 truth 与 edge 的地方
- helper 级 smoke / contract 冻结

## 2. 硬边界

### 2.1 不要把 `SceneAccess` 当成 `observe_*` 镜像

`SceneAccess` 当前提供的是：

- `set_value`
- `set_checked`
- `set_text`
- `set_style_patch`

它不是 object-level `observe_*` 的镜像层。

因此：

- 不要因为 API 对称性，要求 `SceneAccess` 也自动暴露 `observe_value()`。
- 不要把对象级 `signal/state` 语义硬塞进 SoA kernel 更新面。

### 2.2 不要假设两层的兼容回调语义相同

object-level widget 里，旧回调和真相观察面已经被显式拆开。

例如：

- `Checkbox::set_checked()` 会更新真相，但不会自动触发旧 `on_change`
- `Dropdown::set_selected()` 的旧 `on_change` 仍然是命令式兼容面
- `Slider::set_range()` 触发的 clamp 会改变真相，但不会补发旧回调

这些规则成立于 object-level widget 表面。

走 SoA `SceneAccess` 时，应该把它看作：

- runtime 状态写入口

而不是：

- 自动携带 object-level 兼容回调语义的代理层

### 2.3 SoA 跨控件关系应留在 controller / app-state

如果你在 SoA 页面里想表达：

- checkbox 改变后驱动 progress
- dropdown 选择后驱动 slider
- 多个值共同推导一个展示控件

推荐把关系显式写在：

- page controller
- app-state
- 明确的页面逻辑层

而不是偷藏在：

- kernel 内部
- `SceneAccess` 的隐式联动
- “设置一个 handle 顺便帮我通知所有相关控件”的魔法接口

### 2.4 不要把 helper 级 observe 误判成 `SceneAccess` 镜像

像 `DropdownPopup` 这样的 SoA-backed helper，可以自己定义：

- committed truth：`observe_selected()`
- confirm edge：`observe_select()`

但这说明的是：

- helper 内部语义已经被拆清楚

而不是：

- `SceneAccess` 从此也应该拥有同名 `observe_*`
- SoA runtime 会自动帮你广播所有 helper 边沿
- helper 级 callback/signal 语义会自动扩散成系统级 wiring

`MenuTree` 这类 helper 则更进一步：

- 高亮 truth 直接交给外部 `StructuredMenuSelectionModel`
- helper 自己只显式补 `observe_select()` 这类 confirm edge

这同样说明的是 helper contract 被写清楚了，而不是 `SceneAccess` 变成了 observe 总线。

## 3. 当前推荐做法

### 3.1 做 object-level 语义验证

优先看：

- `Examples/ui/vivid/widget_state_demo`

这个示例专门演示：

- `observe_*` 才是真相观察面
- 旧兼容回调仍然保留，但语义已被收紧
- 纯展示控件只保留 observe 面

### 3.2 做 SoA 页面开发

优先看：

- `Examples/ui/vivid/scene_state_demo`
- `charm.ui.scene`
- `SceneBuilder`
- `SceneAccess`
- 页面 controller / app-state

这条线上应优先关注：

- `dispatch_event -> input_event -> controller/app-state -> SceneAccess` 这条显式链
- 句柄更新
- 页面 wiring
- 输入与布局边界

而不是把 SoA runtime 当成对象级 widget 事件系统的自动放大器。

### 3.3 做 SoA-backed helper 语义验证

优先看：

- `Examples/ui/vivid/dropdown_popup_demo`
- `Examples/ui/vivid/menu_tree_demo`

这个示例专门冻结：

- `set_selection()` 只改 committed truth
- 导航高亮不偷偷改 committed truth
- confirm 才触发 edge 与 legacy callback

`MenuTree` 示例则专门冻结：

- 外部 selection model 持有 highlight truth
- hover / 导航推进 truth，但不触发 confirm
- leaf confirm 才触发 `observe_select()`

## 4. 一句话规则

可以直接记成：

> object-level `observe_*` 负责“这个 widget 的真相如何被同步观察”；
> SoA helper 的 `observe_*` 负责“这个局部 helper 自己的 truth/edge 如何被拆清”；
> SoA `SceneAccess` 负责“这个 scene/runtime 里的句柄状态如何被显式推进”。

二者可以协作，但不是一层，也不应伪装成同一层。
