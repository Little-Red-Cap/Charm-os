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
| system compiler 结果物与解释面 | [`artifact_report_v0.md`](artifact_report_v0.md)、[`explain_surface_v0.md`](explain_surface_v0.md) |
| 资源法律与 bring-up 证据 | [`resource_contract_v0.md`](resource_contract_v0.md)、[`bringup_evidence_pipeline_v0.md`](bringup_evidence_pipeline_v0.md) |
| opening judgment / testimony 上层阅读通路 | [`opening_judgment_corridor_witness_taxonomy_v0.md`](opening_judgment_corridor_witness_taxonomy_v0.md) |

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
- [`minimal_kernel_runtime_mailbox_contract.md`](minimal_kernel_runtime_mailbox_contract.md)
- [`minimal_kernel_trap_ingress_contract.md`](minimal_kernel_trap_ingress_contract.md)
- [`minimal_kernel_task_syscall_catalog_contract.md`](minimal_kernel_task_syscall_catalog_contract.md)
- [`minimal_kernel_task_syscall_dispatch_contract.md`](minimal_kernel_task_syscall_dispatch_contract.md)
- [`minimal_kernel_task_syscall_frame_contract.md`](minimal_kernel_task_syscall_frame_contract.md)（当前存在历史编码损坏，待恢复，不作为首选入口）
- [`minimal_kernel_task_message_api_contract.md`](minimal_kernel_task_message_api_contract.md)
- [`minimal_kernel_task_message_table_contract.md`](minimal_kernel_task_message_table_contract.md)
- [`minimal_kernel_task_message_dispatch_contract.md`](minimal_kernel_task_message_dispatch_contract.md)
- [`minimal_kernel_task_message_service_loop_contract.md`](minimal_kernel_task_message_service_loop_contract.md)
- [`minimal_kernel_task_message_service_drain_contract.md`](minimal_kernel_task_message_service_drain_contract.md)
- [`minimal_kernel_task_message_service_pump_contract.md`](minimal_kernel_task_message_service_pump_contract.md)
- [`minimal_kernel_task_message_syscall_bridge_contract.md`](minimal_kernel_task_message_syscall_bridge_contract.md)
- [`minimal_kernel_task_message_syscall_frame_contract.md`](minimal_kernel_task_message_syscall_frame_contract.md)
- [`minimal_kernel_task_message_syscall_frame_caller_contract.md`](minimal_kernel_task_message_syscall_frame_caller_contract.md)
- [`minimal_kernel_task_message_syscall_client_contract.md`](minimal_kernel_task_message_syscall_client_contract.md)
- [`minimal_kernel_task_message_syscall_pump_contract.md`](minimal_kernel_task_message_syscall_pump_contract.md)
- [`minimal_kernel_task_message_runtime_service_contract.md`](minimal_kernel_task_message_runtime_service_contract.md)
- [`minimal_kernel_task_message_runtime_api_contract.md`](minimal_kernel_task_message_runtime_api_contract.md)
- [`minimal_kernel_task_message_syscall_api_contract.md`](minimal_kernel_task_message_syscall_api_contract.md)
- [`minimal_kernel_task_message_session_api_contract.md`](minimal_kernel_task_message_session_api_contract.md)
- [`minimal_kernel_task_message_session_dispatch_contract.md`](minimal_kernel_task_message_session_dispatch_contract.md)
- [`minimal_kernel_task_message_session_acceptor_contract.md`](minimal_kernel_task_message_session_acceptor_contract.md)
- [`minimal_kernel_task_message_session_endpoint_contract.md`](minimal_kernel_task_message_session_endpoint_contract.md)
- [`minimal_kernel_task_message_session_protocol_contract.md`](minimal_kernel_task_message_session_protocol_contract.md)
- [`minimal_kernel_task_message_session_protocol_schema_contract.md`](minimal_kernel_task_message_session_protocol_schema_contract.md)
- [`minimal_kernel_task_message_session_service_contract.md`](minimal_kernel_task_message_session_service_contract.md)
- [`minimal_kernel_task_message_session_service_loop_contract.md`](minimal_kernel_task_message_session_service_loop_contract.md)
- [`minimal_kernel_task_message_session_roundtrip_contract.md`](minimal_kernel_task_message_session_roundtrip_contract.md)
- [`minimal_kernel_runtime_service_contract.md`](minimal_kernel_runtime_service_contract.md)
- [`minimal_kernel_task_runtime_api_contract.md`](minimal_kernel_task_runtime_api_contract.md)
- [`minimal_kernel_task_syscall_api_contract.md`](minimal_kernel_task_syscall_api_contract.md)
- [`ci_minimal_kernel_runtime_session_witness_smoke.ps1`](../../scripts/ci_minimal_kernel_runtime_session_witness_smoke.ps1)
- [`inspect_minimal_kernel_runtime_session_witness_smoke.ps1`](../../scripts/inspect_minimal_kernel_runtime_session_witness_smoke.ps1)
- [`inspect_minimal_kernel_runtime_session_witness_smoke_compare_smoke.ps1`](../../scripts/inspect_minimal_kernel_runtime_session_witness_smoke_compare_smoke.ps1)
- [`inspect_minimal_kernel_runtime_session_witness_compare_summary_smoke.ps1`](../../scripts/inspect_minimal_kernel_runtime_session_witness_compare_summary_smoke.ps1)
- [`system_compiler_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1`](../../scripts/system_compiler_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1)
- [`minimal-kernel-runtime-session-witness.yml`](../../.github/workflows/minimal-kernel-runtime-session-witness.yml)

