# Vivid Evidence Stdout Law

本文定义 Vivid runtime 示例的最小 stdout 证据格式。

它的目标不是做通用日志系统，而是让 `motion_time_demo`、`page_transition_demo` 这类最小运行时示例具备稳定、可读、可由 CI 约束的证据输出。

## 适用范围

适用于不依赖窗口后端、主要验证 Vivid runtime 语义的示例：

- `Examples/ui/vivid/motion_time_demo`
- `Examples/ui/vivid/page_transition_demo`
- `Examples/ui/vivid/component_card_state_demo`
- `Examples/ui/vivid/component_settings_row_demo`
- `Examples/ui/vivid/style_token_law_demo`
- `Examples/ui/vivid/focus_boundary_demo`
- `Examples/ui/vivid/focus_transfer_demo`
- `Examples/ui/vivid/focus_scope_demo`
- `Examples/ui/vivid/focus_scope_nested_demo`
- `Examples/ui/vivid/focus_scope_navigation_demo`
- `Examples/ui/vivid/focus_spatial_navigation_demo`
- `Examples/ui/vivid/focus_semantic_demo`
- `Examples/ui/vivid/semantic_tree_demo`
- `Examples/ui/vivid/semantic_default_demo`
- `Examples/ui/vivid/semantic_action_demo`
- `Examples/ui/vivid/semantic_intent_demo`
- `Examples/ui/vivid/semantic_action_request_demo`
- `Examples/ui/vivid/semantic_focus_query_demo`
- `Examples/ui/vivid/semantic_focus_admission_demo`
- `Examples/ui/vivid/semantic_focus_request_demo`
- `Examples/ui/vivid/widget_signal_demo`
- `Examples/ui/vivid/widget_state_demo`

未来 `Component Lab`、截图回归、Layer budget drill 或其它 Vivid runtime demo 可以复用这套格式。

## 行格式

每个示例至少输出三类行：

```text
[tag] run=<demo_name> phase=begin
[tag] case=<case_name> key=value ...
[tag] run=<demo_name> phase=end result=ok cases=<n>
```

其中：

- `tag` 是短域名，例如 `mt` 表示 motion time，`pt` 表示 page transition。
- `run` 行标记一次完整示例运行的开始与结束。
- `case` 行标记一个可审计的语义证据点。
- `result` 只能是 `ok` 或 `fail`。
- `cases` 必须等于本次运行输出的 `case` 行数量。

## 字段约定

`case` 行遵守这些规则：

- 使用单行 `key=value`，避免多行自然语言描述。
- 字段名使用小写蛇形或短小语义名，例如 `sampled`、`opacity`、`admission`。
- 值优先使用稳定枚举名或整数，避免依赖本地化文本。
- 不输出地址、指针、耗时抖动、随机值等不稳定内容。
- 一个 case 应尽量对应一个明确语义，例如 `static_profile_fade_slide` 或 `pixel_single_cancel`。

## CTest 审计

每个采用本法律的示例必须在自己的 `CMakeLists.txt` 中接入 CTest：

```cmake
enable_testing()

add_test(NAME ${target_name}
    COMMAND $<TARGET_FILE:${target_name}>)
set_tests_properties(${target_name} PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[tag\\] run=<demo_name> phase=end result=ok cases=<n>"
    FAIL_REGULAR_EXPRESSION "\\[ERR\\]|result=fail")
```

这样 CI 不只验证进程退出码，还会验证示例确实输出了预期的最终证据行。

## 当前注册

| 示例 | tag | 最终约束 |
| --- | --- | --- |
| `page_transition_demo` | `pt` | `[pt] run=page_transition_demo phase=end result=ok cases=15` |
| `motion_time_demo` | `mt` | `[mt] run=motion_time_demo phase=end result=ok cases=12` |
| `component_card_state_demo` | `ccs` | `[ccs] run=component_card_state_demo phase=end result=ok cases=5` |
| `component_settings_row_demo` | `csr` | `[csr] run=component_settings_row_demo phase=end result=ok cases=4` |
| `style_token_law_demo` | `stl` | `[stl] run=style_token_law_demo phase=end result=ok cases=6` |
| `focus_boundary_demo` | `fb` | `[fb] run=focus_boundary_demo phase=end result=ok cases=6` |
| `focus_transfer_demo` | `ft` | `[ft] run=focus_transfer_demo phase=end result=ok cases=7` |
| `focus_scope_demo` | `fs` | `[fs] run=focus_scope_demo phase=end result=ok cases=9` |
| `focus_scope_nested_demo` | `fsn` | `[fsn] run=focus_scope_nested_demo phase=end result=ok cases=8` |
| `focus_scope_navigation_demo` | `fsnav` | `[fsnav] run=focus_scope_navigation_demo phase=end result=ok cases=7` |
| `focus_spatial_navigation_demo` | `fss` | `[fss] run=focus_spatial_navigation_demo phase=end result=ok cases=9` |
| `focus_semantic_demo` | `fsem` | `[fsem] run=focus_semantic_demo phase=end result=ok cases=8` |
| `semantic_tree_demo` | `stree` | `[stree] run=semantic_tree_demo phase=end result=ok cases=6` |
| `semantic_default_demo` | `sdef` | `[sdef] run=semantic_default_demo phase=end result=ok cases=6` |
| `semantic_action_demo` | `sact` | `[sact] run=semantic_action_demo phase=end result=ok cases=6` |
| `semantic_intent_demo` | `sint` | `[sint] run=semantic_intent_demo phase=end result=ok cases=7` |
| `semantic_action_request_demo` | `sar` | `[sar] run=semantic_action_request_demo phase=end result=ok cases=6` |
| `semantic_focus_query_demo` | `sfq` | `[sfq] run=semantic_focus_query_demo phase=end result=ok cases=7` |
| `semantic_focus_admission_demo` | `sfa` | `[sfa] run=semantic_focus_admission_demo phase=end result=ok cases=7` |
| `semantic_focus_request_demo` | `sfr` | `[sfr] run=semantic_focus_request_demo phase=end result=ok cases=7` |
| `widget_signal_demo` | `ws` | `[ws] run=widget_signal_demo phase=end result=ok cases=3` |
| `widget_state_demo` | `wst` | `[wst] run=widget_state_demo phase=end result=ok cases=5` |

## 维护规则

- 新增、删除或拆分 case 时，必须同步更新 `cases=<n>`、CTest 约束与对应 README。
- 如果 case 字段语义发生变化，优先保持字段名兼容；确实需要改名时，同步更新文档。
- stdout 证据只表达运行时事实，不承载长解释；长解释放入 `docs/ui/*`。
- 示例可以保留旧的 `[demo] ok` 人类提示，但 CI 的主审计入口必须是 `[tag] run=... phase=end`。
