# 停线前战线浅索引

> [!IMPORTANT]
> **文档状态：`supporting`（停线基线）**
> 本页保留停线前各战线入口，供修复和追溯使用；它不再裁决 Charm Core 或默认路线。
> 先读 [`../CONSTITUTION.md`](../CONSTITUTION.md) 与
> [`architecture/charm_core_contract.md`](architecture/charm_core_contract.md)。

本页用于保存停线前默认入口的浅索引。

目标不是替代各专题文档，而是把“官方入口”和“深层入口”分开，降低回仓和新读者的识别成本。

如需先理解整个仓库的治理模型，先读：

- [`repo_governance.md`](repo_governance.md)

## 1. Shared substrate

- `track_kind`: `substrate`
- `track_status`: `active`
- 官方入口：
  - [`overview.md`](overview.md)
  - [`architecture_overview.md`](architecture_overview.md)
  - [`capability_map.md`](capability_map.md)
  - [`system/init_graph_contract.md`](system/init_graph_contract.md)
- 深层入口：
  - [`io/README.md`](io/README.md)
  - [`storage/README.md`](storage/README.md)
  - [`architecture/README.md`](architecture/README.md)
  - [`project/README.md`](project/README.md)

## 2. System compiler

- `track_kind`: `theory`
- `track_status`: `exploring`
- 官方入口：
  - [`architecture/system_compiler_roadmap.md`](architecture/system_compiler_roadmap.md)
  - [`architecture/system_compiler_vocabulary_v0.md`](architecture/system_compiler_vocabulary_v0.md)
  - [`system/artifact_report_v0.md`](system/artifact_report_v0.md)
  - [`system/explain_surface_v0.md`](system/explain_surface_v0.md)
- 深层入口：
  - [`system/resource_contract_v0.md`](system/resource_contract_v0.md)
  - [`system/bringup_evidence_pipeline_v0.md`](system/bringup_evidence_pipeline_v0.md)
  - [`archive/system-compiler-front-page-v0/README.md`](archive/system-compiler-front-page-v0/README.md)

## 3. Player + Vivid

- `track_kind`: `pressure`
- `track_status`: `active`
- 官方入口：
  - [`../Examples/project/player/README.md`](../Examples/project/player/README.md)
  - [`../Examples/project/player/ARCHITECTURE_CONVERGENCE.md`](../Examples/project/player/ARCHITECTURE_CONVERGENCE.md)
  - [`ui/README.md`](ui/README.md)
- 深层入口：
  - [`ui/player_vivid_patterns.md`](ui/player_vivid_patterns.md)
  - [`../Examples/project/player/app-vivid-MaterialDesign3/README.md`](../Examples/project/player/app-vivid-MaterialDesign3/README.md)
  - [`project/player_design.md`](project/player_design.md)

## 4. RK3506 + minimal-kernel landing

- `track_kind`: `landing`
- `track_status`: `active`
- 官方入口：
  - [`system/minimal_kernel_runtime_evidence_bundle_contract.md`](system/minimal_kernel_runtime_evidence_bundle_contract.md)
  - [`system/minimal_kernel_host_smoke_bundle_contract.md`](system/minimal_kernel_host_smoke_bundle_contract.md)
  - [`../targets/rk3506/README.md`](../targets/rk3506/README.md)
  - [`board/rk3506/README.md`](board/rk3506/README.md)
- 深层入口：
  - [`system/minimal_kernel_runtime_ledger_fact_contract_v0.md`](system/minimal_kernel_runtime_ledger_fact_contract_v0.md)
  - [`system/minimal_kernel_trap_ingress_contract.md`](system/minimal_kernel_trap_ingress_contract.md)
  - [`system/armv7a_runtime_trap_mapping_contract.md`](system/armv7a_runtime_trap_mapping_contract.md)
  - [`../Examples/kernel/armv7a/qemu/README.md`](../Examples/kernel/armv7a/qemu/README.md)

## 5. POSIX v0

- `track_kind`: `maintenance`
- `track_status`: `maintained`
- 官方入口：
  - [`system/posix_support_overview.md`](system/posix_support_overview.md)
  - [`system/posix_maintenance_mode_collaboration.md`](system/posix_maintenance_mode_collaboration.md)
- 深层入口：
  - [`system/posix_stage_summary.md`](system/posix_stage_summary.md)
  - [`system/posix_v0_closure_checklist.md`](system/posix_v0_closure_checklist.md)
  - [`system/posix_linux_compat_tasklist.md`](system/posix_linux_compat_tasklist.md)

