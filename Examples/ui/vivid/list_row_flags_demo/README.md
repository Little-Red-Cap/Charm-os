# list_row_flags_demo

这个示例专门用最小 SoA `Scene / SceneAccess / ListView` 代码，
冻结共享 row capability 的第一版 contract。

它要证明的是：

- `row_flags` 是 list row 的共享能力面，不是某个 helper 私有实现细节
- `group` 与 `disabled` 都应通过共享 flag 暴露
- 通用 `ListView` 输入点击 disabled row 时，不会偷偷推进 selection truth
- programmatic `set_list_view_selected()` 仍然可以显式写入 selection truth

这个示例当前覆盖了：

- `SceneAccess::set_list_view_row_flags_source()`
- `SceneAccess::list_view_item_row_flags()`
- `SceneAccess::list_view_selected()`

其中最关键的 contract 点是：

- disabled row capability 可见
- disabled row click 不制造隐藏 selection
- enabled row click 才推进通用 list selection
- 程序化 truth 写入与输入 law 分离

构建：

```bash
cmake -S Examples/ui/vivid/list_row_flags_demo -B cmake-build-vivid-list-row-flags-demo -G Ninja
cmake --build cmake-build-vivid-list-row-flags-demo
```
