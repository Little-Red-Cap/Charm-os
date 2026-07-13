# Core 收敛前的仓库战线快照

> `status`: `archived`

本文记录核心治理重置前的多战线分类，不描述当前优先级、Charm 身份或 roadmap。当时的
`track_kind/status` 只用于区分 substrate、theory、pressure、landing、maintenance 与 archive，现已停用。

## 历史战线

| 战线 | 当时用途 | 当前追溯入口 |
|---|---|---|
| Shared substrate | Core/init/IO/system/platform 共享实现 | [`architecture_overview.md`](../../architecture_overview.md) |
| System compiler | artifact、explain、resource 与 bring-up 探索 | [`system_compiler_roadmap.md`](../../architecture/system_compiler_roadmap.md) |
| Player + Vivid | 以产品复杂度暴露平台边界 | [`Player README`](../../../Examples/project/player/README.md) |
| RK3506 + minimal-kernel | ARMv7-A/QEMU/SoC landing 与 runtime evidence | [`runtime evidence`](../../system/minimal_kernel_runtime_evidence_bundle_contract.md)、[`RK3506`](../../../targets/rk3506/README.md) |
| POSIX v0 | 用户态兼容与 QEMU regression | [`posix_support_overview.md`](../../system/posix_support_overview.md) |

表中状态和入口只用于追溯，不表示这些战线今天仍以原方式存在。

## 保留判断

- 产品与真实板可以向共享实现施压，但 project/board 事实不能升级为 Core。
- 方法论必须接受真实 consumer 和 machine evidence 检验，不能独立制造主叙事。
- 已收口子系统按 blocker、regression 和 evidence 维护，避免无边界扩张。
- 战线不得建立平行 boot/composition 模型或新的全局 façade。
- 深层 script/workflow/example 不是默认阅读入口，应从专题 contract 进入。

当前判断以 [`CONSTITUTION.md`](../../../CONSTITUTION.md)、
[`charm_core_contract.md`](../../architecture/charm_core_contract.md)、source 和最近证据为准。
