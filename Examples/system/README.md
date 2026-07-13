# System 示例入口

## 文档状态

- `status`: `supporting`
- `scope`: `Examples/system` 任务路由
- `authority`: 各子目录 `CMakeLists.txt` 与源码

本目录保存 system/runtime 的 Host、QEMU 与语义 fixture。目录存在或 smoke 通过只证明局部行为；
系统契约从 [`docs/system`](../../docs/system/README.md) 进入，device 边界见
[`driver_model.md`](../../docs/architecture/driver_model.md)。

## 直接入口

| 目标 | 入口 |
|---|---|
| Resident ELF / QEMU | [`resident_elf_qemu_smoke`](resident_elf_qemu_smoke/README.md) |
| App Lab Host 主链 | [`app_lab_mainline_smoke`](app_lab_mainline_smoke/README.md) |
| AppHost / poster | [`app_host_poster_demo`](app_host_poster_demo/README.md) |
| Capability MVP | [`charm_capability_mvp`](charm_capability_mvp/README.md) |
| reflected profile exploration | [`charm_spine_reflected_profile_smoke`](charm_spine_reflected_profile_smoke/README.md) |

QEMU token、failure taxonomy 和 runner 参数只在对应目录维护。

## 按范围定位

| 范围 | 目录 pattern | 上位边界 |
|---|---|---|
| App ABI 与 Store | `app_abi_*` | [`Examples/app_abi`](../app_abi/README.md) |
| Dev Loader receive/packet/handoff | `dev_loader_*` | [`Examples/dev_loader`](../dev_loader/README.md) |
| Resident artifact/inspect/launcher | `resident_*` | [`resident image contract`](../../docs/architecture/resident_image_platform_v1_contract.md) |
| runtime device/slot | `device_*` | [`driver model`](../../docs/architecture/driver_model.md)、[`IO`](../../docs/io/README.md) |
| capability/backend adapter | `capability_topology_bridge_smoke`、`*_provider_smoke` | [`Core contract`](../../docs/architecture/charm_core_contract.md) |
| power/signal | `power_demo`、`signal_state_closure_demo` | [`system docs`](../../docs/system/README.md) |
| Player fixture | `player_*` | [`Player project`](../project/player/README.md) |
| frozen RTE/Spine exploration | `rte_*`、`charm_spine_*`、`static_reflection_probe` | [`architecture exploration`](../../docs/architecture/README.md) |

每个 target、负例和编译参数由目录内 CMake/source 定义，本页不维护完整清单。

## 解释规则

- App ABI/Store fixture 不证明 ARM image execution、QSPI/eMMC 或产品 update policy。
- Dev Loader fixture 不证明真实 UART/USB、板级内存或目标代码执行。
- device slot fixture 不证明真实 hot-plug controller 或 media。
- capability/provider fixture 不授予 Backend、Provider、RTE 或 Spine Core 身份。
- Host、QEMU 与 real board 是不同证据域，不能互相替代。

`static_reflection_probe` 保留一项工具链限制：不要把 `nonstatic_data_members_of()` 返回的
`vector<info>` 直接作为 type reify 输入。其它编译器约束以该目录源码和 CMake 为准。
