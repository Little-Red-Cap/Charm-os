# UI 文档入口

2026-05 补记：`Examples/ui/vivid/focus_boundary_demo` 是 Focus Evidence Boundary v0 的最小运行样本，验证 focused 不进入普通 Button style mask，但通过 focus ring 改变 render artifact，并用 final causal verdict 收口；法律见 `vivid_focus_evidence_boundary_v0.md`，stdout 见 `vivid_evidence_stdout_law.md`。

2026-05 补记：`Examples/ui/vivid/focus_transfer_demo` 是 Focus Transfer Evidence v0 的最小运行样本，验证 `FocusOut(old)` / `FocusIn(new)`、`input_focused` truth 提交、focus ring artifact 迁移，以及 final `causal_chain` verdict；法律见 `vivid_focus_transfer_evidence_v0.md`。

2026-05 补记：`Examples/ui/vivid/focus_scope_demo` 是 Focus Scope Evidence v0 的最小运行样本，验证 scope 内请求通过真实 input dispatch 迁移焦点、scope 外请求被 runtime focus admission 拒绝、input focus truth 保持在 fallback/current、focus ring artifact 不泄漏到 scope 外 target，并由 final `causal_chain` 收束；法律见 `vivid_focus_scope_evidence_v0.md`。

2026-05 补记：`Examples/ui/vivid/focus_scope_nested_demo` 是 Focus Scope Nested Evidence v0 的最小运行样本，验证 base scope push modal scope、modal trap current-first、pop 恢复 base scope、嵌套/弹窗场景焦点不泄漏，以及 final `causal_chain` verdict；法律见 `vivid_focus_scope_evidence_v0.md`。

2026-05 补记：`Examples/ui/vivid/focus_scope_navigation_demo` 是 Focus Scope Navigation Evidence v0 的最小运行样本，验证 Tab/Right/Down 前进、Left/Up 后退、scope 内循环、scope 外 target 不参与键盘焦点导航，以及 final `causal_chain` verdict；法律见 `vivid_focus_scope_evidence_v0.md`。

2026-05 补记：`Examples/ui/vivid/focus_spatial_navigation_demo` 是 Focus Spatial Navigation Evidence v0 的最小运行样本，验证方向键优先按几何方向选择 active scope 内候选、无候选时回退 preorder wrap、scope 外 target 不参与 spatial navigation，以及 final `causal_chain` verdict；法律见 `vivid_focus_scope_evidence_v0.md`。

2026-05 补记：`Examples/ui/vivid/focus_semantic_demo` 是 Focus Semantic Evidence v0 的最小运行样本，验证 runtime semantic store、`input_focused` truth、visual focus ring artifact 对齐，以及 final `causal_chain` verdict；法律见 `vivid_focus_semantic_evidence_v0.md`。

本目录收纳 Charm UI 语义、布局、渲染、热键和多后端规划相关材料。

如果你是第一次进入仓库，先读：

1. [`../overview.md`](../overview.md)
2. [`../architecture_overview.md`](../architecture_overview.md)
3. [`../README.md`](../README.md)

再回到这里按任务进入。

## 按任务进入

### 我想看 UI 的现行硬规则

先读：

- [`ui_kernel_contract.md`](ui_kernel_contract.md)

### 我想看 UI 的结构化视图与状态表达

先读：

- [`structured_view_model_v1.md`](structured_view_model_v1.md)
- [`vivid_widget_state_observe.md`](vivid_widget_state_observe.md)
- `Examples/ui/vivid/list_row_flags_demo`

### 我想看 Vivid 这条线

建议顺序：

