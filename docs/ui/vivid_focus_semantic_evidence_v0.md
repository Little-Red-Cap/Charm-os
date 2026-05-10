# Vivid Focus Semantic Evidence v0

Semantic request ledger summary: `vivid_semantic_request_ledger_law_v0.md`.

本文定义 Vivid v0 对 semantic focus target 与 visual focus artifact 对齐的最小证据。

## 定位

`Focus Evidence Boundary v0` 证明 `focused` 不进入普通 style mask。

`Focus Transfer / Scope / Navigation Evidence v0` 证明真实 input dispatch 可以提交 `input_focused`，并让 focus ring artifact 迁移。

`Focus Semantic Evidence v0` 继续向前一步：证明 focus 不只是视觉 ring，也必须能映射到稳定的产品语义 target。

v0 暂不引入完整 accessibility tree。当前已把最小语义三元组写入 Vivid SoA node，并通过 runtime API 暴露：

```text
set_semantic(handle, role, id, label)
semantic_snapshot(handle)
semantic_focus_snapshot()
semantic_tree_snapshot(root, max_nodes)
set_semantic_default(handle, stable_id, optional_label)
```

## v0 法律

### Law 1：semantic target 必须稳定

每个可被语义暴露的 focus target 必须有稳定 id、role 与 label。v0 由 runtime 存储：

```text
semantic_id
role
label
focusable
```

这些字段不应依赖 widget handle 地址或运行时随机值。widget handle 只作为 runtime 绑定点，不是 semantic identity。

### Law 2：input focus truth 必须能解析为 semantic focus

当 `input_focused` 提交到某个 target 后，runtime 必须能解析：

```text
input_focused -> semantic_id
```

如果 focused handle 没有 runtime semantic entry，必须显式输出 `semantic_found=0`，而不是静默假装对齐。

### Law 3：semantic focus 与 visual focus artifact 必须对齐

当 semantic target A 成为 current focus：

```text
input_truth=A.handle
semantic_current=A.semantic_id
visual_focus_ring=A.handle
```

v0 使用 `FocusOut / FocusIn`、`input_focused`、`cmd_hash / pixel_hash` 共同证明对齐。

### Law 4：semantic focus 不得绕过 active scope

scope 外 target 即使存在 semantic entry，也不得被 active scope 内的 keyboard / spatial navigation 选中。

```text
outside_semantic_present=1
outside_selected=0
semantic_current remains inside scope
```

### Law 5：非语义 widget 可以存在，但不能污染 semantic focus

装饰性 label、container、divider 这类 widget 可以参与布局和绘制，但不应成为 semantic focus target。

```text
decorative_present=1
decorative_semantic=0
```

### Law 5b: semantic focus alignment must close a causal_chain

`focus_semantic_demo` 的 final causal verdict 必须同时证明：

```text
semantic table -> stable id / role / label
pointer transfer -> FocusOut / FocusIn + semantic_current
keyboard transfer -> FocusOut / FocusIn + semantic_current
outside semantic target -> outside_selected=0
style boundary -> focused_in_style_mask=0
artifact alignment -> focus_ring=1
causal_chain ok=1
```

### Law 6: semantic tree artifact is a fixed-capacity snapshot

Semantic Tree Artifact v0 is not a full accessibility runtime. It is a root-bound evidence artifact collected from the Vivid SoA tree:

```text
root -> preorder semantic nodes -> semantic_hash
```

v0 rules:

- only nodes with runtime semantic entries are collected.
- collection order is deterministic preorder under the requested root.
- choosing `root` is the artifact policy: a page root can include outside semantic nodes, while a focus scope root can exclude them.
- capacity overflow must be explicit through `overflowed=1`; truncation must not be silent.
- `focus_id` records focus truth even when the focused node is beyond stored capacity.
- `semantic_hash` summarizes the semantic artifact only; it is not yet an accessibility tree hash.
- `semantic_tree_demo` closes these facts with a final `causal_chain` verdict over preorder collection, focus marker, root policy, overflow, and hash stability.

### Law 7: semantic defaults are opt-in, not automatic identity

Pattern Semantic Defaults v0 lets Vivid derive role / label source, but product code must still provide stable semantic id:

