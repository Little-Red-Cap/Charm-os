# widget_state_demo

这个示例用最小的 object-level widget 代码，冻结当前 Vivid 控件接入
`service::state` 之后的 observe 语义。

如果你要先看 object-level 的边沿事件表面，请先看：

- `Examples/ui/vivid/widget_signal_demo`

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

示例 stdout 遵守 `docs/ui/vivid_evidence_stdout_law.md`：统一为 `[wst] run=widget_state_demo phase=begin/end` 与 `[wst] case=...` 的 summary 形式，并由 CTest 约束最终 `result=ok cases=5`。

```bash
cmake -S Examples/ui/vivid/widget_state_demo -B cmake-build-vivid-widget-state-demo-codex -G Ninja
cmake --build cmake-build-vivid-widget-state-demo-codex -j 22
ctest --test-dir cmake-build-vivid-widget-state-demo-codex --output-on-failure
cmake-build-vivid-widget-state-demo-codex/vivid-widget-state-demo
```
