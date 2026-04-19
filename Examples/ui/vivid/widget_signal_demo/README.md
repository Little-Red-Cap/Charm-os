# widget_signal_demo

这个示例用最小的 object-level widget 代码，冻结当前 Vivid 控件接入
`service::signal` 之后的 edge 语义。

它刻意不走 SoA `SceneAccess`，而是直接实例化控件对象，专门演示这几件事：

- `Button::observe_click()` 是对象级边沿事件观察面
- 旧 `set_on_click()` 兼容回调仍然保留，但它不是状态真相接口
- 被接受的 click 会同步广播 edge；未被接受的 click 不会偷偷触发

这个示例当前覆盖了：

- `Button`

其中最关键的语义点是：

- `set_text()` 之类的 programmatic 更新不会制造 click edge
- 命中的 click 会同时触发 `observe_click()` 与旧 `set_on_click()` 兼容回调
- `unobserve_click(token)` 之后 edge 观察者保持静默，但旧兼容回调仍按原语义工作
- disabled button 不接受 click，也不会制造隐藏 edge

如果你要看 object-level widget 的状态真相观察面，请继续看：

- `Examples/ui/vivid/widget_state_demo`

构建：

```bash
cmake -S Examples/ui/vivid/widget_signal_demo -B Examples/ui/vivid/widget_signal_demo/build -G Ninja
cmake --build Examples/ui/vivid/widget_signal_demo/build
```