```text
set_semantic_default(handle, stable_id)
```

v0 rules:

- default role is derived from `WidgetKind`.
- default label is derived from widget text when no label override is supplied.
- stable id is never derived from handle, pointer, or display text.
- decorative widgets remain non-semantic until explicitly opted in.
- explicit `set_semantic()` may override a previous default.
- `semantic_default_demo` closes these facts with a final `causal_chain` verdict over role derivation, label source, explicit override, decorative boundary, and tree artifact integration.

### Law 8: semantic actions are artifact facts, not event execution

Semantic Action Artifact v0 lets a semantic node expose the minimal fixed action mask it supports:

```text
semantic_id
role
label
actions=activate
```

v0 rules:

- actions are stored as a fixed `SemanticActionMask`.
- `Button` and `ListItem` default to `activate`; `Text` and `Container` default to no action.
- product/runtime code may explicitly override the action mask through `set_semantic_actions()`.
- semantic tree nodes carry action masks and include them in `semantic_hash`.
- an action bit is evidence of capability, not a request to synthesize input or invoke OS accessibility.
- v0 does not introduce a full accessibility runtime.
- `semantic_action_demo` closes these facts with a final `causal_chain` verdict over role-derived actions, no-action roles, explicit override, tree action artifacts, and hash participation.

### Law 9: semantic intent resolution is address lookup, not execution

Semantic Intent Resolution v0 lets runtime resolve a product request:

```text
root + semantic_id + action -> SemanticIntentResolution
```

v0 rules:

- lookup is root-bound and deterministic.
- duplicate ids under the requested root are `ambiguous_id`, not silently first-match.
- missing id, invalid root, unsupported action, and disabled target are explicit status values.
- `resolved` means the target is addressable and currently executable.
- resolution must not synthesize input, dispatch callbacks, mutate pressed/focused state, or bind OS accessibility.
- execution remains a future, separate admission step.

### Law 10: semantic action admission is execution permission, not execution

Semantic Action Admission v0 turns a successful semantic intent resolution into an explicit action execution plan:

```text
root + semantic_id + action -> SemanticActionAdmission
```

v0 rules:

- admission reuses the root-bound `SemanticIntentResolution` result instead of inventing a second lookup law.
- intent failures are mapped to admission rejection statuses with the same semantic reason.
- `admitted` means the runtime may later request focus and emit the action; it does not mean the action already ran.
- admitted plans must declare whether a future execution would request focus and emit a click.
- admission must not synthesize input events, mutate input focus truth, press widgets, toggle state, dispatch callbacks, or draw artifacts.
- full request execution still has a later focus-admission boundary and may reject before click emission when focus preparation is denied.

### Law 11: semantic focus query is focus addressability, not focus transfer

Semantic Focus Query v0 lets runtime answer whether a semantic id can become focus under a requested root and current active scope:

```text
root + semantic_id + active_scope -> SemanticFocusQuery
```

v0 rules:

- query is root-bound and deterministic.
- duplicate ids under the requested root are `ambiguous_id`.
- non-focusable, disabled, invalid root, missing id, and missing target are explicit status values.
- active trapped focus scope may reject an otherwise valid target as `outside_active_scope`.
- `resolved` means the target is focus-addressable now.
- query must not emit `FocusIn/FocusOut`, mutate input focus truth, or draw focus ring artifacts.

### Law 12: semantic focus admission is transfer permission, not transfer execution

Semantic Focus Admission v0 turns a successful focus query into an explicit focus-transfer plan:

```text
root + semantic_id + current_focus + active_scope -> SemanticFocusAdmission
```

v0 rules:

- admission reuses the root-bound `SemanticFocusQuery` result instead of performing a second semantic law.
- query failures are mapped to admission rejection statuses with the same semantic reason.
- `admitted` means the runtime may transfer focus later; it does not mean focus already moved.
- `already_focused` is admitted but has `transfer_needed=0` and no planned `FocusOut/FocusIn`.
- transfer plans must declare whether a future execution would emit `FocusOut` and `FocusIn`.
- admission must not emit `FocusIn/FocusOut`, mutate input focus truth, or draw focus ring artifacts.

