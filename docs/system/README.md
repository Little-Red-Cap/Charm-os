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
| ARMv7-A / RK3506 平台边界 | [`armv7a_platform_contract.md`](armv7a_platform_contract.md)、[`../board/rk3506/README.md`](../board/rk3506/README.md) |
| minimal-kernel runtime 总证据链 | [`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md) |
| minimal-kernel host smoke / 冷启动与热复用 | [`minimal_kernel_host_smoke_bundle_contract.md`](minimal_kernel_host_smoke_bundle_contract.md) |
| syscall / trap 链 | [`minimal_kernel_task_syscall_table_contract.md`](minimal_kernel_task_syscall_table_contract.md)、[`minimal_kernel_trap_syscall_contract.md`](minimal_kernel_trap_syscall_contract.md)、[`armv7a_runtime_trap_mapping_contract.md`](armv7a_runtime_trap_mapping_contract.md) |
| POSIX / Linux 用户态兼容 | [`posix_support_overview.md`](posix_support_overview.md) |
| system compiler 结果物与解释面 | [`artifact_report_v0.md`](artifact_report_v0.md)、[`explain_surface_v0.md`](explain_surface_v0.md) |
| 资源法律与 bring-up 证据 | [`resource_contract_v0.md`](resource_contract_v0.md)、[`bringup_evidence_pipeline_v0.md`](bringup_evidence_pipeline_v0.md) |
| opening judgment / testimony 上层阅读通路 | [`opening_judgment_corridor_v0.md`](opening_judgment_corridor_v0.md)、[`opening_judgment_corridor_witness_taxonomy_v0.md`](opening_judgment_corridor_witness_taxonomy_v0.md) |

## 阅读规则

- `*_contract.md` / `*_overview.md` 优先视为当前入口或现行约束。
- `*_plan.md` / `*_roadmap.md` / `*_draft.md` 优先视为方向、迁移或讨论材料。
- `*_v0.md` / `*_review.md` / `*_summary.md` 优先视为阶段快照，需要结合当前代码一起读。
- `*_tasklist.md` / `*_checklist.md` 主要服务推进、验收和排期，不默认充当主题首页。

## 阶段材料

front-page、opening-flow、witness、biography、world compare 等 system compiler 阶段材料不再逐篇列入默认阅读路径。

如果需要追溯这条历史链，先从这些上位文档进入：

- [`../architecture/system_compiler_roadmap.md`](../architecture/system_compiler_roadmap.md)
- [`../architecture/system_compiler_vocabulary_v0.md`](../architecture/system_compiler_vocabulary_v0.md)
- [`artifact_report_v0.md`](artifact_report_v0.md)
- [`explain_surface_v0.md`](explain_surface_v0.md)

## 暂时不要怎么读

- 不要第一次进 `docs/system/` 就从 `plan / draft / review / checklist` 开始。
- 不要把 `v0`、阶段总结或 tasklist 直接当成已经冻结的长期契约。
- 如果 system 文档和当前代码、目录入口、构建入口明显不一致，优先回到上位入口重新确认。
