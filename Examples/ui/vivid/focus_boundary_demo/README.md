# Vivid Focus Boundary Demo

`focus_boundary_demo` 是 Vivid Evidence Lab 的 focus 边界样本。

它验证一条很小但关键的 UI 法律：

```text
focused 不进入普通 Button style mask；
focused 改变的是 focus ring / render artifact；
它不改变 Button 的 resolved style evidence。
```

证据链：

```text
focus state delta -> style evidence stable -> focus ring artifact changed -> clear returns baseline -> causal verdict
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
[fb] case=causal_chain ...
[fb] run=focus_boundary_demo phase=end result=ok cases=7
```

CTest 守住最终 `result=ok cases=7`。
