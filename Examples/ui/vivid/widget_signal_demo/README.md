# widget_signal_demo

这个示例用最小的 object-level widget 代码，冻结当前 Vivid 控件接入
`service::signal` 之后的 edge 语义。
它刻意不走 SoA `SceneAccess`，而是直接实例化控件对象，专门演示这几件事：

- `Button::observe_click()` / `MenuItem::observe_click()` / `ListItem::observe_click()` 是对象级边沿事件观察面
- 旧 `set_on_click()` 兼容回调仍然保留，但它不等同于统一 truth/edge 模型
- 被接受的 click 会同步广播 edge；未被接受的 click 不会偷偷触发

这个示例当前覆盖了：

- `Button`
- `MenuItem`
- `ListItem`

其中最关键的语义点是：

- `set_text()` 这类 programmatic 更新不会制造 click edge
- 命中的 click 会同时触发 `observe_click()` 与旧 `set_on_click()` 兼容回调
- `unobserve_click(token)` 之后 edge 观察者保持静默，但旧兼容回调仍按原语义工作
- disabled widget 不接受 click，也不会制造隐藏 edge

如果你要看 object-level widget 的状态真相观察面，请继续看：

- `Examples/ui/vivid/widget_state_demo`

示例 stdout 遵守 `docs/ui/vivid_evidence_stdout_law.md`：统一为 `[ws] run=widget_signal_demo phase=begin/end` 与 `[ws] case=...` 的 summary 形式，并由 CTest 约束最终 `result=ok cases=3`。

构建：

```bash
cmake -S Examples/ui/vivid/widget_signal_demo -B cmake-build-vivid-widget-signal-demo-codex -G Ninja
cmake --build cmake-build-vivid-widget-signal-demo-codex -j 22
ctest --test-dir cmake-build-vivid-widget-signal-demo-codex --output-on-failure
cmake-build-vivid-widget-signal-demo-codex/vivid-widget-signal-demo
```
