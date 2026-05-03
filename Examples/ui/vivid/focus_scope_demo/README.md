# Vivid Focus Scope Demo

`focus_scope_demo` 是 Vivid Evidence Lab 的 focus scope 样本。

它验证：

```text
FocusScope 允许 scope 内迁移；
FocusScope 拒绝 scope 外请求；
被拒绝后 scope truth 保持在 fallback/current；
focus ring artifact 不泄漏到 scope 外 target。
```

stdout 遵守 `docs/ui/vivid_evidence_stdout_law.md`：

```text
[fs] run=focus_scope_demo phase=begin
[fs] case=scope_model ...
[fs] case=initial_focus ...
[fs] case=inside_request_decision ...
[fs] case=inside_transfer_artifact ...
[fs] case=outside_request_decision ...
[fs] case=scope_trap_truth ...
[fs] case=scope_trap_artifact ...
[fs] run=focus_scope_demo phase=end result=ok cases=7
```

CTest 守住最终 `result=ok cases=7`。

详细法律见 `docs/ui/vivid_focus_scope_evidence_v0.md`。
