# Vivid Focus Semantic Demo

`focus_semantic_demo` 是 Vivid Evidence Lab 的 semantic focus 对齐样本。

它验证：

```text
semantic target table provides stable id / role / label
input_focused resolves to semantic current target
FocusOut / FocusIn stays synchronized with semantic current
focus ring artifact aligns with semantic target
outside semantic target does not participate in active scope navigation
decorative widget does not enter semantic target table
semantic focus closes a final causal_chain verdict
```

stdout 遵守 `docs/ui/vivid_evidence_stdout_law.md`：

```text
[fsem] run=focus_semantic_demo phase=begin
[fsem] case=semantic_table ...
[fsem] case=decorative_excluded ...
[fsem] case=initial_semantic_focus ...
[fsem] case=transfer_semantic_focus ...
[fsem] case=keyboard_semantic_focus ...
[fsem] case=outside_semantic_not_selected ...
[fsem] case=style_boundary ...
[fsem] case=artifact_alignment ...
[fsem] case=causal_chain ...
[fsem] run=focus_semantic_demo phase=end result=ok cases=9
```