## POSIX 补充入口

先读 [`posix_support_overview.md`](posix_support_overview.md)。按需要继续：

- 第一阶段基线：[`posix_elf_stage1_baseline.md`](posix_elf_stage1_baseline.md)
- 路线与维护：[`posix_compat_roadmap.md`](posix_compat_roadmap.md)、[`posix_maintenance_mode_collaboration.md`](posix_maintenance_mode_collaboration.md)、[`posix_stage_summary.md`](posix_stage_summary.md)
- 推进与验收：[`posix_linux_compat_tasklist.md`](posix_linux_compat_tasklist.md)、[`posix_v0_closure_checklist.md`](posix_v0_closure_checklist.md)、[`posix_busybox_phase_checklist.md`](posix_busybox_phase_checklist.md)
- 子主题设计：[`posix_spawn_minimal_design.md`](posix_spawn_minimal_design.md)、[`posix_fd_table_minimal_design.md`](posix_fd_table_minimal_design.md)、[`posix_errno_mapping.md`](posix_errno_mapping.md)、[`posix_error_semantics.md`](posix_error_semantics.md)

## System compiler 补充入口

如果你在追 front-page / opening-flow / biography / world / witness 历史链，不要从本目录旧路径查找；先进入归档目录：

- [`../archive/system-compiler-front-page-v0/README.md`](../archive/system-compiler-front-page-v0/README.md)

当前仍保留的 opening corridor smoke 入口：

- [`system_compiler_front_page_entry_world_shelf_review_opening_corridor_smoke.ps1`](../../scripts/system_compiler_front_page_entry_world_shelf_review_opening_corridor_smoke.ps1)
- [`system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_bridge_smoke.ps1`](../../scripts/system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_bridge_smoke.ps1)
- [`system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_bridge_blocked_smoke.ps1`](../../scripts/system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_bridge_blocked_smoke.ps1)
- [`system_compiler_front_page_entry_runtime_session_open_event_witness_smoke.ps1`](../../scripts/system_compiler_front_page_entry_runtime_session_open_event_witness_smoke.ps1)
- [`system_compiler_front_page_entry_opener_open_event_witness_smoke.ps1`](../../scripts/system_compiler_front_page_entry_opener_open_event_witness_smoke.ps1)

## SSU 补充入口

- [`ssu_status.md`](ssu_status.md)
- [`ssu_contract.md`](ssu_contract.md)
- [`ssu_discipline.md`](ssu_discipline.md)
- [`ssu_review_checklist.md`](ssu_review_checklist.md)
- [`ssu_submit_discipline.md`](ssu_submit_discipline.md)
- [`ssu_submit_inventory.md`](ssu_submit_inventory.md)
- [`ssu_migration_priority.md`](ssu_migration_priority.md)
- [`ssu_vnext.md`](ssu_vnext.md)

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
