# 系统文档入口

本目录收纳 Charm 的系统装配、启动流程、平台边界、minimal-kernel、POSIX 执行面、SSU，以及 artifact / explain surface 相关材料。

它不是第一次进入仓库时的总入口。建立整体认知时先读：

1. [`../overview.md`](../overview.md)
2. [`../architecture_overview.md`](../architecture_overview.md)
3. [`../README.md`](../README.md)

## 当前入口

| 目的 | 先看 |
|---|---|
| 系统装配、启动顺序、服务初始化 | [`init_graph_contract.md`](init_graph_contract.md)、[`service_component_init.md`](service_component_init.md) |
| 真实板级 system coordination / service snapshot | [`system_coordination_contract_v0.md`](system_coordination_contract_v0.md)、[`../architecture/real_board_landing_gap_audit_v0.md`](../architecture/real_board_landing_gap_audit_v0.md) |
| ARMv7-A / RK3506 平台边界 | [`armv7a_platform_contract.md`](armv7a_platform_contract.md)、[`../board/rk3506/README.md`](../board/rk3506/README.md) |
| ARMv7-A / QEMU seam smoke | [`run_qemu_arch_ingress_seam_ci.ps1`](../../Examples/kernel/armv7a/qemu/run_qemu_arch_ingress_seam_ci.ps1) |
| minimal-kernel runtime 总证据链 | [`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md)、[`minimal_kernel_runtime_ledger_fact_contract_v0.md`](minimal_kernel_runtime_ledger_fact_contract_v0.md) |
| minimal-kernel host smoke / 冷启动与热复用 | [`minimal_kernel_host_smoke_bundle_contract.md`](minimal_kernel_host_smoke_bundle_contract.md) |
| syscall / trap 链 | [`minimal_kernel_task_syscall_table_contract.md`](minimal_kernel_task_syscall_table_contract.md)、[`minimal_kernel_trap_syscall_contract.md`](minimal_kernel_trap_syscall_contract.md)、[`armv7a_runtime_trap_mapping_contract.md`](armv7a_runtime_trap_mapping_contract.md) |
| POSIX / Linux 用户态兼容 | [`posix_support_overview.md`](posix_support_overview.md) |
| system compiler exploration 的结果物与解释工具 | [`artifact_report_v0.md`](artifact_report_v0.md)、[`explain_surface_v0.md`](explain_surface_v0.md) |
| 资源约束探索与 bring-up 报告原型 | [`resource_contract_v0.md`](resource_contract_v0.md)、[`bringup_evidence_pipeline_v0.md`](bringup_evidence_pipeline_v0.md) |
| opening judgment / testimony 上层阅读通路 | [`opening_judgment_corridor_witness_taxonomy_v0.md`](opening_judgment_corridor_witness_taxonomy_v0.md) |
| 脚本面收敛 / evidence harness 治理 | [`script_surface_reduction_governance_v0.md`](script_surface_reduction_governance_v0.md)、[`script_surface_reduction_inventory_v0.md`](script_surface_reduction_inventory_v0.md) |
| schema 面收敛 / shared definition 候选治理 | [`schema_surface_reduction_governance_v0.md`](schema_surface_reduction_governance_v0.md)、[`schema_surface_reduction_inventory_v0.md`](schema_surface_reduction_inventory_v0.md) |
| `schemas/examples` sample hygiene / static refs smoke | [`schema_examples_hygiene_v0.md`](schema_examples_hygiene_v0.md)、[`../../scripts/schema_examples_hygiene_smoke.ps1`](../../scripts/schema_examples_hygiene_smoke.ps1) |

## 阅读规则

- `*_contract.md` / `*_overview.md` 优先视为当前入口或现行约束。
- `*_plan.md` / `*_roadmap.md` / `*_draft.md` 优先视为方向、迁移或讨论材料。
- `*_v0.md` / `*_review.md` / `*_summary.md` 优先视为阶段快照，需要结合当前代码一起读。
- `*_tasklist.md` / `*_checklist.md` 主要服务推进、验收和排期，不默认充当主题首页。

## Minimal-kernel 证据链补充入口

如果你正在追完整 syscall / trap / runtime session 证据链，再顺着读：

- [`minimal_kernel_runtime_bridge_contract.md`](minimal_kernel_runtime_bridge_contract.md)
- [`kernel_runtime_session_witness_v0.md`](kernel_runtime_session_witness_v0.md)
- [`minimal_kernel_runtime_ledger_fact_contract_v0.md`](minimal_kernel_runtime_ledger_fact_contract_v0.md)
- [`minimal_kernel_runtime_evidence_matrix.md`](minimal_kernel_runtime_evidence_matrix.md)
- [`minimal_kernel_task_syscall_table_contract.md`](minimal_kernel_task_syscall_table_contract.md)
- [`minimal_kernel_trap_syscall_contract.md`](minimal_kernel_trap_syscall_contract.md)
- [`minimal_kernel_trap_ingress_contract.md`](minimal_kernel_trap_ingress_contract.md)
- [`armv7a_runtime_trap_mapping_contract.md`](armv7a_runtime_trap_mapping_contract.md)
- [`minimal_kernel_task_message_runtime_contract.md`](minimal_kernel_task_message_runtime_contract.md)
- [`ci_minimal_kernel_runtime_session_witness_smoke.ps1`](../../scripts/ci_minimal_kernel_runtime_session_witness_smoke.ps1)
- [`inspect_minimal_kernel_runtime_session_witness_smoke.ps1`](../../scripts/inspect_minimal_kernel_runtime_session_witness_smoke.ps1)
- [`inspect_minimal_kernel_runtime_session_witness_smoke_compare_smoke.ps1`](../../scripts/inspect_minimal_kernel_runtime_session_witness_smoke_compare_smoke.ps1)
- [`inspect_minimal_kernel_runtime_session_witness_compare_summary_smoke.ps1`](../../scripts/inspect_minimal_kernel_runtime_session_witness_compare_summary_smoke.ps1)
- [`system_compiler_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1`](../../scripts/system_compiler_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1)
- [`minimal-kernel-runtime-session-witness.yml`](../../.github/workflows/minimal-kernel-runtime-session-witness.yml)

早期 bridge/mailbox/service/task API 分篇讨论已收敛，独立取舍与未实施方向见
[`../archive/minimal-kernel-runtime-v0/README.md`](../archive/minimal-kernel-runtime-v0/README.md)。

materialized graph、bring-up evidence、resource contract 与 ARMv7-A staging 的完整阶段叙事已移入
[`../archive/system-evidence-and-staging-v0/README.md`](../archive/system-evidence-and-staging-v0/README.md)；现行路径只保留实现边界或探索状态。

## POSIX 补充入口

先读 [`posix_support_overview.md`](posix_support_overview.md)。按需要继续：

- 分层边界：[`posix_three_layer_contract.md`](posix_three_layer_contract.md)
- 子主题契约：[`posix_program_image_contract.md`](posix_program_image_contract.md)、[`posix_spawn_contract.md`](posix_spawn_contract.md)、[`posix_fd_table_contract.md`](posix_fd_table_contract.md)、[`posix_user_runtime_contract.md`](posix_user_runtime_contract.md)、[`posix_error_semantics.md`](posix_error_semantics.md)

POSIX v0 的阶段基线、roadmap、任务清单与维护记录见
[`../archive/posix-v0/README.md`](../archive/posix-v0/README.md)，不作为当前状态入口。

## System compiler 补充入口

如果你在追 front-page / opening-flow / biography / world / witness 历史链，不要从本目录旧路径查找；先进入归档目录：

- [`../archive/system-compiler-front-page-v0/README.md`](../archive/system-compiler-front-page-v0/README.md)

当前仍保留的 opening corridor smoke 入口：

- [`system_compiler_front_page_entry_world_shelf_review_opening_corridor_smoke.ps1`](../../scripts/system_compiler_front_page_entry_world_shelf_review_opening_corridor_smoke.ps1)
- [`system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_bridge_smoke.ps1`](../../scripts/system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_bridge_smoke.ps1)
- [`system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_bridge_blocked_smoke.ps1`](../../scripts/system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_bridge_blocked_smoke.ps1)
- [`system_compiler_front_page_entry_runtime_session_open_event_witness_smoke.ps1`](../../scripts/system_compiler_front_page_entry_runtime_session_open_event_witness_smoke.ps1)
- [`system_compiler_front_page_entry_opener_open_event_witness_smoke.ps1`](../../scripts/system_compiler_front_page_entry_opener_open_event_witness_smoke.ps1)

当前 system compiler / front-page schema 面治理入口：

- [`schema_surface_reduction_governance_v0.md`](schema_surface_reduction_governance_v0.md)
- [`schema_surface_reduction_inventory_v0.md`](schema_surface_reduction_inventory_v0.md)
- [`schema_examples_hygiene_v0.md`](schema_examples_hygiene_v0.md)

## SSU 补充入口

- [`ssu_contract.md`](ssu_contract.md)
- [`ssu_review_checklist.md`](ssu_review_checklist.md)

SSU 是 kernel/scheduler 的 supporting 实现机制，不是 Charm Core 主线或应用模型。
历史推进记录见 [`../archive/ssu-phase-notes/README.md`](../archive/ssu-phase-notes/README.md)。

## 阶段材料

front-page、opening-flow、witness、biography、world compare 等 system compiler 阶段材料不再逐篇列入默认阅读路径。

如果需要追溯这条历史链，先从这些上位文档进入：

- [`../architecture/system_compiler_roadmap.md`](../architecture/system_compiler_roadmap.md)
- [`../architecture/system_compiler_vocabulary_v0.md`](../architecture/system_compiler_vocabulary_v0.md)
- [`artifact_report_v0.md`](artifact_report_v0.md)
- [`explain_surface_v0.md`](explain_surface_v0.md)
- [`../archive/system-compiler-front-page-v0/README.md`](../archive/system-compiler-front-page-v0/README.md)

## 暂时不要怎么读

- 不要第一次进 `docs/system/` 就从 `plan / draft / review / checklist` 开始。
- 不要把 `v0`、阶段总结或 tasklist 直接当成已经冻结的长期契约。
- 不要把归档材料重新抬回 `docs/system/README.md` 的默认首读路径。
- 如果 system 文档和当前代码、目录入口、构建入口明显不一致，优先回到上位入口重新确认。
