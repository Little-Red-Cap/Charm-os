# Vivid Render Evidence Chain v0

## 2026-05 补记：Focus Evidence

Focus 进入 Evidence Plane 后，必须证明它是 navigation / focus ring artifact，而不是普通 Button style mask 的新维度：

```text
focused_in_style_mask=0
style_same=1
artifact_changed=1
focus_ring=1
```

v0 由 `Examples/ui/vivid/focus_boundary_demo` 验证：`set_focused(true)` 不改变 `ResolvedStyleEvidence`，但会改变 draw command evidence 与 render artifact；`set_focused(false)` 后 artifact 回到 baseline，并由 final `causal_chain` 汇总 focus boundary verdict。详细法律见 `vivid_focus_evidence_boundary_v0.md`。

`Examples/ui/vivid/focus_transfer_demo` 继续验证 focus transfer evidence：真实 input dispatch 产生 `FocusOut / FocusIn`，focus truth 提交到新 target，style evidence 保持稳定，artifact 迁移由 `dirty_hash / pixel_hash / target` 证明，并由 final `causal_chain` 汇总 transfer verdict。详细法律见 `vivid_focus_transfer_evidence_v0.md`。

`Examples/ui/vivid/focus_scope_demo` 继续验证 focus scope evidence：scope policy 已接入真实 input dispatch，内部请求产生 `FocusOut / FocusIn` 并提交 `input_focused`，外部请求保留 pointer event 但不产生 focus transfer，并用外部 target 的 unfocused baseline 证明 focus ring artifact 不泄漏，最终由 `causal_chain` 汇总 inside allow / outside reject / no-leak verdict。详细法律见 `vivid_focus_scope_evidence_v0.md`。

`Examples/ui/vivid/focus_scope_nested_demo` 继续验证 nested/modal focus scope evidence：push 后 active scope 切换到 modal，pop 后恢复 base scope；modal 外请求不产生 focus transfer，也不把 focus ring artifact 泄漏到 base target，并由 final `causal_chain` 汇总 push/pop transaction verdict。详细法律见 `vivid_focus_scope_evidence_v0.md`。

`Examples/ui/vivid/focus_scope_navigation_demo` 继续验证 keyboard / d-pad focus navigation evidence：key event 不直接写 visual state，而是产生 `FocusOut / FocusIn`、提交 `input_focused`，再由 focus ring artifact 证明结果；scope 外 target 的 command evidence 保持 baseline。详细法律见 `vivid_focus_scope_evidence_v0.md`。

`Examples/ui/vivid/focus_spatial_navigation_demo` 继续验证 spatial focus navigation evidence：方向键先由 runtime 根据 world rect 选择几何候选，无候选再回退到 preorder wrap；scope 外 target 的 command evidence 保持 baseline。详细法律见 `vivid_focus_scope_evidence_v0.md`。

`Examples/ui/vivid/focus_semantic_demo` 继续验证 semantic focus evidence：runtime semantic store 可以把 `input_focused` 解析为稳定 semantic id / role / label，semantic current target 与 visual focus ring artifact 对齐，scope 外 semantic target 不参与 active scope navigation。详细法律见 `vivid_focus_semantic_evidence_v0.md`。

本文定义 Vivid 从 widget 级证据进入 component 级因果证据的最小路线。

它的目标不是替代截图回归，而是在截图之前先证明：

```text
state truth
  -> invalidation intent
  -> dirty evidence
  -> draw command evidence
  -> render artifact evidence
```

## 定位

`vivid_evidence_stdout_law.md` 定义 stdout 行格式。

本文定义 stdout 中应该逐步承载哪些 UI 因果事实。

## v0 证据段

### State Truth

状态变化必须能回答：

```text
哪个 truth 变了？
旧值和新值是什么？
变化来源是什么？
```

v0 先不要求所有 Vivid 控件都产出统一 `StateDelta` 类型，但 component demo 必须在 case 中输出稳定的状态摘要。

2026-05 addendum: `Examples/ui/vivid/support/vivid_evidence_support.hpp` now provides a demo-side `StateDeltaEvidence` helper. It standardizes stdout fields without promoting a core Vivid API:

```text
state_delta=<0|1> id=<id> key=<key> old=<old> new=<new> changed=<0|1> source=<source>
```

Multi-delta component cases may use named prefixes such as `enabled_old` / `level_old`, but they should still preserve `id`, `key`, `old`, `new`, `changed`, and `source` as the stable causal vocabulary.

### Invalidation Intent

状态变化必须声明预期影响：

