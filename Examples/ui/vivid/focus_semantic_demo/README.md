# Vivid Focus Semantic Demo

`focus_semantic_demo` 是 Vivid Evidence Lab 的 semantic focus 对齐样本。

它验证：

```text
semantic target table 提供稳定 id / role / label；
input_focused 可以解析为 semantic current target；
FocusOut / FocusIn 与 semantic current 同步；
focus ring artifact 与 semantic target 对齐；
scope 外 semantic target 不参与 active scope navigation；
decorative widget 不进入 semantic target table。
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
[fsem] run=focus_semantic_demo phase=end result=ok cases=8
```
