# menu_tree_demo

这个示例专门用最小 SoA `SoaKernel / SoaFactory / MenuTree` 代码，
冻结 `MenuTree` 当前的 truth/edge 边界。

它要证明的是：

- `StructuredMenuSelectionModel` 才是 `MenuTree` 的高亮 truth 持有面
- `MouseMove` 之类的导航会推进外部 selection truth，但不会制造 confirm edge
- `Click / Enter / Space` 确认 leaf 时，才会触发 `observe_select()` 与旧 `set_on_select()` 兼容回调

这个示例当前覆盖了：

- `MenuTree::observe_select()`
- external `StructuredMenuSelectionModel`
- legacy `set_on_select()`

其中最关键的 contract 点是：

- opening menu / opening submenu 可以 materialize highlight truth，但不等于 confirm
- outside click 只负责关闭 menu，不会制造隐藏 confirm
- disconnect token 生效后，confirm edge 观察者保持静默，但 legacy callback 仍按原语义工作

如果你要看 SoA-backed helper 自己持有 committed truth 的版本，请继续看：

- `Examples/ui/vivid/dropdown_popup_demo`

如果你要看 object-level widget 的 truth/edge 表面，请继续看：

- `Examples/ui/vivid/widget_signal_demo`
- `Examples/ui/vivid/widget_state_demo`

如果你要看 `SceneAccess` 的场景级显式状态推进，请继续看：

- `Examples/ui/vivid/scene_state_demo`

构建：

```bash
cmake -S Examples/ui/vivid/menu_tree_demo -B cmake-build-vivid-menu-tree-demo -G Ninja
cmake --build cmake-build-vivid-menu-tree-demo
```
