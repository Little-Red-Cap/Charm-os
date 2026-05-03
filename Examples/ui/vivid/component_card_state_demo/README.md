# component_card_state_demo

这个示例验证 Vivid Render Evidence Chain v0 的第二个 component 级样本。

它关注多个 child state 汇入同一个 card artifact：

- `Checkbox` 控制 enabled truth。
- `Slider` 控制 level truth。
- `ProgressBarSimple` 展示派生 output。
- summary label 展示组合状态。

示例 stdout 遵守 `docs/ui/vivid_evidence_stdout_law.md`：统一使用 `[ccs] run=component_card_state_demo phase=begin/end` 与 `[ccs] case=...` 的 summary 形式，并由 CTest 约束最终 `result=ok cases=5`。

构建：

```bash
cmake -S Examples/ui/vivid/component_card_state_demo -B cmake-build-vivid-component-card-state-demo-codex -G Ninja
cmake --build cmake-build-vivid-component-card-state-demo-codex -j 22
ctest --test-dir cmake-build-vivid-component-card-state-demo-codex --output-on-failure
cmake-build-vivid-component-card-state-demo-codex/vivid-component-card-state-demo
```