```text
none
paint_only
layout
text_metrics
style
render_cache
```

v0 先允许 demo 以 `invalidation=paint_only` 这类字段表达 intent。

2026-05 addendum: `Examples/ui/vivid/support/vivid_evidence_support.hpp` now provides a demo-side `InvalidationEvidence` helper:

```text
invalidation=1 kind=<kind> dirty_scope=<scope> component_x=<x> component_y=<y> component_w=<w> component_h=<h> layout_changed=<0|1>
```

### Dirty Evidence

渲染后必须能回答：

```text
是否产生 dirty？
dirty 是否局部？
dirty 是否越出 component 边界？
```

v0 使用 `dirty_count` 与 `dirty_hash` 做最小证据。

### Draw Command Evidence

截图之前，先审计绘制意图：

```text
cmd_count
cmd_bytes
cmd_hash
exec_cmds
failed_cmds
```

v0 可以用 `Scene::last_cmd_stats()` 与 `Scene::last_exec_stats()` 的稳定字段组成 `cmd_hash`。

2026-05 addendum: `Examples/ui/vivid/support/vivid_evidence_support.hpp` now provides `print_render_evidence(prefix, evidence)`. It emits a stable prefixed summary:

```text
<prefix>_dirty_count=<n> <prefix>_dirty_hash=<hash> <prefix>_cmd_count=<n> <prefix>_cmd_bytes=<n> <prefix>_exec_cmds=<n> <prefix>_failed=<n> <prefix>_cmd_hash=<hash> <prefix>_pixel_hash=<hash>
```

### Render Artifact Evidence

最终 artifact 先用摘要表达：

```text
width
height
pixel_hash
```

PNG / screenshot diff 是后续投影，不是 v0 的第一真相。

2026-05 addendum: demo support now provides `RenderArtifactDeltaEvidence` for the final artifact verdict:

```text
artifact_delta=<0|1> changed=<0|1> dirty_within_component=<0|1> single_dirty_rect=<0|1>
```

This delta complements prefixed render summaries. It tells whether the artifact changed and whether the dirty evidence stayed inside the claimed component boundary.

`Examples/ui/vivid/support/vivid_evidence_support.hpp` also provides small stdout composition helpers:

```text
render_component_artifact_delta(scene, canvas, component_bounds, before)
print_render_artifact_verdict(delta, prefix, evidence)
print_render_artifact_comparison(delta, before, after)
```

These helpers keep Component Lab, Focus Evidence, Style Token Law, and Intent-to-Artifact cases aligned on the same artifact capture / verdict shape. They are demo-side evidence vocabulary, not a Vivid core render API.

Promotion boundary: `vivid_evidence_artifact_promotion_v0.md` records which Evidence Lab names remain demo-only, which field vocabularies are law candidates, and which runtime-native ledgers may become core-facing contracts.

Field vocabulary: `vivid_evidence_vocabulary_law_v0.md` defines the stable meaning of `StateDeltaEvidence`, `InvalidationEvidence`, `RenderEvidence`, `RenderArtifactDeltaEvidence`, and `CausalChainEvidence` fields.

### Style Evidence

Style Token Law 进入 Evidence Plane 后，resolved style 也需要可审计摘要：

```text
style_key
color_hash
metrics_hash
style_state_mask
state_count
impact
impact_mask
```

v0 由 `charm.core.style_evidence` 提供 `ResolvedStyleEvidence` 与 `StyleStateEvidence`，并由 `Examples/ui/vivid/style_token_law_demo` 验证 color token 变化只改变 color evidence，不改变 metrics evidence；同时验证 Button 普通 style mask 包含 hovered / pressed / disabled，但不包含 focused。

`Examples/ui/vivid/support/vivid_evidence_support.hpp` provides demo-side stdout helpers for the stable style evidence shape:

```text
print_style_state_mask(widget, law, evidence)
print_resolved_style_evidence(widget, state, evidence)
print_focus_style_evidence(widget, focused, evidence, style_same, focused_in_style_mask)
```

These helpers keep Style Token Law and Focus Evidence demos aligned without promoting style stdout formatting into a Vivid core API.

`Examples/ui/vivid/style_token_law_demo` closes its style evidence path with a final `causal_chain` verdict:

```text
button.accent_token
  -> token version + resolved style color
  -> paint_only impact
  -> stable metrics
  -> bounded render artifact
  -> causal_chain ok=1
```

## Evidence Lab 支撑工具

`Examples/ui/vivid/support/vivid_evidence_support.hpp` 是 v0 的示例侧共享证据账本。