## 6. 脚本 / workflow / 示例浅索引

### minimal-kernel / evidence

- `track_kind`: `landing`
- `track_status`: `active`
- 官方入口：
  - [`../scripts/minimal_kernel_runtime_evidence_bundle.ps1`](../scripts/minimal_kernel_runtime_evidence_bundle.ps1)
  - [`../scripts/minimal_kernel_runtime_host_smoke_dual_bundle.ps1`](../scripts/minimal_kernel_runtime_host_smoke_dual_bundle.ps1)
  - [`../scripts/minimal_kernel_runtime_armv7a_qemu_smoke_bundle.ps1`](../scripts/minimal_kernel_runtime_armv7a_qemu_smoke_bundle.ps1)
  - [`../scripts/ci_minimal_kernel_runtime_evidence_bundle.ps1`](../scripts/ci_minimal_kernel_runtime_evidence_bundle.ps1)
  - [`../.github/workflows/minimal-kernel-runtime-evidence.yml`](../.github/workflows/minimal-kernel-runtime-evidence.yml)
- 深层入口：
  - [`../.github/workflows/minimal-kernel-runtime-session-witness.yml`](../.github/workflows/minimal-kernel-runtime-session-witness.yml)
  - [`../.github/workflows/minimal-kernel-runtime-system-compiler-witness.yml`](../.github/workflows/minimal-kernel-runtime-system-compiler-witness.yml)
  - [`../.github/workflows/minimal-kernel-runtime-system-compiler-world-compare.yml`](../.github/workflows/minimal-kernel-runtime-system-compiler-world-compare.yml)

### build / examples

- `track_kind`: `substrate`
- `track_status`: `active`
- 官方入口：
  - [`../CMakePresets.json`](../CMakePresets.json)
  - [`project/README.md`](project/README.md)
  - [`../.github/workflows/build-clang.yml`](../.github/workflows/build-clang.yml)
  - [`../.github/workflows/build-arm-none-eabi.yml`](../.github/workflows/build-arm-none-eabi.yml)
- 深层入口：
  - [`agent/routes/build.md`](agent/routes/build.md)
  - [`documentation_maintenance.md`](documentation_maintenance.md)

### Player 项目入口

- `track_kind`: `pressure`
- `track_status`: `active`
- 官方入口：
  - [`../Examples/project/player/README.md`](../Examples/project/player/README.md)
  - [`../Examples/project/player/win/CMakeLists.txt`](../Examples/project/player/win/CMakeLists.txt)
  - 历史路径：`Examples/project/player/stn32h747_HQZY/CM7/CMakeLists.txt`（停线时已不存在）
- 深层入口：
  - [`../Examples/project/player/app-vivid-MaterialDesign3/README.md`](../Examples/project/player/app-vivid-MaterialDesign3/README.md)
  - 历史路径：`Examples/project/player/stn32h747_HQZY/CM7/CLION_WORKFLOW.md`（停线时已不存在）

### RK3506 叶子 target 入口

- `track_kind`: `landing`
- `track_status`: `active`
- 官方入口：
  - [`../targets/rk3506/README.md`](../targets/rk3506/README.md)
  - [`../targets/rk3506/CharmTargetConfig.cmake`](../targets/rk3506/CharmTargetConfig.cmake)
  - [`board/rk3506/README.md`](board/rk3506/README.md)
- 深层入口：
  - [`board/rk3506/post_ddr_handoff_contract.md`](board/rk3506/post_ddr_handoff_contract.md)
  - [`../targets/rk3506/sources.cmake`](../targets/rk3506/sources.cmake)

### POSIX 维护入口

- `track_kind`: `maintenance`
- `track_status`: `maintained`
- 官方入口：
  - [`system/posix_support_overview.md`](system/posix_support_overview.md)
  - [`system/posix_maintenance_mode_collaboration.md`](system/posix_maintenance_mode_collaboration.md)
- 深层入口：
  - [`../Examples/posix`](../Examples/posix)
  - [`../Examples/kernel/posix`](../Examples/kernel/posix)
  - [`system/posix_stage_summary.md`](system/posix_stage_summary.md)

## 使用提醒

- 先认战线，再进专题。
- 先看官方入口，再进深层材料。
- 深层入口默认不承担首读职责。
