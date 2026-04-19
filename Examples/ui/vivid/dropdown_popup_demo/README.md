# dropdown_popup_demo

这个示例专门用最小 SoA `SoaKernel / SoaFactory / DropdownPopup` 代码，
冻结 `DropdownPopup` 当前的 truth/edge 边界。

它要证明的不是 “popup 能不能弹出来”，而是这三条语义：

- `set_selection()` 只改 committed selection truth，不产生 confirm edge
- `MouseMove / MouseWheel / Up / Down` 只移动临时高亮，不偷偷改 committed truth
- `Click / Enter / Space` 确认时，才同时触发 `observe_select()` 和旧 `set_on_select()` 兼容回调

这个示例当前覆盖了：

- `DropdownPopup::observe_selected()`
- `DropdownPopup::observe_select()`
- legacy `set_on_select()`

其中最关键的 contract 点是：

- 同一个选项被再次确认时，不会伪造 truth change，但仍然会发 confirm edge
- popup 外点击只负责关闭，不会制造隐藏 confirm
- disconnect token 生效后，truth/edge 观察者保持静默，但底层 committed truth 与 legacy callback 语义仍然成立

如果你要看 Vivid object-level widget 的 truth/edge 表面，请继续看：

- `Examples/ui/vivid/widget_signal_demo`
- `Examples/ui/vivid/widget_state_demo`

如果你要看 SoA `SceneAccess` 的场景级显式状态推进，请继续看：

- `Examples/ui/vivid/scene_state_demo`

构建：

```bash
cmake -S Examples/ui/vivid/dropdown_popup_demo -B cmake-build-vivid-dropdown-popup-demo -G Ninja
cmake --build cmake-build-vivid-dropdown-popup-demo
```
