# Charm 多战线母仓治理页

本页是当前仓库的治理与状态模型入口。

它只回答几个最容易失真的问题：

- 这个仓库现在同时活着哪些战线
- 它们各自为什么存在
- 谁在驱动谁
- 哪些内容是共享底座，不能被单条线私有化
- 三个月后回来看，应该先从哪里恢复上下文

如果你是第一次进入仓库，建议先读：

1. [`../README.md`](../README.md)
2. 本页
3. [`README.md`](README.md)

## 仓库级公开标签

后续讨论文档、脚本、workflow、示例、项目线时，统一使用下面两组标签：

- `track_kind`
  - `substrate`
  - `theory`
  - `pressure`
  - `landing`
  - `maintenance`
  - `archive`
- `track_status`
  - `active`
  - `exploring`
  - `maintained`
  - `archived`

### 标签语义

- `substrate`
  共享能力底座。被多条线复用，但不应被单条线偷偷改写成私有规则。
- `theory`
  方法论、词汇、结果物和解释面探索。
- `pressure`
  由真实需求持续施压的产品线或项目线。
- `landing`
  面向板级、SoC、runtime 证据与 bring-up 落地的验证线。
- `maintenance`
  已收口、以 blocker / 回归 / 验证链维护为主的子系统。
- `archive`
  历史阶段材料或已退出默认入口的旧路线。

## 当前活跃战线矩阵

| Track | Kind | Status | 角色 | 当前入口 |
|---|---|---|---|---|
| Shared substrate | `substrate` | `active` | 共享能力底座与公共语义面 | [`architecture_overview.md`](architecture_overview.md)、[`capability_map.md`](capability_map.md) |
| System compiler | `theory` | `exploring` | 当前最重要的方法论尝试，用于解释系统如何成立 | [`architecture/system_compiler_roadmap.md`](architecture/system_compiler_roadmap.md)、[`system/artifact_report_v0.md`](system/artifact_report_v0.md) |
| Player + Vivid | `pressure` | `active` | 真实需求压力线，用产品复杂度逼共享底座收敛 | [`../Examples/project/player/README.md`](../Examples/project/player/README.md)、[`../Examples/project/player/ARCHITECTURE_CONVERGENCE.md`](../Examples/project/player/ARCHITECTURE_CONVERGENCE.md) |
| RK3506 + minimal-kernel landing | `landing` | `active` | 板级 / SoC / runtime 证据与 bring-up 落地线 | [`system/minimal_kernel_runtime_evidence_bundle_contract.md`](system/minimal_kernel_runtime_evidence_bundle_contract.md)、[`../targets/rk3506/README.md`](../targets/rk3506/README.md) |
| POSIX v0 | `maintenance` | `maintained` | 已收口维护线，只接受 blocker / 回归 / 验证链修复 | [`system/posix_support_overview.md`](system/posix_support_overview.md)、[`system/posix_maintenance_mode_collaboration.md`](system/posix_maintenance_mode_collaboration.md) |

## 各战线说明

### Shared substrate

- `track_kind`: `substrate`
- `track_status`: `active`
- 角色：
  - 承载 `Modules/core/init/io/system/platform` 以及被多条线共同复用的公共能力面。
  - 提供依赖红线、装配规则、能力图、板级事实接入方式。
- 当前入口：
  - [`architecture_overview.md`](architecture_overview.md)
  - [`capability_map.md`](capability_map.md)
  - [`system/init_graph_contract.md`](system/init_graph_contract.md)
- 驱动力：
  - 被 `theory`、`pressure`、`landing` 三条线共同施压。
- 主要产物或证据：
  - `init.graph / init.materialize / init.observe`
  - `Channel / Reactor / Registry`
  - 板级 capability / BoardCaps / export 相关契约
- 不负责什么：
  - 不为任何单条线保留平行启动模型、平行装配模型或平行全局入口。
  - 不承诺某一条产品线就是仓库唯一宇宙中心。

### System compiler

- `track_kind`: `theory`
- `track_status`: `exploring`
- 角色：
  - 当前最重要的方法论尝试。
  - 目标是把“系统如何成立”解释清楚、举证清楚、暴露清楚。
- 当前入口：
  - [`architecture/system_compiler_roadmap.md`](architecture/system_compiler_roadmap.md)
  - [`architecture/system_compiler_vocabulary_v0.md`](architecture/system_compiler_vocabulary_v0.md)
  - [`system/artifact_report_v0.md`](system/artifact_report_v0.md)
  - [`system/explain_surface_v0.md`](system/explain_surface_v0.md)
- 驱动力：
  - 来自真实落地和真实项目的反向压力，而不是单独自我闭环。
- 主要产物或证据：
  - `artifact report`
  - `explain surface`
  - `resource contract`
  - `bringup evidence pipeline`
- 不负责什么：
  - 不是被神化的终局理论。
  - 不应脱离 `pressure` 或 `landing` 单独重写整个仓库叙事。
  - 不默认等于新的 DSL、codegen 平台或唯一未来配置格式。

### Player + Vivid

- `track_kind`: `pressure`
- `track_status`: `active`
- 角色：
  - 真实需求压力线。
  - 用产品复杂度和 UI/Audio/USB/存储组合把共享底座逼向真实可用形态。
- 当前入口：
  - [`../Examples/project/player/README.md`](../Examples/project/player/README.md)
  - [`../Examples/project/player/ARCHITECTURE_CONVERGENCE.md`](../Examples/project/player/ARCHITECTURE_CONVERGENCE.md)
  - [`ui/README.md`](ui/README.md)
