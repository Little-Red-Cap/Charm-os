# style_token_law_demo

这个示例验证 Vivid Style Token Law v0 的第一条运行证据。

它关注：

- semantic token 通过 `StyleRolePatch` 进入 widget style。
- Button style state mask 保持有边界。
- color token 变化声明为 `paint_only`。
- resolved style color / render artifact 变化，但 metrics 不变。
- final `causal_chain` 汇总 token、impact 与 artifact verdict。

示例 stdout 遵守 `docs/ui/vivid_evidence_stdout_law.md`：统一使用 `[stl] run=style_token_law_demo phase=begin/end` 与 `[stl] case=...` 的 summary 形式，并由 CTest 约束最终 `result=ok cases=7`。

构建：

```bash
cmake -S Examples/ui/vivid/style_token_law_demo -B cmake-build-vivid-style-token-law-demo-codex -G Ninja
cmake --build cmake-build-vivid-style-token-law-demo-codex -j 22
ctest --test-dir cmake-build-vivid-style-token-law-demo-codex --output-on-failure
cmake-build-vivid-style-token-law-demo-codex/vivid-style-token-law-demo
```
