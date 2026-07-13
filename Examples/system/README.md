# System 示例入口

本目录保存 system/runtime 的 host、QEMU 与语义 smoke。示例只证明对应 fixture；默认系统契约从
[`docs/system/README.md`](../../docs/system/README.md) 进入，device 边界见
[`driver_model.md`](../../docs/architecture/driver_model.md)。

## 推荐入口

| 目标 | 示例 |
|---|---|
| Resident ELF / QEMU | [`resident_elf_qemu_smoke`](resident_elf_qemu_smoke/README.md) |
| app_lab host 主链 | [`app_lab_mainline_smoke`](app_lab_mainline_smoke/README.md) |
| AppHost / poster | [`app_host_poster_demo`](app_host_poster_demo/README.md) |
| Runtime block/channel slot | [`device_runtime_block_slot_demo`](device_runtime_block_slot_demo/README.md)、[`device_runtime_channel_slot_demo`](device_runtime_channel_slot_demo/README.md) |
| Device bus / registry | `device_bus_demo/`、`device_registry_demo/` |
| Power policy 原型 | `power_demo/` |

QEMU runner、scope token、failure taxonomy 和 evidence bundle 参数只在
`resident_elf_qemu_smoke/README.md` 维护，不在本页复制。

## Backend / capability smokes

| 边界 | 示例 |
|---|---|
| topology bridge | [`capability_topology_bridge_smoke`](capability_topology_bridge_smoke/README.md) |
| console provider | [`console_output_provider_smoke`](console_output_provider_smoke/README.md) |
| block storage provider | [`block_storage_provider_smoke`](block_storage_provider_smoke/README.md) |

这些 smokes 验证局部 requirement/provider/binding 适配，不授予 Backend、Provider 或 adapter
Charm Core 身份。

## 冻结探索

以下 Host fixture 保留 RTE/Spine/static-reflection 的历史局部断言：

| Fixture | 保留的局部问题 |
|---|---|
| `charm_spine_smoke` | kind/role、binding、init/context/evidence 的最初组合形状 |
| `charm_spine_evidence_projection_smoke` | init 后只读 evidence side channel |
| `rte_component_context_smoke` | 显式 binding、裁剪 context 与最小 init order |
| `rte_context_slice_smoke` | 同一解析结果中的 per-app context slice |
| `rte_evidence_slice_smoke` | app context 与 profile-wide evidence 分离 |
| `rte_explain_projection_smoke` | accepted result 的只读 explain 投影 |
| `rte_init_projection_smoke` | 局部 component 描述到 `init::Graph` 的映射 |
| `rte_multi_role_provider_smoke` | 同 kind 多 role 必须分别声明和绑定 |
| `rte_profile_materialization_smoke` | init 与 app context 是不同投影 |
| `rte_profile_resolution_smoke` | missing/duplicate/extra/wrong-role/stale binding 拒绝 |
| `rte_profile_selection_smoke` | 同一 app 要求可绑定不同 fixture provider |
| `rte_projection_consistency_smoke` | 多个投影保留同一已选 provider identity |
| `rte_projection_gate_smoke` | 只有 accepted local result 可进入投影 |
| `rte_reflected_context_smoke` | `<meta>` 字段发现与裁剪 context 的局部组合 |
| `rte_reflected_profile_resolution_smoke` | reflected token 参与局部 binding 拒绝 |
| `charm_spine_reflected_profile_smoke` | accepted/blocked report 顺序；有独立 [`README`](charm_spine_reflected_profile_smoke/README.md) |
| `static_reflection_probe` | ARM compiler 的 `<meta>`/member-discovery 编译期能力 |

`static_reflection_probe` 仍记录一项工具链限制：不要把
`nonstatic_data_members_of()` 返回的 `vector<info>` 直接作为 type reify 输入。具体编译器参数与断言以
各 fixture 的 `CMakeLists.txt`、源文件和 `run-rte-phase0-smoke.ps1` 为准。

它们不是当前平台主链或默认回归入口，不能从 smoke 数量推导 RTE、Spine、Profile、Projection
或 Evidence 已进入 Core。架构状态以 [`architecture/README.md`](../../docs/architecture/README.md)
和 Constitution 为准。

## 使用规则

- 有局部 README 时先读 README；冻结 fixture 直接以源码、CMake 和聚合 runner 为准。
- Host、QEMU 与 real board 证据分开解释。
- boot 与板级 bring-up 分别从 [`boot/README.md`](../boot/README.md) 和具体 project 进入。