### Law 13: semantic focus request is the first controlled transfer execution

Semantic Focus Request v0 is the execution boundary after query and admission:

```text
SemanticFocusQuery -> SemanticFocusAdmission -> SemanticFocusRequest
```

v0 rules:

- request must first run admission and reject with the same admission reason when admission fails.
- request may mutate input focus truth only when admission is `admitted` and `transfer_needed=1`.
- request must reuse the normal input focus transfer path so `FocusOut/FocusIn` and focused state evidence stay consistent.
- `already_focused` is an explicit no-op and must not emit focus events.
- rejected requests must preserve the current focus truth and must not emit focus events.
- committed requests must expose before/after focus truth and event evidence.
- runtime exposes `SemanticFocusRequestLedger` so focus request evidence uses the same artifact language as action request evidence.
- request stdout evidence should use `ledger=focus_request stage=focus_admission/already_focused/execution`.

### Law 14: semantic action request rejection must name its boundary

Semantic Action Request v0 is allowed to cross from semantic planning into input execution, so rejection cannot remain an undifferentiated `rejected` fact.

v0 request ledger:

```text
SemanticActionAdmission rejected -> action_admission_rejected
SemanticFocusRequest rejected    -> focus_request_rejected
input action queue overflow      -> input_action_overflow
no click emitted after execution -> no_action_emitted
```

v0 rules:

- successful requests must report `reject_reason=none`.
- unsupported action, disabled target, ambiguous id, missing id, and invalid root must reject at the action admission boundary.
- active scope denial must reject at the focus request boundary after action admission succeeds.
- rejected requests must not emit click evidence unless the reason explicitly represents an execution-time failure.
- stdout evidence must print both high-level status and rejection reason.
- runtime exposes `SemanticActionRequestLedger` so ledger semantics are not trapped inside demo printf code.
- request stdout evidence should use `ledger=action_request stage=<boundary>` from the runtime ledger artifact so each case records the last boundary reached.

## 首个落点

`Examples/ui/vivid/focus_semantic_demo` 是 Focus Semantic Evidence v0 的第一条运行证据。

它验证：

- runtime semantic store 中存在 `primary / secondary / outside` 三个稳定条目。
- decorative label 不进入 runtime semantic store。
- pointer focus 可以从 primary 迁移到 secondary，并解析为 `semantic_id=secondary`。
- keyboard navigation 在 active scope 内迁移 semantic focus。
- scope 外 semantic target 不参与 active scope navigation。
- focus ring artifact 与 semantic current target 对齐。

stdout 最终约束：

```text
[fsem] run=focus_semantic_demo phase=end result=ok cases=9
```

`Examples/ui/vivid/semantic_tree_demo` is the first Semantic Tree Artifact v0 runtime evidence.
It verifies deterministic preorder collection, decorative exclusion, focus markers, root-bound policy, overflow reporting, stable semantic hash, and final causal-chain evidence.

stdout final contract:

```text
[stree] run=semantic_tree_demo phase=end result=ok cases=7
```

`Examples/ui/vivid/semantic_default_demo` is the first Pattern Semantic Defaults v0 runtime evidence.
It verifies default role derivation, text-based label source, explicit label override, opt-in boundary for decorative labels, explicit override, semantic tree artifact integration, and final causal-chain evidence.

stdout final contract:

```text
[sdef] run=semantic_default_demo phase=end result=ok cases=7
```

`Examples/ui/vivid/semantic_action_demo` is the first Semantic Action Artifact v0 runtime evidence.
It verifies role-derived activate defaults, non-action semantic roles, explicit action override, tree action artifacts, semantic hash participation, and final causal-chain evidence.

stdout final contract:

```text
[sact] run=semantic_action_demo phase=end result=ok cases=7
```

`Examples/ui/vivid/semantic_intent_demo` is the first Semantic Intent Resolution v0 runtime evidence.
It verifies root-bound id/action lookup, no-execute side effects, unsupported action, missing id, ambiguous duplicate id, disabled target, and invalid request statuses.

stdout final contract:

```text
[sint] run=semantic_intent_demo phase=end result=ok cases=7
```

