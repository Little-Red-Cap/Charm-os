# 系统文档入口

## 文档状态

- `status`: `supporting`
- `scope`: init、runtime、POSIX、SSU 与系统工具路由
- `authority`: [`CONSTITUTION.md`](../../CONSTITUTION.md)

本页只做任务路由。行为、字段和验证命令由对应 contract、源码与 runner 定义。

| 问题 | 入口 |
|---|---|
| 初始化与服务顺序 | [`init_graph_contract.md`](init_graph_contract.md) |
| ARMv7-A 平台与 trap | [`armv7a_platform_contract.md`](armv7a_platform_contract.md)、[`armv7a_runtime_trap_mapping_contract.md`](armv7a_runtime_trap_mapping_contract.md) |
| Minimal-kernel 总证据链 | [`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md) |
| Minimal-kernel host smoke | [`minimal_kernel_host_smoke_bundle_contract.md`](minimal_kernel_host_smoke_bundle_contract.md) |
| Runtime bridge / task message | [`minimal_kernel_runtime_bridge_contract.md`](minimal_kernel_runtime_bridge_contract.md)、[`minimal_kernel_task_message_runtime_contract.md`](minimal_kernel_task_message_runtime_contract.md) |
| Syscall / trap | [`minimal_kernel_task_syscall_table_contract.md`](minimal_kernel_task_syscall_table_contract.md)、[`minimal_kernel_trap_syscall_contract.md`](minimal_kernel_trap_syscall_contract.md) |
| Session witness / ledger | [`kernel_runtime_session_witness_v0.md`](kernel_runtime_session_witness_v0.md)、[`minimal_kernel_runtime_ledger_fact_contract_v0.md`](minimal_kernel_runtime_ledger_fact_contract_v0.md) |
| POSIX | [`posix_support_overview.md`](posix_support_overview.md) |
| RTOS runtime / ISR | [`rtos_runtime_contract.md`](rtos_runtime_contract.md) |
| Power | [`power_lowpower_overview.md`](power_lowpower_overview.md) |
| SSU | [`ssu_contract.md`](ssu_contract.md) |
| Artifact / explain 工具 | [`artifact_report_v0.md`](artifact_report_v0.md)、[`explain_surface_v0.md`](explain_surface_v0.md) |
| Script / schema 面治理 | [`script_surface_reduction_governance_v0.md`](script_surface_reduction_governance_v0.md)、[`schema_surface_reduction_governance_v0.md`](schema_surface_reduction_governance_v0.md) |

POSIX 细分契约从 [`posix_support_overview.md`](posix_support_overview.md) 进入；Minimal-kernel runner、
schema 和 CI 由总证据链维护，不在本页复制。

历史讨论位于 [`minimal-kernel-runtime-v0`](../archive/minimal-kernel-runtime-v0/README.md)、
[`posix-v0`](../archive/posix-v0/README.md)、[`system-evidence-and-staging-v0`](../archive/system-evidence-and-staging-v0/README.md)、
[`system-compiler-front-page-v0`](../archive/system-compiler-front-page-v0/README.md) 和
[`ssu-phase-notes`](../archive/ssu-phase-notes/README.md)。归档结论与 smoke 名称不证明当前实现状态。
