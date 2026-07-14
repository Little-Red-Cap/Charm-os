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

Profile 与 target 的完整集合以 [`CMakePresets.json`](CMakePresets.json)、`profiles/*/profile.cmake` 和
CMake configure 输出为准，不在 README 复制清单。

## Build

从本目录使用 [`CMakePresets.json`](CMakePresets.json) 配置和构建，并复用所选 preset 的同一个
`cmake-build-*`。Target/preset 名称以该文件为准，不在 README 复制命令清单。切换 compiler/binutils
时重新配置，避免 cache 混用旧 linker/archiver；不要并发构建同一个 Ninja binary directory。

Arm GNU 查找与 `H747_LAB_ARM_GNU_TOOLCHAIN_ROOT` override 由 toolchain file 定义。

Configure 会先运行 H747 BSP doctor：foundation profile 只要求其实际 source set；依赖 I2C、storage、
FMC/SDRAM、QSPI 或 SPI 的 profile 缺失对应 generated source 时明确失败。这类失败属于 BSP source
前置条件，不是 ELF、AppRuntime、packetstream 或 Store regression。外部 BSP 只能通过显式
`DRAFT_ROOT`/cache boundary 接入，不把 board fact 复制回 Charm。

Host fixture 也使用调用方拥有的单一 `cmake-build-*`；executable、CTest artifact 和 `.ppm` evidence
不写入源码树。

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

目录 ownership、profile/app source 与迁移边界只在
[`h747_lab_layering_contract.md`](docs/h747_lab_layering_contract.md) 维护。
