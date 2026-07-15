# Vivid Widget State/Observe 边界

> status: `contract`

本文区分 object-level widget、SoA scene access 与 SoA-backed helper 的状态/观察职责。三者可以协作，
但不是同一层，也不能自动镜像。

## 三层表面

| 表面 | truth owner / 更新入口 | observe 语义 | 典型用途 |
|---|---|---|---|
| object-level widget | widget 内的 `service::state<T, N>`，或显式绑定的 caller-owned model | state surface 同执行域同步观察；caller model 由 owner 直接读取 | 小系统、object smoke、局部组合 |
| SoA scene | kernel/scene graph；`SceneBuilder`、`SceneAccess` 按 handle 更新 | 不提供 object observe 镜像 | page/controller、布局、input、record/execute |
| SoA-backed helper | helper 或 caller-owned model | helper-local committed truth / confirm edge | popup、menu、picker 等局部交互 |

object-level state 例子包括 `Checkbox::observe_checked()`、`Dropdown::observe_selected()`、
`SegmentedControl::observe_selected()` 和 `Slider::observe_value()`。`ToggleGroup::Item::checked` 则由调用方拥有，
object widget 直接写入同一模型并用 callback 表达命令边沿，不再复制一份隐式 truth。SoA scene 例子包括
`set_value(handle, ...)`、`set_checked(handle, ...)`、`set_text(handle, ...)` 与
`set_style_patch(handle, ...)`。

## 硬边界

### SceneAccess 不是 observe 总线

不要为了 API 对称给 `SceneAccess` 自动增加 `observe_value()`，也不要把 object-level signal/state 语义
塞进 kernel handle update surface。SoA scene 的职责是显式推进 runtime truth，不是代理每个 widget 的
本地连接。

### Truth 与 legacy callback 分离

object-level setter 更新 truth，不保证触发旧命令式 callback。例如 programmatic set 或 range clamp 可以
改变 truth，但不应伪造 user-action callback。consumer 需要完整 truth observation 时使用 `observe_*`；需要
兼容 action edge 时使用明确的 callback/edge surface。

这条规则只约束 object-level widget。SoA `SceneAccess` 更新不能自动继承 object callback 语义。

### 跨控件关系属于 controller/app-state

checkbox 驱动 progress、dropdown 驱动 slider、多个值推导展示状态等关系，应显式写在 page controller 或
app-state：

```text
dispatch_event
  -> input_event
  -> controller/app-state
  -> SceneAccess update
```

不得把跨控件联动隐藏在 kernel、SceneAccess side effect 或“设置一个 handle 顺便广播”的魔法接口中。

### Helper edge 只说明 helper contract

SoA-backed helper 可以区分：

```text
committed truth -> observe_selected()
confirm edge    -> typed callback or explicit observer surface
```

这不意味着 scene/runtime 自动广播 helper edge。`DropdownPopup` 自己持有 committed selection，并通过
`observe_selected()` 暴露 truth change；confirm 只使用一个带 committed index 的 typed callback。需要多播时由
caller 显式转发到自己的 signal。
`MenuTree` 把 highlight truth 交给 caller-owned selection model，只保留一个带 `menu_item_ref` 的 confirm callback；
需要多播时由 caller 显式转发到自己的 signal。disabled item 是否能 highlight、open 或 confirm 由 helper contract
明确定义，不能从 SceneAccess 推导。

## 证据入口

| 入口 | 证明内容 |
|---|---|
| `Examples/ui/vivid/widget_state_demo` | object truth observation 与 legacy callback 分离 |
| `Examples/ui/vivid/scene_state_demo` | handle-driven scene update 与 controller wiring |
| `Examples/ui/vivid/dropdown_popup_demo` | committed selection 与 confirm edge |
| `Examples/ui/vivid/menu_tree_demo` | caller-owned highlight truth、disabled boundary 与 confirm edge |

具体 API 列表、fixture 和 stdout 由当前源码与测试定义。维护时只需保持三层 ownership、truth/edge 分离和
显式 controller wiring，不在本文复制实现清单。
