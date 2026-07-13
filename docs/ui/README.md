# UI 文档入口

## 文档状态

- `status`: `supporting`
- `scope`: UI、Vivid、Player UI 与 EInk 专题路由
- `authority`: [`ui_kernel_contract.md`](ui_kernel_contract.md)

本文只提供下一跳，不复制 demo case、stdout token、测试数量或阶段状态。当前行为必须由专题契约、
源码、CMake/CTest 和当次证据共同确认。

## 按任务进入

| 任务 | 入口 |
|---|---|
| UI ownership、状态提交、布局与渲染硬规则 | [`ui_kernel_contract.md`](ui_kernel_contract.md) |
| Vivid source 分层、渲染、catalog 与静态内存 | [`Modules/ui/vivid/ARCHITECTURE.md`](../../Modules/ui/vivid/ARCHITECTURE.md) |
| 结构化视图与 widget 状态观察 | [`structured_view_model_v1.md`](structured_view_model_v1.md)、[`vivid_widget_state_observe.md`](vivid_widget_state_observe.md) |
| Vivid runtime 范围与 backend 边界 | [`vivid_runtime_charter.md`](vivid_runtime_charter.md)、[`vivid_import_boundary_contract.md`](vivid_import_boundary_contract.md)、[`vivid_multibackend_plan.md`](vivid_multibackend_plan.md) |
| replay 与 evidence 总入口 | [`vivid_replay_workflow.md`](vivid_replay_workflow.md)、[`vivid_evidence_lab_manifest_v0.md`](vivid_evidence_lab_manifest_v0.md)、[`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md) |
| render/draw/scene evidence | [`vivid_render_evidence_chain_v0.md`](vivid_render_evidence_chain_v0.md)、[`vivid_draw_cmd_evidence_boundary_v0.md`](vivid_draw_cmd_evidence_boundary_v0.md)、[`vivid_soa_table_tree_evidence_law_v0.md`](vivid_soa_table_tree_evidence_law_v0.md)、[`vivid_scene_support_boundary_v0.md`](vivid_scene_support_boundary_v0.md) |
| verdict、artifact 与 evidence vocabulary | [`vivid_causal_verdict_law_v0.md`](vivid_causal_verdict_law_v0.md)、[`vivid_evidence_artifact_promotion_v0.md`](vivid_evidence_artifact_promotion_v0.md)、[`vivid_evidence_vocabulary_law_v0.md`](vivid_evidence_vocabulary_law_v0.md)、[`vivid_intent_to_artifact_evidence_v0.md`](vivid_intent_to_artifact_evidence_v0.md) |
| style、layer 与 motion | [`vivid_style_token_law_v0.md`](vivid_style_token_law_v0.md)、[`vivid_layer_runtime_v0.md`](vivid_layer_runtime_v0.md)、[`vivid_motion_runtime_v0.md`](vivid_motion_runtime_v0.md)、[`vivid_page_layer_style_patch.md`](vivid_page_layer_style_patch.md) |
| focus 行为与证据 | [`vivid_focus_evidence_boundary_v0.md`](vivid_focus_evidence_boundary_v0.md)、[`vivid_focus_transfer_evidence_v0.md`](vivid_focus_transfer_evidence_v0.md)、[`vivid_focus_scope_evidence_v0.md`](vivid_focus_scope_evidence_v0.md)、[`vivid_focus_semantic_evidence_v0.md`](vivid_focus_semantic_evidence_v0.md) |
| semantic request 与 transition | [`vivid_semantic_request_ledger_law_v0.md`](vivid_semantic_request_ledger_law_v0.md)、[`vivid_semantic_transition_law_v0.md`](vivid_semantic_transition_law_v0.md)、[`vivid_semantic_transition_evidence_v0.md`](vivid_semantic_transition_evidence_v0.md)、[`vivid_semantic_action_state_transition_law_v0.md`](vivid_semantic_action_state_transition_law_v0.md)、[`vivid_semantic_action_state_transition_evidence_v0.md`](vivid_semantic_action_state_transition_evidence_v0.md) |
| widget spec、静态内存与产品配置 | [`vivid_widget_spec_reflection_v0.md`](vivid_widget_spec_reflection_v0.md)、[`vivid_static_memory_admission.md`](vivid_static_memory_admission.md) |
| 显示热键 | [`vivid_display_hotkeys.md`](vivid_display_hotkeys.md) |
| EInk refresh | [`eink_refresh_policy.md`](eink_refresh_policy.md) |
| Player UI 与 Vivid pattern | [`player_ui.md`](player_ui.md)、[`player_vivid_patterns.md`](player_vivid_patterns.md) |
| Player portability 与 provider 边界 | [`player_portability_boundary.md`](player_portability_boundary.md) |

## Evidence 入口

Evidence Lab 的 case、tag、axis 和推荐样本由
[`vivid_evidence_lab_manifest_v0.md`](vivid_evidence_lab_manifest_v0.md) 维护；README 不再逐项复制。
快速漂移检查：

```powershell
./scripts/vivid_evidence_lab_manifest_smoke.ps1
```

示例按用途从 [`../../Examples/ui/README.md`](../../Examples/ui/README.md) 进入。`soa_demo`、focus、
semantic、motion 和 component demos 只能证明各自 fixture 覆盖的语义，不能替代产品或实板证据。

## 阅读与维护

1. 先读 UI kernel contract。
2. 再读目标 runtime/topic 的专题契约。
3. 需要行为证据时检查 manifest、对应 sample source、CMake/CTest 和当次日志。
4. backend、Player 或实板接线只引用 UI 稳定行为，不把局部 workaround 写回 UI contract。

新增推荐入口时更新本表；case 数、stdout 细节和阶段进度留在 manifest、测试或专题文档。Host 样本、
QEMU 与真实板属于不同证据域，不能互相替代。
