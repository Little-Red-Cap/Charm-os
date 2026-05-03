# Vivid Focus Scope Nested Demo

`focus_scope_nested_demo` 是 Vivid Evidence Lab 的 nested/modal focus scope 样本。

它验证：

```text
base scope 可以先接管 focus admission；
modal scope 可以 push 到 active scope；
modal 外请求不产生 focus transfer；
pop 后恢复 base scope；
每个阶段 focus ring artifact 不泄漏到被拒绝 target。
```

stdout 遵守 `docs/ui/vivid_evidence_stdout_law.md`：

```text
[fsn] run=focus_scope_nested_demo phase=begin
[fsn] case=tree_model ...
[fsn] case=base_scope_install ...
[fsn] case=base_initial_focus ...
[fsn] case=modal_scope_push ...
[fsn] case=modal_inside_focus ...
[fsn] case=modal_trap_dispatch ...
[fsn] case=modal_scope_pop ...
[fsn] case=restored_base_trap ...
[fsn] run=focus_scope_nested_demo phase=end result=ok cases=8
```

CTest 守住最终 `result=ok cases=8`。
