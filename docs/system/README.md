# 系统文档入口

本目录收纳 Charm 的系统装配、启动流程、平台边界、最小内核、POSIX 执行面、SSU，以及 artifact / explain surface 相关材料。

它不是第一次进入仓库时的总入口。  
如果你还是在建立整体认知，先读：

1. [`../overview.md`](../overview.md)
2. [`../architecture_overview.md`](../architecture_overview.md)
3. [`../README.md`](../README.md)

## 先怎么判断一篇 system 文档值不值得先读

- `*_contract.md` / `*_overview.md`
  优先视为当前入口或现行约束。
- `*_plan.md` / `*_roadmap.md` / `*_draft.md`
  优先视为方向、迁移或讨论材料。
- `*_v0.md` / `*_review.md` / `*_summary.md`
  优先视为阶段快照或阶段收口，需要结合当前代码一起读。
- `*_tasklist.md` / `*_checklist.md`
  主要服务推进、验收和排期，不默认充当主题首页。

## 按任务进入

### 我在看系统装配和启动边界

先读：

- [`init_graph_contract.md`](init_graph_contract.md)
- [`service_component_init.md`](service_component_init.md)
- [`armv7a_platform_contract.md`](armv7a_platform_contract.md)
- 如果同时涉及板级 bring-up，再读 [`../board/README.md`](../board/README.md)

### 我在看 ARMv7-A / RK3506 / 最小内核

先读：

- [`armv7a_platform_contract.md`](armv7a_platform_contract.md)
- [`minimal_kernel_runtime_bridge_contract.md`](minimal_kernel_runtime_bridge_contract.md)
- [`minimal_kernel_runtime_evidence_matrix.md`](minimal_kernel_runtime_evidence_matrix.md)
- [`minimal_kernel_runtime_mailbox_contract.md`](minimal_kernel_runtime_mailbox_contract.md)
- [`minimal_kernel_task_message_api_contract.md`](minimal_kernel_task_message_api_contract.md)
- [`minimal_kernel_task_message_table_contract.md`](minimal_kernel_task_message_table_contract.md)
- [`minimal_kernel_runtime_service_contract.md`](minimal_kernel_runtime_service_contract.md)
- [`minimal_kernel_task_runtime_api_contract.md`](minimal_kernel_task_runtime_api_contract.md)
- [`minimal_kernel_task_syscall_api_contract.md`](minimal_kernel_task_syscall_api_contract.md)
- [`minimal_kernel_trap_syscall_contract.md`](minimal_kernel_trap_syscall_contract.md)
- [`armv7a_runtime_trap_mapping_contract.md`](armv7a_runtime_trap_mapping_contract.md)
- 板级上下文见 [`../board/rk3506/README.md`](../board/rk3506/README.md)

如果你正在追完整 syscall / trap 链，再顺着读：

- [`minimal_kernel_task_syscall_catalog_contract.md`](minimal_kernel_task_syscall_catalog_contract.md)
- [`minimal_kernel_task_syscall_dispatch_contract.md`](minimal_kernel_task_syscall_dispatch_contract.md)
- [`minimal_kernel_task_syscall_table_contract.md`](minimal_kernel_task_syscall_table_contract.md)
- [`minimal_kernel_task_syscall_frame_contract.md`](minimal_kernel_task_syscall_frame_contract.md)
- [`minimal_kernel_trap_ingress_contract.md`](minimal_kernel_trap_ingress_contract.md)

### 我在看 POSIX / Linux 用户态兼容

先读：

- [`posix_support_overview.md`](posix_support_overview.md)
- [`posix_subsystem_principles.md`](posix_subsystem_principles.md)
- [`posix_three_layer_contract.md`](posix_three_layer_contract.md)
- [`posix_user_runtime_minimal_design.md`](posix_user_runtime_minimal_design.md)

按需要继续：

- 路线与维护：[`posix_compat_roadmap.md`](posix_compat_roadmap.md)、[`posix_maintenance_mode_collaboration.md`](posix_maintenance_mode_collaboration.md)、[`posix_stage_summary.md`](posix_stage_summary.md)
- 推进与验收：[`posix_linux_compat_tasklist.md`](posix_linux_compat_tasklist.md)、[`posix_v0_closure_checklist.md`](posix_v0_closure_checklist.md)、[`posix_busybox_phase_checklist.md`](posix_busybox_phase_checklist.md)
- 子主题设计：[`posix_spawn_minimal_design.md`](posix_spawn_minimal_design.md)、[`posix_fd_table_minimal_design.md`](posix_fd_table_minimal_design.md)、[`posix_errno_mapping.md`](posix_errno_mapping.md)、[`posix_error_semantics.md`](posix_error_semantics.md)

### 我在看 system compiler / explain surface / bring-up 证据链

先读：

- [`resource_contract_v0.md`](resource_contract_v0.md)
- [`artifact_report_v0.md`](artifact_report_v0.md)
- [`explain_surface_v0.md`](explain_surface_v0.md)
- [`bringup_evidence_pipeline_v0.md`](bringup_evidence_pipeline_v0.md)

再按工具链继续：

- [`init_materialized_graph_observe.md`](init_materialized_graph_observe.md)
- [`init_materialized_graph_tooling_milestone.md`](init_materialized_graph_tooling_milestone.md)
- [`../../schemas/README.md`](../../schemas/README.md)

### 我在看 SSU

建议顺序：

- [`ssu_status.md`](ssu_status.md)
- [`ssu_contract.md`](ssu_contract.md)
- [`ssu_discipline.md`](ssu_discipline.md)
- [`ssu_review_checklist.md`](ssu_review_checklist.md)

如果你正在看后续演进，再继续：

- [`ssu_submit_discipline.md`](ssu_submit_discipline.md)
- [`ssu_submit_inventory.md`](ssu_submit_inventory.md)
- [`ssu_migration_priority.md`](ssu_migration_priority.md)
- [`ssu_vnext.md`](ssu_vnext.md)

## 当前目录里的几个文档簇

- 装配与平台边界：`init_graph`、`armv7a_*`、`minimal_kernel_*`
- POSIX 执行面：`posix_*`
- System compiler / explain surface：`artifact_report_v0`、`resource_contract_v0`、`explain_surface_v0`、`bringup_evidence_pipeline_v0`
- SSU：`ssu_*`
- 专题总览：`power_lowpower_overview.md`、`av_pipeline_overview.md`、`at_system.md`

补充提醒：

- `av_pipeline_overview.md` 更偏 AV 中间件接口草图；如果你是看当前音频主线，优先回到 [`../audio/README.md`](../audio/README.md) 与 [`charm_audio_architecture.md`](charm_audio_architecture.md)。

## 暂时不要怎么读

- 不要第一次进 `docs/system/` 就从 `plan / draft / review / checklist` 开始。
- 不要把 `v0`、阶段总结或 tasklist 直接当成已经冻结的长期契约。
- 如果你读到的 system 文档和当前代码、目录入口、构建入口明显不一致，优先回到上位入口文档重新确认。
