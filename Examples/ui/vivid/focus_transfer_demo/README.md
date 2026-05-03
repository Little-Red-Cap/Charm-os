# Vivid Focus Transfer Demo

`focus_transfer_demo` 是 Vivid Evidence Lab 的 focus 迁移样本。

它验证：

```text
source focus ring -> destination focus ring
old target emits FocusOut
new target emits FocusIn
ordinary Button style evidence stays stable
```

stdout 遵守 `docs/ui/vivid_evidence_stdout_law.md`：

```text
[ft] run=focus_transfer_demo phase=begin
[ft] case=style_mask_boundary ...
[ft] case=initial_focus_artifact ...
[ft] case=transfer_event_trace ...
[ft] case=focus_truth_after_transfer ...
[ft] case=style_evidence_after_transfer ...
[ft] case=render_artifact_after_transfer ...
[ft] case=clear_destination_focus ...
[ft] run=focus_transfer_demo phase=end result=ok cases=7
```

CTest 守住最终 `result=ok cases=7`。