1. [`vivid_runtime_charter.md`](vivid_runtime_charter.md)
2. [`vivid_multibackend_plan.md`](vivid_multibackend_plan.md)
3. [`vivid_replay_workflow.md`](vivid_replay_workflow.md)
4. [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)
5. [`vivid_evidence_lab_manifest_v0.md`](vivid_evidence_lab_manifest_v0.md)
6. [`vivid_render_evidence_chain_v0.md`](vivid_render_evidence_chain_v0.md)
7. [`vivid_evidence_artifact_promotion_v0.md`](vivid_evidence_artifact_promotion_v0.md)
8. [`vivid_evidence_vocabulary_law_v0.md`](vivid_evidence_vocabulary_law_v0.md)
9. [`vivid_intent_to_artifact_evidence_v0.md`](vivid_intent_to_artifact_evidence_v0.md)
10. [`vivid_style_token_law_v0.md`](vivid_style_token_law_v0.md)
11. [`vivid_layer_runtime_v0.md`](vivid_layer_runtime_v0.md)
12. [`vivid_motion_runtime_v0.md`](vivid_motion_runtime_v0.md)
13. [`vivid_display_hotkeys.md`](vivid_display_hotkeys.md)
14. [`vivid_focus_evidence_boundary_v0.md`](vivid_focus_evidence_boundary_v0.md)
15. [`vivid_focus_transfer_evidence_v0.md`](vivid_focus_transfer_evidence_v0.md)
16. [`vivid_focus_scope_evidence_v0.md`](vivid_focus_scope_evidence_v0.md)
17. [`vivid_focus_semantic_evidence_v0.md`](vivid_focus_semantic_evidence_v0.md)
18. [`vivid_semantic_request_ledger_law_v0.md`](vivid_semantic_request_ledger_law_v0.md)
19. [`vivid_page_layer_style_patch.md`](vivid_page_layer_style_patch.md)

Semantic Tree Artifact v0 runtime sample:

- `Examples/ui/vivid/semantic_tree_demo`: verifies root-bound semantic preorder collection, focus marker, decorative exclusion, overflow reporting, stable `semantic_hash`, and final causal verdict; stdout follows [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md), guarded by CTest final `result=ok cases=7`.
- `Examples/ui/vivid/semantic_default_demo`: verifies opt-in pattern semantic defaults, default role / label source, explicit override, decorative boundary, semantic tree integration, and final causal verdict; stdout follows [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md), guarded by CTest final `result=ok cases=7`.
- `Examples/ui/vivid/semantic_action_demo`: verifies Semantic Action Artifact v0, including role-derived `activate`, no-action semantic roles, explicit action override, tree action artifacts, action participation in `semantic_hash`, and final causal verdict; stdout follows [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md), guarded by CTest final `result=ok cases=7`.
- `Examples/ui/vivid/semantic_intent_demo`: verifies Semantic Intent Resolution v0, including root-bound id/action lookup, no-execute side effects, rejected resolutions without input mutation, unsupported action, missing id, ambiguous duplicate id, disabled target, and invalid request statuses; stdout follows [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md), guarded by CTest final `result=ok cases=8`.
- `Examples/ui/vivid/semantic_action_admission_demo`: verifies Semantic Action Admission v0, including intent resolution, focus/click execution planning, planning-only side effects, rejected admissions without focus/event/press mutation, unsupported action, disabled target, duplicate id, and invalid request statuses; stdout follows [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md), guarded by CTest final `result=ok cases=8`.
- `Examples/ui/vivid/semantic_action_request_demo`: verifies Semantic Action Request v0, including controlled semantic activate execution, focus preparation event trace, emitted click evidence, unsupported/focus-scope rejection without focus or click event pollution, duplicate id, missing id, request ledger evidence, and final causal-chain evidence; stdout follows [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md), guarded by CTest final `result=ok cases=11`.
- `Examples/ui/vivid/semantic_focus_query_demo`: verifies Semantic Focus Query v0, including focus-addressable ids, no focus transfer side effects, rejected queries without focus/event mutation, non-focusable targets, disabled targets, active-scope rejection, ambiguous duplicate id, and invalid request statuses; stdout follows [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md), guarded by CTest final `result=ok cases=8`.
- `Examples/ui/vivid/semantic_focus_admission_demo`: verifies Semantic Focus Admission v0, including admitted transfer plans, already-focused no-op plans, no focus-transfer side effects, rejected admissions without focus/event mutation, non-focusable and disabled rejection, active-scope rejection, ambiguous duplicate id, and invalid request statuses; stdout follows [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md), guarded by CTest final `result=ok cases=8`.
- `Examples/ui/vivid/semantic_focus_request_demo`: verifies Semantic Focus Request v0, including controlled focus transfer execution, `FocusOut/FocusIn` event evidence, semantic focus truth after request, request-driven focus artifact evidence, rejection without artifact mutation, final causal-chain evidence, already-focused no-op, active-scope rejection, non-focusable and disabled rejection, ambiguous duplicate id, and invalid request statuses; stdout follows [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md), guarded by CTest final `result=ok cases=12`.
- `Examples/ui/vivid/intent_artifact_demo`: verifies Intent-to-Artifact Evidence v0, including semantic action request ledger, checked state delta, paint-only invalidation intent, bounded dirty evidence, changed render artifact, disabled rejection with no state/artifact mutation, and final causal chain summary; stdout follows [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md), guarded by CTest final `result=ok cases=9`.
- `Examples/ui/vivid/evidence_vocabulary_demo`: verifies Evidence Vocabulary Law v0 field shape and helper-derived verdicts for state delta, invalidation, render evidence, artifact delta, and causal chain; stdout follows [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md), guarded by CTest final `result=ok cases=5`.
- `Examples/ui/vivid/evidence_lab_manifest_demo`: verifies Evidence Lab Manifest v0, including stable run/tag/cases registry shape, unique stdout tags, axis coverage, Intent-to-Artifact anchor, vocabulary anchor, stdout law sync, CMake gate sync, doc route sync, and promotion boundary; stdout follows [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md), guarded by CTest final `result=ok cases=9`.