它先服务 Component Lab，不作为 Vivid core 公共 API 承诺：

- `RunLog` 统一 begin / case / end stdout 计数。
- `expect()` 统一 `[ERR]` 失败出口。
- `RenderEvidence` 聚合 dirty / command / pixel artifact 摘要。
- `render_scene()` 统一 record / execute 后的证据采集。
- `dirty_stays_inside()` 验证 component dirty 不越界。
- `same_handle()` / `click_center()` / `mouse_down_center()` / `mouse_up_center()` 收束 demo-side handle comparison 与 pointer setup，不作为 Vivid core input API。
- `FocusMoveTrace` / `collect_focus_move()` 与 `PointerFocusTrace` / `collect_pointer_focus_trace()` 收束 demo-side pointer / focus event 计数，不作为 Vivid core focus transaction API。
- `count_click_events_since()` 收束 demo-side semantic action click evidence 计数，不作为 Vivid core input ledger API。

## 首个落点

`Examples/ui/vivid/component_settings_row_demo` 是第一条 component 级证据链样本。

它验证一个 settings row component 中：

- slider truth 变化可被观测。
- progress mirror 受 slider truth 驱动。
- value label 随 truth 更新。
- 本次变化声明为 `paint_only`。
- render 后输出 dirty / command / pixel artifact 摘要。

`Examples/ui/vivid/component_card_state_demo` 是第二条 component 级证据链样本。

它验证一个 card component 中：

- checkbox 与 slider 两个 child truth 同时变化。
- progress mirror 与 summary label 汇入同一个 component artifact。
- 多 child state 变化仍声明为 `paint_only`。
- render 后输出单个 card dirty rect 与 command / pixel artifact 摘要。

stdout 仍遵守 `vivid_evidence_stdout_law.md`。

## 2026-05 Addendum: Component Settings Row Causal Verdict

`Examples/ui/vivid/component_settings_row_demo` now closes the first component-level causal chain with a final `causal_chain` verdict:

```text
settings_row.value
  -> state_delta
  -> paint_only invalidation
  -> bounded dirty / DrawCmd / pixel artifact
  -> causal_chain ok=1
```

The final verdict proves the slider truth, progress mirror, value label, paint-only invalidation intent, single dirty rect, component-bounded dirty evidence, and changed render artifact are treated as one auditable component consequence.

## 2026-05 Addendum: Component Card State Causal Verdict

`Examples/ui/vivid/component_card_state_demo` extends the component causal chain to multiple child state deltas:

```text
power_card.state
  -> enabled + level state_delta
  -> output + summary derivation
  -> paint_only invalidation
  -> bounded dirty / DrawCmd / pixel artifact
  -> causal_chain ok=1
```

The final verdict proves multiple child truths can feed one component artifact without losing locality or causality evidence.

## 2026-05 Addendum: Semantic Artifact Evidence

`Examples/ui/vivid/semantic_tree_demo` extends semantic focus evidence into a root-bound Semantic Tree Artifact v0. The artifact is still intentionally smaller than an accessibility runtime: it collects runtime semantic entries under a requested root, preserves deterministic preorder, marks focus truth, reports fixed-capacity overflow, and emits `semantic_hash`.

The evidence chain now has a semantic branch before screenshot CI:

```text
semantic store
  -> semantic focus snapshot
  -> semantic tree snapshot
  -> semantic_hash
```

Stdout remains governed by `vivid_evidence_stdout_law.md`:

```text
[stree] run=semantic_tree_demo phase=end result=ok cases=6
```

## 2026-05 Addendum: Pattern Semantic Defaults

`Examples/ui/vivid/semantic_default_demo` verifies Pattern Semantic Defaults v0. This is deliberately opt-in: Vivid derives default role and label source, while product code still supplies stable semantic id.

Evidence chain:

```text
WidgetKind + text
  -> set_semantic_default(stable_id)
  -> semantic snapshot
  -> semantic tree artifact
```

The demo guards that decorative widgets are not auto-enrolled and that explicit `set_semantic()` can override a default.

## 2026-05 Addendum: Semantic Action Artifact

`Examples/ui/vivid/semantic_action_demo` verifies Semantic Action Artifact v0. Semantic nodes can now carry fixed action facts without turning Vivid into a full accessibility runtime:

```text
semantic store
  -> semantic action mask
  -> semantic tree node actions
  -> semantic_hash
```

The demo guards role-derived `activate` defaults for Button/ListItem, no-action defaults for Container/Text, explicit action override, and action participation in semantic tree hashes.

## 2026-05 Addendum: Semantic Intent Resolution

