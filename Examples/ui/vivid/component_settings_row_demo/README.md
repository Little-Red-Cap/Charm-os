# component_settings_row_demo

这个示例验证 Vivid Render Evidence Chain v0 的第一条 component 级因果证据链。

它刻意不做 screenshot golden，而是先证明：

```text
state truth -> invalidation intent -> dirty evidence -> draw command evidence -> render artifact evidence -> causal verdict
```

当前 component 是一个 settings row：

- `Label` 展示标题。
- `Slider` 承载可变 truth。
- `ProgressBarSimple` 镜像 slider truth。
- value label 展示派生文本。

示例 stdout 遵守 `docs/ui/vivid_evidence_stdout_law.md`：统一使用 `[csr] run=component_settings_row_demo phase=begin/end` 与 `[csr] case=...` 的 summary 形式，并由 CTest 约束最终 `result=ok cases=5`。

构建：

```bash
cmake -S Examples/ui/vivid/component_settings_row_demo -B cmake-build-vivid-component-settings-row-demo-codex -G Ninja
cmake --build cmake-build-vivid-component-settings-row-demo-codex -j 22
ctest --test-dir cmake-build-vivid-component-settings-row-demo-codex --output-on-failure
cmake-build-vivid-component-settings-row-demo-codex/vivid-component-settings-row-demo
```
