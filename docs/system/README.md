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
- [`system_coordination_contract_v0.md`](system_coordination_contract_v0.md)
- [`armv7a_platform_contract.md`](armv7a_platform_contract.md)
- 如果同时涉及板级 bring-up，再读 [`../board/README.md`](../board/README.md)

### 我在看真实板级 system coordination / service snapshot

先读：

- [`system_coordination_contract_v0.md`](system_coordination_contract_v0.md)
- [`../architecture/real_board_landing_gap_audit_v0.md`](../architecture/real_board_landing_gap_audit_v0.md)
- [`../architecture/capability_recovery_rules.md`](../architecture/capability_recovery_rules.md)

这条路线用于判断 `SystemShell`、`ServiceSnapshotContract`、`PowerProfile`、`GuardedMutation`、`ReadyFacts` 的归属边界。它不要求先把 audio / network / display / storage 的重 runtime 全部接入系统层。

### 我在看 ARMv7-A / RK3506 / 最小内核

先读：

- [`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md)
- [`minimal_kernel_host_smoke_bundle_contract.md`](minimal_kernel_host_smoke_bundle_contract.md)
- [`minimal_kernel_trap_syscall_contract.md`](minimal_kernel_trap_syscall_contract.md)
- [`minimal_kernel_trap_ingress_contract.md`](minimal_kernel_trap_ingress_contract.md)
- [`armv7a_platform_contract.md`](armv7a_platform_contract.md)
- 板级上下文见 [`../board/rk3506/README.md`](../board/rk3506/README.md)

### 我在看 POSIX / Linux 用户态兼容

先读：

- [`posix_support_overview.md`](posix_support_overview.md)
- [`posix_subsystem_principles.md`](posix_subsystem_principles.md)
- [`posix_three_layer_contract.md`](posix_three_layer_contract.md)
- [`posix_user_runtime_minimal_design.md`](posix_user_runtime_minimal_design.md)

### 我在看 system compiler / explain surface / bring-up 证据链

先读：

- [`artifact_report_v0.md`](artifact_report_v0.md)
- [`explain_surface_v0.md`](explain_surface_v0.md)
- [`resource_contract_v0.md`](resource_contract_v0.md)
- [`bringup_evidence_pipeline_v0.md`](bringup_evidence_pipeline_v0.md)

### 我在看 SSU

先读：

- [`ssu_status.md`](ssu_status.md)
- [`ssu_contract.md`](ssu_contract.md)
- [`ssu_discipline.md`](ssu_discipline.md)
- [`ssu_review_checklist.md`](ssu_review_checklist.md)

## 当前目录里的几个文档簇

- 装配与平台边界：`init_graph`、`armv7a_*`、`minimal_kernel_*`
- POSIX 执行面：`posix_*`
- System compiler / explain surface：`artifact_report_v0`、`resource_contract_v0`、`explain_surface_v0`、`bringup_evidence_pipeline_v0`
- SSU：`ssu_*`

## 历史证据链

早期 `system compiler front-page / opening-flow / biography / world / witness` 阶段材料已归档，不再作为默认首读路径：

- [`../archive/system-compiler-front-page-v0/README.md`](../archive/system-compiler-front-page-v0/README.md)

## 暂时不要怎么读

- 不要第一次进 `docs/system/` 就从 `plan / draft / review / checklist` 开始。
- 不要把 `v0`、阶段总结或 tasklist 直接当成已经冻结的长期契约。
- 如果你读到的 system 文档和当前代码、目录入口、构建入口明显不一致，优先回到上位入口文档重新确认。