Fast manifest smoke:

```powershell
./scripts/vivid_evidence_lab_manifest_smoke.ps1
```

最小验证示例：

- `Examples/ui/vivid/widget_signal_demo`：验证 object-level widget click edge 与旧回调兼容；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=3`。
- `Examples/ui/vivid/widget_state_demo`：验证 object-level widget state truth 与 observe 语义；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=5`。
- `Examples/ui/vivid/component_settings_row_demo`：验证 component 级 state truth → render evidence chain → causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=5`。
- `Examples/ui/vivid/component_card_state_demo`：验证多 child state 汇入同一个 component artifact 与 causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=6`。
- `Examples/ui/vivid/style_token_law_demo`：验证 semantic token、style state mask、paint-only impact、render artifact evidence 与 causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=7`。
- `Examples/ui/vivid/focus_scope_demo`：验证 FocusScope 接入真实 input dispatch 后允许 scope 内迁移、拒绝 scope 外焦点提交，并证明 scope 外 target 不接收 focus ring artifact 与 final causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=10`。
- `Examples/ui/vivid/focus_scope_nested_demo`：验证 FocusScope push/pop 后 modal scope 与 base scope 的焦点收尾律与 final causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=9`。
- `Examples/ui/vivid/focus_scope_navigation_demo`：验证 keyboard / d-pad focus navigation 在 active scope 内按顺序循环与 final causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=8`。
- `Examples/ui/vivid/focus_spatial_navigation_demo`：验证 directional key 在 active scope 内优先按 world rect 选择 spatial candidate 与 final causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=10`。
- `Examples/ui/vivid/focus_semantic_demo`：验证 runtime semantic store、input focus truth、visual focus ring artifact 对齐与 final causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=9`。
- `Examples/ui/vivid/motion_time_demo`：验证 Managed UI Time、motion recipe、transition trace 与 compose dry-run。

### 我想看 EInk / Player UI

读：

- [`eink_refresh_policy.md`](eink_refresh_policy.md)
- [`player_ui.md`](player_ui.md)
- [`player_vivid_patterns.md`](player_vivid_patterns.md)

## 当前建议阅读顺序