`Examples/ui/vivid/semantic_action_request_demo` is the first Semantic Action Request v0 runtime evidence.
It verifies that semantic intent resolution and action admission remain side-effect free, while action request crosses into controlled execution: it prepares semantic focus through `SemanticFocusRequest`, emits a `Click` event, reuses normal widget click behavior, rejects unsupported action ids before execution, rejects scope-forbidden targets through focus admission, and proves rejected requests do not emit focus or click events.
It also records `SemanticActionRequestRejectReason` so CI can distinguish action-admission rejection from focus-request rejection.
The main request cases emit `ledger=action_request stage=action_admission/focus_request/execution` lines generated from `SemanticActionRequestLedger`, and the execution cases include focus/click event traces so `focus_ready` and `click` are tied back to input evidence.

stdout final contract:

```text
[sar] run=semantic_action_request_demo phase=end result=ok cases=11
```

`Examples/ui/vivid/semantic_action_admission_demo` is the first Semantic Action Admission v0 runtime evidence.
It verifies admitted activate plans, no execution side effects, focus/click plan evidence, unsupported action rejection, disabled target rejection, ambiguous duplicate ids, missing ids, invalid root, and missing request id.

stdout final contract:

```text
[saa] run=semantic_action_admission_demo phase=end result=ok cases=7
```

`Examples/ui/vivid/semantic_focus_query_demo` is the first Semantic Focus Query v0 runtime evidence.
It verifies focus-addressable semantic ids, no focus transfer side effects, non-focusable targets, disabled targets, active-scope rejection, ambiguous duplicate ids, and invalid request statuses.

stdout final contract:

```text
[sfq] run=semantic_focus_query_demo phase=end result=ok cases=7
```

`Examples/ui/vivid/semantic_focus_admission_demo` is the first Semantic Focus Admission v0 runtime evidence.
It verifies admitted transfer plans, already-focused no-op plans, no focus transfer side effects, non-focusable and disabled rejection, active-scope rejection, ambiguous duplicate ids, and invalid request statuses.

stdout final contract:

```text
[sfa] run=semantic_focus_admission_demo phase=end result=ok cases=7
```

`Examples/ui/vivid/semantic_focus_request_demo` is the first Semantic Focus Request v0 runtime evidence.
It verifies controlled focus transfer execution, `FocusOut/FocusIn` event evidence, semantic focus truth after request, request-driven focus artifact evidence, rejection without artifact mutation, final causal-chain evidence, already-focused no-op, active-scope rejection, non-focusable and disabled rejection, ambiguous duplicate ids, and invalid request statuses.
The main request cases emit `ledger=focus_request stage=focus_admission/already_focused/execution` lines generated from `SemanticFocusRequestLedger`.
It also records the visual consequence of the semantic request: focused style evidence remains stable, while the focus ring render artifact moves to the semantic destination with bounded dirty evidence.

stdout final contract:

```text
[sfr] run=semantic_focus_request_demo phase=end result=ok cases=12
```

核心字段：

```text
semantic_id=primary/secondary/outside
role=button/list_item
actions=activate
intent_status=resolved/ambiguous_id/unsupported_action/disabled
action_admission_status=admitted/ambiguous_id/unsupported_action/disabled
action_request_reason=none/action_admission_rejected/focus_request_rejected/input_action_overflow/no_action_emitted
action_will_request_focus=0/1
action_will_emit_click=0/1
focus_query_status=resolved/outside_active_scope/not_focusable/disabled
focus_admission_status=admitted/already_focused/outside_active_scope/not_focusable/disabled
focus_request_status=committed/already_focused/rejected
focus_request_stage=focus_admission/already_focused/execution
committed=0/1
transfer_needed=0/1
focus_out=0/1
focus_in=0/1
semantic_found=1
semantic_current=secondary
semantic_hash=...
input_truth=secondary
focus_ring=1
outside_semantic_present=1
outside_selected=0
decorative_semantic=0
causal_chain=1
overflowed=1
```

## 后续方向

- 区分 input focus、semantic focus、accessibility focus 与 visual focus ring。
- 输出 semantic tree / accessibility tree 的 artifact hash。
- 让 component pattern 声明默认 semantic role 与 label source。
