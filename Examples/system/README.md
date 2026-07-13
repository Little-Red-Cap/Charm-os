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

以下目录保留 RTE/Spine/static-reflection 的历史语义 fixture：

- `rte_*_smoke/`
- `charm_spine*_smoke/`
- `static_reflection_probe/`
- `run-rte-phase0-smoke.ps1`

它们不是当前平台主链或默认回归入口，不能从 smoke 数量推导 RTE、Spine、Profile、Projection
或 Evidence 已进入 Core。架构状态以 [`architecture/README.md`](../../docs/architecture/README.md)
和 Constitution 为准。

## 使用规则

- 先读目标目录 README，再运行其声明的 runner。
- Host、QEMU 与 real board 证据分开解释。
- boot 与板级 bring-up 分别从 [`boot/README.md`](../boot/README.md) 和具体 project 进入。
