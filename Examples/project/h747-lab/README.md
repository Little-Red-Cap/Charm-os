# H747 Lab

`h747-lab` 是 DIY STM32H747 board 的独立 project 入口，不接入仓库根 `CMakeLists.txt`。本目录拥有
preset、board package、service、profile、app 与 firmware target。

## 入口

| 任务 | 入口 |
|---|---|
| App/target 索引 | [`apps/README.md`](apps/README.md) |
| source、profile 与 target ownership | [`h747_lab_layering_contract.md`](docs/h747_lab_layering_contract.md) |
| project-local capability shape | [`h747_lab_capability_contract.md`](docs/h747_lab_capability_contract.md) |
| `dev_loader/app_lab/posix_lab` 分工 | [`h747_lab_dynamic_boundary_roadmap.md`](docs/h747_lab_dynamic_boundary_roadmap.md) |
| Host/QEMU 原型进入 H747 的边界 | [`h747_lab_spine_migration_boundary.md`](docs/h747_lab_spine_migration_boundary.md) |

Profile 与 target 的完整集合以 [`CMakePresets.json`](CMakePresets.json)、`profiles/*/profile.cmake` 和
CMake configure 输出为准，不在 README 复制清单。

## Build

在本目录配置并复用同一个 `cmake-build-*`。常用 resident runtime 示例：

```powershell
cmake --preset h747-lab-debug
cmake --build --preset build-h747-lab-dev-loader-debug -- -j1
```

Capability MVP 使用独立 preset：

```powershell
cmake --preset h747-lab-capability-mvp-debug
cmake --build --preset build-h747-lab-capability-mvp-debug -- -j1
```

Toolchain 会按工具链文件的当前规则查找 Arm GNU，也可显式指定：

```powershell
cmake --preset h747-lab-debug -DH747_LAB_ARM_GNU_TOOLCHAIN_ROOT="<arm-gnu-root>"
```

切换 compiler/binutils 时，对同一 preset 使用 `cmake --fresh`，避免 CMake cache 混用旧 linker 或
archiver。不要对同一个 Ninja binary directory 并发运行多个 build。

Configure 会先运行 H747 BSP doctor：foundation profile 只要求其实际 source set；依赖 I2C、storage、
FMC/SDRAM、QSPI 或 SPI 的 profile 缺失对应 generated source 时明确失败。这类失败属于 BSP source
前置条件，不是 ELF、AppRuntime、packetstream 或 Store regression。外部 BSP 只能通过显式
`DRAFT_ROOT`/cache boundary 接入，不把 board fact 复制回 Charm。

Host fixture 复用本目录的 host build tree：

```powershell
cmake -S host -B cmake-build-host-debug
cmake --build cmake-build-host-debug -- -j1
ctest --test-dir cmake-build-host-debug -C Debug --output-on-failure
```

生成的 executable、CTest artifact 和 `.ppm` evidence 保持在 `cmake-build-*`，不写入源码树。

## Evidence

| 范围 | 入口 |
|---|---|
| SDRAM、QSPI 与外部总线 | [`h747_lab_memory_evidence.md`](docs/h747_lab_memory_evidence.md) |
| SDRAM framebuffer、LTDC/DSI 与 raster present | [`h747_lab_raster_evidence.md`](docs/h747_lab_raster_evidence.md) |
| App Lab embedded/QSPI baseline | [`h747_lab_app_lab_smoke.md`](docs/h747_lab_app_lab_smoke.md) |
| Player MD3 board evidence | [`h747_lab_player_md3_smoke.md`](docs/h747_lab_player_md3_smoke.md) |
| Resident image roles | [`h747_lab_dynamic_boundary_roadmap.md`](docs/h747_lab_dynamic_boundary_roadmap.md) |

Host、QEMU、build-only 和 real-board 是不同证据域。具体地址、JEDEC、status token、接受条件和失败
分类只在对应 evidence/contract 与 capture script 中维护。

## Layout

| 路径 | Ownership |
|---|---|
| `board/h747_diy` | startup、linker、board fact、IRQ/clock 与 HAL/CubeMX adaptation |
| `capabilities` | project-local source constraint/value type，不是 App ABI 或 Charm Core |
| `runtime` | firmware startup 与 init graph execution |
| `services` | peripheral lifecycle 与 typed facade |
| `apps` | scenario/domain behavior；不直接拥有 HAL handle |
| `profiles` | board/runtime/service/app composition |
| `tools` | flash、capture、artifact 与 board diagnostics |
| `docs` | project contract 与 retained evidence |

每个 app 由 `apps/<name>/app.cmake` 声明 source，每个 firmware target 由
`profiles/<name>/profile.cmake` 选择 board、runtime、service 和 app。Verified board fact 可以迁移，
旧 project 目录不能作为新 target 的隐式 source dependency。