`Examples/ui/vivid/semantic_intent_demo` verifies Semantic Intent Resolution v0. It keeps action execution deliberately separate from address lookup:

```text
semantic tree root
  -> semantic id + action request
  -> SemanticIntentResolution
  -> status evidence
```

The demo guards resolved lookup, no input/callback side effects, rejected resolutions without input mutation, unsupported action, missing id, ambiguous duplicate id, disabled target, invalid root, and missing request id.

## 2026-05 Addendum: Semantic Action Request

`Examples/ui/vivid/semantic_action_request_demo` verifies Semantic Action Request v0. It is the first semantic action path that crosses from intent resolution into controlled execution:

```text
SemanticIntentResolution
  -> SemanticActionAdmission
  -> SemanticFocusRequest
  -> Click event evidence
  -> normal widget click behavior
```

The demo guards no-execute resolution, executed activate requests, focus/click event traces, already-focused execution without hidden transfer, unsupported-action rejection through action admission, active-scope rejection through focus admission, rejected paths with no focus or click event pollution, ambiguous duplicate ids, missing request ids, final causal-chain evidence, and `SemanticActionRequestLedger` / `reject_reason` evidence that names the failed boundary. Ledger rules are summarized in `vivid_semantic_request_ledger_law_v0.md`.

## 2026-05 Addendum: Intent-to-Artifact Evidence

`Examples/ui/vivid/intent_artifact_demo` connects semantic action request evidence to component render artifact evidence:

```text
SemanticActionRequest
  -> SemanticActionRequestLedger
  -> checked state delta
  -> paint_only invalidation intent
  -> dirty / DrawCmd evidence
  -> render artifact hash
```

The demo guards a committed `settings.wifi.toggle` semantic activate request, normal checkbox click behavior, bounded component dirty evidence, changed DrawCmd / pixel artifact summaries, and a disabled-target rejection path that leaves state and artifact unchanged. The stage law is summarized in `vivid_intent_to_artifact_evidence_v0.md`.

## 2026-05 Addendum: Semantic Action Admission

`Examples/ui/vivid/semantic_action_admission_demo` verifies Semantic Action Admission v0. It keeps action execution permission separate from actual input execution:

```text
semantic tree root
  -> semantic id + action request
  -> SemanticIntentResolution
  -> SemanticActionAdmission
  -> focus/click execution plan
```

The demo guards admitted activate plans, planning-only side effects, rejected admissions without focus/event/press mutation, planned focus/click evidence, unsupported action, disabled target, ambiguous duplicate id, missing id, invalid root, and missing request id.

## 2026-05 Addendum: Semantic Focus Query

`Examples/ui/vivid/semantic_focus_query_demo` verifies Semantic Focus Query v0. It keeps focus addressability separate from focus transfer:

```text
semantic tree root
  -> semantic id + active scope
  -> SemanticFocusQuery
  -> focus-addressability status
```

The demo guards resolved focus targets, no focus transfer side effects, rejected queries without focus/event mutation, non-focusable targets, disabled targets, active-scope rejection, ambiguous duplicate ids, missing ids, invalid root, and missing request id.

## 2026-05 Addendum: Semantic Focus Admission

`Examples/ui/vivid/semantic_focus_admission_demo` verifies Semantic Focus Admission v0. It keeps transfer permission separate from transfer execution:

```text
semantic tree root
  -> semantic id + current focus + active scope
  -> SemanticFocusAdmission
  -> transfer plan / rejection status
```

The demo guards admitted transfer plans, already-focused no-op plans, no focus transfer side effects, rejected admissions without focus/event mutation, non-focusable targets, disabled targets, active-scope rejection, ambiguous duplicate ids, missing ids, invalid root, and missing request id.

## 2026-05 Addendum: Semantic Focus Request

`Examples/ui/vivid/semantic_focus_request_demo` verifies Semantic Focus Request v0. It is the first semantic focus path that crosses from admission into controlled input execution:

```text
SemanticFocusQuery
  -> SemanticFocusAdmission
  -> SemanticFocusRequest
  -> input focus truth + FocusOut/FocusIn evidence
```

The demo guards committed transfer execution, event evidence, semantic focus truth after request, focus-ring artifact migration, rejection without artifact mutation, final causal-chain verdict, already-focused no-op, active-scope rejection, non-focusable targets, disabled targets, ambiguous duplicate ids, invalid root, missing request id, and `SemanticFocusRequestLedger` evidence that names the final request stage. Ledger rules are summarized in `vivid_semantic_request_ledger_law_v0.md`.
