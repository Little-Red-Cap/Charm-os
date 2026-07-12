# POSIX v0 阶段材料归档

状态：archive。

本目录保存 POSIX v0 推进期间的基线、roadmap、任务清单、维护协作、收口判断和工具链
解阻记录，以及早期分层论证。这些材料包含真实取舍与故障经验，但大量“当前状态”、
“官方基线”和“下一步”已经具有时间依赖，不再作为现行能力说明。

保留文件：

- [`posix_elf_stage1_baseline.md`](posix_elf_stage1_baseline.md)
- [`posix_errno_mapping.md`](posix_errno_mapping.md)
- [`posix_error_semantics_v1.md`](posix_error_semantics_v1.md)
- [`posix_fd_table_minimal_design.md`](posix_fd_table_minimal_design.md)
- [`posix_stage_summary.md`](posix_stage_summary.md)
- [`posix_compat_roadmap.md`](posix_compat_roadmap.md)
- [`posix_linux_compat_tasklist.md`](posix_linux_compat_tasklist.md)
- [`posix_v0_closure_checklist.md`](posix_v0_closure_checklist.md)
- [`posix_busybox_phase_checklist.md`](posix_busybox_phase_checklist.md)
- [`posix_cleanup_refactor_plan.md`](posix_cleanup_refactor_plan.md)
- [`posix_maintenance_mode_collaboration.md`](posix_maintenance_mode_collaboration.md)
- [`posix_modules_ts_build_notes.md`](posix_modules_ts_build_notes.md)
- [`posix_program_image_minimal_design.md`](posix_program_image_minimal_design.md)
- [`posix_program_image_elf_minimal_design.md`](posix_program_image_elf_minimal_design.md)
- [`posix_program_image_modulex_adapter.md`](posix_program_image_modulex_adapter.md)
- [`posix_spawn_minimal_design.md`](posix_spawn_minimal_design.md)
- [`posix_subsystem_principles.md`](posix_subsystem_principles.md)
- [`posix_three_layer_contract.md`](posix_three_layer_contract.md)

当前源码边界和验证入口见：

- [`../../system/posix_support_overview.md`](../../system/posix_support_overview.md)

判断当前能力时必须重新检查源码、CMake 与当次 smoke，不得引用归档中的阶段结论代替验证。
