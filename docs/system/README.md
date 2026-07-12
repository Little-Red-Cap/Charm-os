# 系统文档入口

本页只提供系统装配、runtime、POSIX、SSU 与工具链的最短入口，不罗列全部 schema、脚本和
阶段证据。第一次进入仓库时先读：

1. [`../README.md`](../README.md)
2. [`../overview.md`](../overview.md)
3. [`../architecture/charm_core_contract.md`](../architecture/charm_core_contract.md)

## 按问题进入

| 问题 | 先读 |
|---|---|
| 初始化、装配和服务顺序 | [`init_graph_contract.md`](init_graph_contract.md) |
| 真实板共享系统协调 | [`system_coordination_contract_v0.md`](system_coordination_contract_v0.md) |
| ARMv7-A 平台与 trap 映射 | [`armv7a_platform_contract.md`](armv7a_platform_contract.md)、[`armv7a_runtime_trap_mapping_contract.md`](armv7a_runtime_trap_mapping_contract.md) |
| Minimal-kernel runtime 证据 | [`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md) |
| Minimal-kernel host smoke | [`minimal_kernel_host_smoke_bundle_contract.md`](minimal_kernel_host_smoke_bundle_contract.md) |
| Syscall / trap | [`minimal_kernel_task_syscall_table_contract.md`](minimal_kernel_task_syscall_table_contract.md)、[`minimal_kernel_trap_syscall_contract.md`](minimal_kernel_trap_syscall_contract.md) |
| POSIX 用户态兼容 | [`posix_support_overview.md`](posix_support_overview.md) |
| SSU | [`ssu_contract.md`](ssu_contract.md) |
| Artifact / explain 工具 | [`artifact_report_v0.md`](artifact_report_v0.md)、[`explain_surface_v0.md`](explain_surface_v0.md) |
| Script / schema 面治理 | [`script_surface_reduction_governance_v0.md`](script_surface_reduction_governance_v0.md)、[`schema_surface_reduction_governance_v0.md`](schema_surface_reduction_governance_v0.md) |

## Minimal-kernel

默认从 runtime evidence bundle 进入。只有修改对应边界时再深入：

- runtime 与执行会话：[`minimal_kernel_runtime_bridge_contract.md`](minimal_kernel_runtime_bridge_contract.md)
- ledger facts：[`minimal_kernel_runtime_ledger_fact_contract_v0.md`](minimal_kernel_runtime_ledger_fact_contract_v0.md)
- task message：[`minimal_kernel_task_message_runtime_contract.md`](minimal_kernel_task_message_runtime_contract.md)
- session witness：[`kernel_runtime_session_witness_v0.md`](kernel_runtime_session_witness_v0.md)

具体 runner、schema 和 CI workflow 由上述 contract 维护，不在本页复制。早期分篇讨论见
[`../archive/minimal-kernel-runtime-v0/`](../archive/minimal-kernel-runtime-v0/README.md)。

## POSIX

先读 [`posix_support_overview.md`](posix_support_overview.md)。现行专题契约包括：

- [`posix_three_layer_contract.md`](posix_three_layer_contract.md)
- [`posix_program_image_contract.md`](posix_program_image_contract.md)
- [`posix_spawn_contract.md`](posix_spawn_contract.md)
- [`posix_fd_table_contract.md`](posix_fd_table_contract.md)
- [`posix_user_runtime_contract.md`](posix_user_runtime_contract.md)
- [`posix_error_semantics.md`](posix_error_semantics.md)

POSIX v0 阶段材料见 [`../archive/posix-v0/`](../archive/posix-v0/README.md)。

## 工具与历史

- System Compiler、front-page、opening-flow 与 world compare 是工具/探索线，不定义 Charm Core。
- 资源、bring-up、materialized graph 与 ARMv7-A staging 历史见
  [`../archive/system-evidence-and-staging-v0/`](../archive/system-evidence-and-staging-v0/README.md)。
- Front-page 历史见
  [`../archive/system-compiler-front-page-v0/`](../archive/system-compiler-front-page-v0/README.md)。
- SSU 阶段记录见 [`../archive/ssu-phase-notes/`](../archive/ssu-phase-notes/README.md)。

## 阅读规则

- contract/overview 只在自身专题内有效，不能覆盖 Constitution 与核心契约。
- plan、roadmap、draft、review、summary、tasklist 和 checklist 默认是阶段材料。
- schema、sample、report 字段和文档数量都不是运行证据；以源码和当次 smoke 为准。
- 不从本页反推某项能力已在 Host、QEMU 或真实板上成立。
