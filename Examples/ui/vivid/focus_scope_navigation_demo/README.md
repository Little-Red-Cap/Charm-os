# Vivid Focus Scope Navigation Demo

`focus_scope_navigation_demo` 是 Vivid Evidence Lab 的 keyboard / d-pad focus navigation 样本。

它验证：

```text
Tab / Right / Down 在 active focus scope 内前进；
Left / Up 在 active focus scope 内后退；
导航按 preorder focusable 顺序循环；
scope 外 target 不参与键盘焦点导航；
每次导航都产生 FocusOut / FocusIn 与 input_focused truth 提交；
ordered navigation closes a final causal_chain verdict。
```

stdout 遵守 `docs/ui/vivid_evidence_stdout_law.md`：

```text
[fsnav] run=focus_scope_navigation_demo phase=begin
[fsnav] case=scope_model ...
[fsnav] case=initial_focus ...
[fsnav] case=tab_to_second ...
[fsnav] case=right_to_third ...
[fsnav] case=down_wrap_first ...
[fsnav] case=left_wrap_third ...
[fsnav] case=outside_not_in_nav ...
[fsnav] case=causal_chain ...
[fsnav] run=focus_scope_navigation_demo phase=end result=ok cases=8
```
