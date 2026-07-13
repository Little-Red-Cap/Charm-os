# POSIX v0 阶段材料归档

状态：archive。

本目录只保存 POSIX v0 中仍可独立消费的 ELF 基线、工具链故障记录和架构取舍。
这些材料不作为现行能力说明。

保留文件：

- [`posix_elf_stage1_baseline.md`](posix_elf_stage1_baseline.md)
- [`posix_modules_ts_build_notes.md`](posix_modules_ts_build_notes.md)
- [`posix_architecture_retained_notes.md`](posix_architecture_retained_notes.md)：从早期 subsystem principles
  与 three-layer 草案中保留宏边界、三层责任、ABI spine 和 image 执行取舍。

当前源码边界和验证入口见：

- [`../../system/posix_support_overview.md`](../../system/posix_support_overview.md)
- [`../../system/posix_error_semantics.md`](../../system/posix_error_semantics.md)
- [`../../system/posix_fd_table_contract.md`](../../system/posix_fd_table_contract.md)
- [`../../system/posix_program_image_contract.md`](../../system/posix_program_image_contract.md)
- [`../../system/posix_spawn_contract.md`](../../system/posix_spawn_contract.md)
- [`../../system/posix_user_runtime_contract.md`](../../system/posix_user_runtime_contract.md)

判断当前能力时必须重新检查源码、CMake 与当次 smoke，不得引用归档中的阶段结论代替验证。
