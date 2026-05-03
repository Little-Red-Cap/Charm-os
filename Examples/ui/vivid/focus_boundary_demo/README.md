# Vivid Focus Boundary Demo

`focus_boundary_demo` 是 Vivid Evidence Lab 的 focus 边界样本。

它验证一条很小但关键的 UI 法律：

```text
focused 不进入普通 Button style mask；
focused 改变的是 focus ring / render artifact，
而不是 Button 的 resolved style evidence。
```

stdout 遵守 `docs/ui/vivid_evidence_stdout_law.md`：

```text
[fb] run=focus_boundary_demo phase=begin
[fb] case=style_mask_boundary ...
[fb] case=style_evidence_before ...
[fb] case=focus_state_delta ...
[fb] case=style_evidence_after ...
[fb] case=render_artifact_after ...
[fb] case=focus_clear_artifact ...
[fb] run=focus_boundary_demo phase=end result=ok cases=6
```

CTest 守住最终 `result=ok cases=6`。