- 驱动力：
  - 来自真实项目组织、页面结构、运行场景和多板级实现压力。
- 主要产物或证据：
  - Player profiles / runtime glue
  - Vivid helper 上收
  - 真实页面、真实启动链、真实板级场景
- 不负责什么：
  - 不是“随手示例”。
  - 不能反向把共享底座变成 Player 私有规则。
  - 不能继续默默保留平行启动模型和旁路装配。

### RK3506 + minimal-kernel landing

- `track_kind`: `landing`
- `track_status`: `active`
- 角色：
  - 板级 / SoC / runtime 证据与 bring-up 落地线。
  - 用 ARMv7-A QEMU 与 RK3506 真板路径验证共享语义和方法论能否撑过更复杂的平台。
- 当前入口：
  - [`system/minimal_kernel_runtime_evidence_bundle_contract.md`](system/minimal_kernel_runtime_evidence_bundle_contract.md)
  - [`system/minimal_kernel_host_smoke_bundle_contract.md`](system/minimal_kernel_host_smoke_bundle_contract.md)
  - [`../targets/rk3506/README.md`](../targets/rk3506/README.md)
  - [`board/rk3506/README.md`](board/rk3506/README.md)
- 驱动力：
  - 来自 runtime trap / syscall / handoff / board leaf target / early bring-up 的真实落地需求。
- 主要产物或证据：
  - minimal-kernel runtime evidence bundle
  - ARMv7-A QEMU smoke
  - RK3506 leaf target 与板级手册
- 不负责什么：
  - 不是单独宇宙。
  - minimal-kernel evidence 是连接方法论与落地线的验证轨，不应被误读为仓库唯一故事。
  - 不应反向定义所有共享能力都只能按 SoC bring-up 口径演进。

### POSIX v0

- `track_kind`: `maintenance`
- `track_status`: `maintained`
- 角色：
  - 已收口维护线。
  - 继续承接 blocker、回归和验证链维护。
- 当前入口：
  - [`system/posix_support_overview.md`](system/posix_support_overview.md)
  - [`system/posix_maintenance_mode_collaboration.md`](system/posix_maintenance_mode_collaboration.md)
- 驱动力：
  - 真实用户态阻塞点、QEMU smoke 回归、newlib/stdio 验证链稳定性。
- 主要产物或证据：
  - POSIX QEMU smoke
  - BusyBox / real-ELF / newlib 样例
  - 收口判定与维护协作文档
- 不负责什么：
  - 不是当前扩张前线。
  - 不再承接“看起来更像 Linux”的无边界扩面。

## 战线之间的关系

- `substrate` 是公共底座。
- `theory` 负责解释与语言收敛。
- `pressure` 负责用真实产品需求逼近问题。
- `landing` 负责用板级 / SoC / runtime 证据证明问题不是纸上谈兵。
- `maintenance` 负责守住已收口子系统，不让它们回到无边界扩张。

安全理解方式是：

- `theory` 给出当前最强解释语言。
- `pressure` 暴露“真实项目到底痛不痛”。
- `landing` 暴露“复杂平台到底站不站得住”。
- `substrate` 只接受被证明有必要的公共收敛。

## 当前默认阅读意图

如果你要理解：

- Charm 的共同语义面：
  - 先看 [`overview.md`](overview.md)
  - 再看 [`architecture_overview.md`](architecture_overview.md)
  - 再回到 [`capability_map.md`](capability_map.md)
- 当前的方法论探索：
  - 先看 [`architecture/system_compiler_roadmap.md`](architecture/system_compiler_roadmap.md)
  - 再看 [`system/artifact_report_v0.md`](system/artifact_report_v0.md)
- 真实产品压力线：
  - 先看 [`../Examples/project/player/README.md`](../Examples/project/player/README.md)
  - 再看 [`../Examples/project/player/ARCHITECTURE_CONVERGENCE.md`](../Examples/project/player/ARCHITECTURE_CONVERGENCE.md)
- 板级 / SoC 落地线：
  - 先看 [`system/minimal_kernel_runtime_evidence_bundle_contract.md`](system/minimal_kernel_runtime_evidence_bundle_contract.md)
  - 再看 [`../targets/rk3506/README.md`](../targets/rk3506/README.md)
- 维护态子系统：
  - 先看 [`system/posix_support_overview.md`](system/posix_support_overview.md)

## 后续工作准入规则

后续任何非微小改动，默认应先声明：

1. 这次改动由哪条战线驱动。
2. 它触碰了哪些 `substrate` 面。
3. 它依赖什么证据站住。

具体约束如下：

- `pressure` / `landing`
  - 可以逼 `substrate` 演进。
  - 不能默默开平行启动模型、平行装配模型、平行契约。
- `theory`
  - 可以定义词汇、结果物、解释面。
  - 不能脱离 `pressure` 或 `landing` 单独重写大范围仓库叙事。
- `maintenance`
  - 只接受 blocker、回归、验证链修复。
- `archive`
  - 保留追溯价值，但不返回默认入口。

## 不要怎么读这个仓库

- 不要把任意一条活跃战线误读成“唯一正确主线”。
- 不要把 `Examples/project/player` 视为普通示例区。
- 不要把 RK3506 / minimal-kernel 视为脱离仓库其它方法论的孤立实验。
- 不要把 `system compiler` 视为已经冻结的终局理论。
- 不要把维护态子系统重新拉回无边界扩张。
