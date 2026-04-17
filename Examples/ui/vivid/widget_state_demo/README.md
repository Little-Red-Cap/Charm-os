# widget_state_demo

这个示例用最小的 object-level widget 代码，冻结当前 Vivid 控件接入
`service::state` 之后的 observe 语义。

它刻意不走 SoA `SceneAccess`，而是直接实例化控件对象，专门演示这几件事：

- `observe_checked()` / `observe_selected()` / `observe_value()` 是对象级真相观察面
- 旧 `set_on_change()` 兼容回调仍然保留，但不再等同于“所有状态变化”
- programmatic `set_*()` 和交互/命令式回调的边界是显式的

这个示例当前覆盖了：

- `Checkbox`
- `Dropdown`
- `Slider`
- `ProgressBarSimple`
- `Arc`

其中最关键的语义点是：

- `Checkbox::set_checked()` 会改变真相，但不会触发旧 `on_change`
- `Dropdown::set_selected()` 的旧 `on_change` 仍然是命令式兼容面
- `Slider::set_range()` 触发的 clamp 会更新真相，但不会补发旧回调
- 纯展示控件只暴露 observe 面，不额外引入命令式回调包袱

构建：

```bash
cmake -S Examples/ui/vivid/widget_state_demo -B Examples/ui/vivid/widget_state_demo/build -G Ninja
cmake --build Examples/ui/vivid/widget_state_demo/build
```
