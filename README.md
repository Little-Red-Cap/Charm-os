<div align="center">

# ✨ Charm ✨

**C++26 Modules · Capability Graph · Non-blocking IO · Evidence-first Bring-up**

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![C++26](https://img.shields.io/badge/C%2B%2B-26-blue.svg?style=flat-square)](https://en.cppreference.com/w/cpp)
<br>
[![Clang Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-clang.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)
[![ARM Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-arm-none-eabi.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)

</div>

Charm 不是单一 demo、单一板级工程，也不是只围绕某一个子系统展开的仓库。

它更像一个**多战线母仓**：

- 一边在探索“嵌入式系统如何被更清楚地解释、举证和组织”
- 一边用真实项目压力逼共享能力收敛
- 一边把能力压到板级 / SoC / runtime 证据链上做落地验证

如果你三个月后再回来看，最先该恢复的不是某个模块细节，而是：

1. 现在活着哪些战线
2. 它们各自为什么存在
3. 谁在驱动谁
4. 你这次该从哪里继续

## 从这里开始

- 仓库治理与战线状态：
  - [`docs/repo_governance.md`](docs/repo_governance.md)
- 当前战线浅索引：
  - [`docs/current_tracks_index.md`](docs/current_tracks_index.md)
- 文档总路由：
  - [`docs/README.md`](docs/README.md)

## 如果你想理解什么

### 我想先理解 Charm 的共同语义面

按这个顺序读：

1. [`docs/overview.md`](docs/overview.md)
2. [`docs/architecture_overview.md`](docs/architecture_overview.md)
3. [`docs/capability_map.md`](docs/capability_map.md)
4. [`docs/system/init_graph_contract.md`](docs/system/init_graph_contract.md)

你会看到的关键词是：

- `Capability`
- `init.graph`
- `Channel / Reactor / Registry`
- 共享能力底座 `substrate`

### 我想看当前的方法论探索

按这个顺序读：

1. [`docs/architecture/system_compiler_roadmap.md`](docs/architecture/system_compiler_roadmap.md)
2. [`docs/architecture/system_compiler_vocabulary_v0.md`](docs/architecture/system_compiler_vocabulary_v0.md)
3. [`docs/system/artifact_report_v0.md`](docs/system/artifact_report_v0.md)
4. [`docs/system/explain_surface_v0.md`](docs/system/explain_surface_v0.md)

这条线当前的标签是：

- `track_kind`: `theory`
- `track_status`: `exploring`

它是当前最重要的方法论尝试，但不是被神化的终局。

### 我想看真实项目如何逼仓库收敛

按这个顺序读：

1. [`Examples/project/player/README.md`](Examples/project/player/README.md)
2. [`Examples/project/player/ARCHITECTURE_CONVERGENCE.md`](Examples/project/player/ARCHITECTURE_CONVERGENCE.md)
3. [`docs/ui/README.md`](docs/ui/README.md)

这条线当前的标签是：

- `track_kind`: `pressure`
- `track_status`: `active`

`Player` 不是噪音示例，而是当前最强真实压力线之一。

### 我想看板级 / SoC / runtime 证据落地

按这个顺序读：

1. [`docs/system/minimal_kernel_runtime_evidence_bundle_contract.md`](docs/system/minimal_kernel_runtime_evidence_bundle_contract.md)
2. [`docs/system/minimal_kernel_host_smoke_bundle_contract.md`](docs/system/minimal_kernel_host_smoke_bundle_contract.md)
3. [`targets/rk3506/README.md`](targets/rk3506/README.md)
4. [`docs/board/rk3506/README.md`](docs/board/rk3506/README.md)

这条线当前的标签是：

- `track_kind`: `landing`
- `track_status`: `active`

`minimal-kernel evidence` 不是单独宇宙，而是连接方法论与落地线的验证轨。

### 我想进入维护态子系统

按这个顺序读：

1. [`docs/system/posix_support_overview.md`](docs/system/posix_support_overview.md)
2. [`docs/system/posix_maintenance_mode_collaboration.md`](docs/system/posix_maintenance_mode_collaboration.md)

这条线当前的标签是：

- `track_kind`: `maintenance`
- `track_status`: `maintained`

`POSIX v0` 已收口，不再是默认扩张前线。

## 当前仓库怎么理解最安全

- `system compiler`：当前最重要的方法论尝试
- `Player + Vivid`：真实需求压力线
- `RK3506 + minimal-kernel landing`：板级 / SoC / runtime 落地线
- `POSIX v0`：维护线
- `Modules/core/init/io/system/platform`：共享底座 `substrate`

这几条线可以互相施压，但不能互相偷换定义。

## 构建入口

当前建议优先使用 CMake Presets，而不是手写本地构建目录。

- 主机调试：`cmake --preset host-debug`
- 主机构建：`cmake --build --preset host-debug`
- 主机 GCC16 调试：`cmake --preset host-gcc16`
- 主机 GCC16 构建：`cmake --build --preset host-gcc16`
  默认关闭 `WinSock` 后端，优先作为 GCC16 / C++26 模块与语义实编入口
- RK3506 最小镜像：`cmake --preset rk3506-baremetal-image-uart0-minimal-debug`
- RK3506 最小镜像构建：`cmake --build --preset rk3506-baremetal-image-uart0-minimal-debug`

更细的脚本 / workflow / 示例入口，请回到：

- [`docs/current_tracks_index.md`](docs/current_tracks_index.md)

## 协作入口

- 仓库第一跳约定：
  - [`AGENTS.md`](AGENTS.md)
- Agent 第二跳任务卡片：
  - [`docs/agent/routes/README.md`](docs/agent/routes/README.md)
- 文档维护：
  - [`docs/documentation_maintenance.md`](docs/documentation_maintenance.md)

## 不要怎么读

- 不要先把 `docs/` 当成“按文件名漫游”的资料堆。
- 不要把任意一条活跃战线误读成仓库唯一主角。
- 不要把 `Player` 当普通示例区。
- 不要把 `system compiler` 当已经冻结的最终理论。
- 不要把维护态材料重新当成默认前线入口。
