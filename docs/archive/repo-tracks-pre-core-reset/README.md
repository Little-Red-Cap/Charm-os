# Core 收敛前的仓库战线快照

## 状态

本文归档核心治理重置前的多战线组织方式。它不描述当前优先级，也不定义 Charm 身份、Core 或 roadmap。

当时使用两组标签：

- `track_kind`: `substrate/theory/pressure/landing/maintenance/archive`
- `track_status`: `active/exploring/maintained/archived`

这些标签曾帮助区分共享底座、方法论、产品压力、板级落地和维护线；它们不是稳定架构词汇，当前不再要求新文档声明。

## 当时的战线

| 战线 | 当时用途 | 当前追溯入口 |
|---|---|---|
| Shared substrate | Core/init/IO/system/platform 的共享实现 | [`../../architecture_overview.md`](../../architecture_overview.md) |
| System compiler | artifact、explain、resource 与 bring-up 方法论探索 | [`../../architecture/system_compiler_roadmap.md`](../../architecture/system_compiler_roadmap.md)、[`../system-compiler-front-page-v0/README.md`](../system-compiler-front-page-v0/README.md) |
| Player + Vivid | 用真实产品复杂度暴露平台边界 | [`../../../Examples/project/player/README.md`](../../../Examples/project/player/README.md) |
| RK3506 + minimal-kernel | ARMv7-A/QEMU/SoC landing 与 runtime evidence | [`../../system/minimal_kernel_runtime_evidence_bundle_contract.md`](../../system/minimal_kernel_runtime_evidence_bundle_contract.md)、[`../../../targets/rk3506/README.md`](../../../targets/rk3506/README.md) |
| POSIX v0 | 用户态兼容和 QEMU 回归维护 | [`../../system/posix_support_overview.md`](../../system/posix_support_overview.md) |

表中的入口是追溯线索，不表示这些战线今天仍处于原状态。

## 保留的治理判断

- 真实产品和真实板可以向共享实现施压，但不能把 project/board 事实升级为 Core。
- 方法论必须接受消费者和机器证据检验，不能独立制造仓库主叙事。
- 已收口子系统应以 blocker、回归和证据维护为主，避免无边界扩张。
- 任何战线都不应建立平行启动模型、平行装配模型或新的全局 facade。
- 深层脚本、workflow 和示例不是默认阅读入口；应先从对应专题契约进入。

## 历史状态边界

`active`、`maintained` 及“当前最重要的方法论尝试”都是停线时点判断，不能作为当前状态。

当前路线以 Constitution、核心契约、源码和最近证据为准：

- [`../../../CONSTITUTION.md`](../../../CONSTITUTION.md)
- [`../../architecture/charm_core_contract.md`](../../architecture/charm_core_contract.md)
- [`../../README.md`](../../README.md)