- UI 硬规则：`ui_kernel_contract.md`
- 结构化视图：`structured_view_model_v1.md`
- Vivid 路线：`vivid_runtime_charter.md` → `vivid_multibackend_plan.md` → `vivid_replay_workflow.md` → `vivid_evidence_stdout_law.md` → `vivid_render_evidence_chain_v0.md` → `vivid_style_token_law_v0.md` → `vivid_layer_runtime_v0.md` → `vivid_motion_runtime_v0.md`
- Player/UI 组合：`player_ui.md` → `player_vivid_patterns.md`

## 使用提醒

- UI 这条线既有契约，也有项目化样例和阶段性补丁，不要把它们混成一层。
- 当输入、状态提交、布局影响位、渲染 record/execute 边界变化时，应同步更新这里的入口。

## 2026-05 Vivid runtime 验证入口补记

- `Examples/ui/vivid/motion_time_demo`：验证 Managed UI Time、Motion recipe、transition trace、compose dry-run 与单页 `PageMotionTransition`；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=12`。
- `Examples/ui/vivid/page_transition_demo`：验证双页 `PageTransitionRunner`、`fade_slide` recipe 与 Cheap profile 量化；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=15`；矩阵见 `docs/ui/vivid_motion_runtime_v0.md`。
- `Examples/ui/vivid/component_settings_row_demo`：验证 settings row component 的 state truth、invalidation intent、dirty evidence、draw command evidence、render artifact evidence 与 causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=5`；链路见 [`vivid_render_evidence_chain_v0.md`](vivid_render_evidence_chain_v0.md)。
- `Examples/ui/vivid/component_card_state_demo`：验证 card component 中多 child state、derived output、summary text、同一 render artifact 与 causal verdict 的因果关系；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=6`；链路见 [`vivid_render_evidence_chain_v0.md`](vivid_render_evidence_chain_v0.md)。
- `Examples/ui/vivid/style_token_law_demo`：验证 Style Token Law v0 的 semantic token、role patch、state mask、paint-only impact、artifact 变化与 causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=7`；法律见 [`vivid_style_token_law_v0.md`](vivid_style_token_law_v0.md)。
- `Examples/ui/vivid/focus_scope_demo`：验证 Focus Scope Evidence v0 的 scope containment、runtime scope install、inside dispatch allow、outside dispatch reject、fallback input truth、no-leak artifact 与 final causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=10`；法律见 [`vivid_focus_scope_evidence_v0.md`](vivid_focus_scope_evidence_v0.md)。
- `Examples/ui/vivid/focus_scope_nested_demo`：验证 Focus Scope Nested Evidence v0 的 base scope、modal scope push/pop、modal trap current-first、restored base fallback 与 final causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=9`；法律见 [`vivid_focus_scope_evidence_v0.md`](vivid_focus_scope_evidence_v0.md)。
- `Examples/ui/vivid/focus_scope_navigation_demo`：验证 Focus Scope Navigation Evidence v0 的 Tab / directional key focus move、forward/reverse wrap、outside target exclusion 与 final causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=8`；法律见 [`vivid_focus_scope_evidence_v0.md`](vivid_focus_scope_evidence_v0.md)。
- `Examples/ui/vivid/focus_spatial_navigation_demo`：验证 Focus Spatial Navigation Evidence v0 的 spatial candidate、preorder fallback、Tab preorder、outside target exclusion 与 final causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=10`；法律见 [`vivid_focus_scope_evidence_v0.md`](vivid_focus_scope_evidence_v0.md)。
- `Examples/ui/vivid/focus_semantic_demo`：验证 Focus Semantic Evidence v0 的 stable semantic id / role / label、semantic current target、input focus truth、focus ring artifact 对齐与 final causal verdict；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=9`；法律见 [`vivid_focus_semantic_evidence_v0.md`](vivid_focus_semantic_evidence_v0.md)。
- `Examples/ui/vivid/widget_signal_demo`：验证 Button / MenuItem / ListItem 的 object-level click edge；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=3`。
- `Examples/ui/vivid/widget_state_demo`：验证 Checkbox / Dropdown / Slider / ProgressBarSimple / Arc 的 object-level state truth；stdout 遵守 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)，并由 CTest 约束最终 `result=ok cases=5`。
