# POSIX v0 阶段材料归档

状态：archive。

本目录保存 POSIX v0 的 ELF 基线、工具链解阻记录和早期 fd/spawn/image/runtime 分层论证。
这些材料包含真实取舍与故障经验，但不再作为现行能力说明。

保留文件：

- [`posix_elf_stage1_baseline.md`](posix_elf_stage1_baseline.md)
- [`posix_errno_mapping.md`](posix_errno_mapping.md)
- [`posix_error_semantics_v1.md`](posix_error_semantics_v1.md)
- [`posix_fd_table_minimal_design.md`](posix_fd_table_minimal_design.md)
- [`posix_modules_ts_build_notes.md`](posix_modules_ts_build_notes.md)
- [`posix_program_image_minimal_design.md`](posix_program_image_minimal_design.md)
- [`posix_program_image_elf_minimal_design.md`](posix_program_image_elf_minimal_design.md)
- [`posix_program_image_modulex_adapter.md`](posix_program_image_modulex_adapter.md)
- [`posix_spawn_minimal_design.md`](posix_spawn_minimal_design.md)
- [`posix_subsystem_principles.md`](posix_subsystem_principles.md)
- [`posix_three_layer_contract.md`](posix_three_layer_contract.md)
- [`posix_user_runtime_minimal_design.md`](posix_user_runtime_minimal_design.md)

BusyBox phase checklist、cleanup plan、compat roadmap、Linux tasklist、maintenance collaboration、
stage summary 和 v0 closure checklist 已删除。它们维护的“当前/下一步/Done/P1”和旧 runner 路径
没有长期证据价值；需要追溯时使用 Git 历史。

当前源码边界和验证入口见：

- [`../../system/posix_support_overview.md`](../../system/posix_support_overview.md)

判断当前能力时必须重新检查源码、CMake 与当次 smoke，不得引用归档中的阶段结论代替验证。
