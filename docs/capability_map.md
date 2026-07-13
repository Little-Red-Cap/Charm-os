# Capability 实现索引

## 文档角色

本文是 `supporting` 任务路由，帮助定位仓库已有实现。它不是 Charm Core 的能力注册表，也不证明某个模块、capability name 或状态已经获准成为公共契约。

涉及 Core 准入时进入 [`capability route`](agent/routes/capability.md)。

## 按任务进入

| 任务 | 当前实现入口 | 文档/证据入口 |
|---|---|---|
| 判断概念能否进入 Core | Requirement / Provision / Binding | [`architecture/charm_core_contract.md`](architecture/charm_core_contract.md) |
| 装配静态系统 | `init.graph`、`CoreSystemChain`、bring-up | [`system/init_graph_contract.md`](system/init_graph_contract.md) |
| 接入 console 与字节流 | `io.channel`、`io.registry`、`io.reactor`、`out.*` | [`io/README.md`](io/README.md) |
| 接入 block/VFS | `block.device`、`block.registry`、`fs_vfs` | [`storage/README.md`](storage/README.md) |
| 接入固定控制器 | `BoardCaps`、HAL binding、init graph | [`architecture/driver_model.md`](architecture/driver_model.md) |
| 接入运行期设备 | `device::Bus/Registry/Driver`、stable slot export | [`architecture/driver_model.md`](architecture/driver_model.md) |
| 下载、存储并运行 App image | received/QSPI/eMMC -> loader -> AppRuntime | [`architecture/resident_image_platform_v1_contract.md`](architecture/resident_image_platform_v1_contract.md) |
| 验证 minimal-kernel | host semantic evidence + ARMv7-A/QEMU evidence | [`system/minimal_kernel_runtime_evidence_bundle_contract.md`](system/minimal_kernel_runtime_evidence_bundle_contract.md) |
| 运行 POSIX/ELF 样本 | POSIX facade 与 same-address-space runtime | [`system/posix_support_overview.md`](system/posix_support_overview.md) |
| 使用 ModuleX | loader/linker 与 App image format | [`system/README.md`](system/README.md) |
| 接入 UI | Ink/Vivid runtime 与 backend | [`ui/README.md`](ui/README.md) |
| 接入 Audio | media/audio pipeline 与 backend | [`audio/README.md`](audio/README.md) |
| 接入真实板或 QEMU | platform/target/project profile | [`project/README.md`](project/README.md) |

这张表只指向实现路径。具体行为、错误和生命周期由专题契约决定。

## Capability 证据

声称“仓库提供某项 capability”至少要区分：

| 证据 | 能证明什么 | 不能证明什么 |
|---|---|---|
| module/type 存在 | 有代码入口 | API 稳定、跨平台可用 |
| capability name 出现在 init node | 有 provider/requirement 字符串 | 解析、启动和失败语义完整 |
| host fake/smoke | 平台无关调用语义可运行 | QEMU 或真板硬件成立 |
| QEMU smoke | 特定虚拟 target 的机器路径成立 | 真板时序、DMA、cache、外设成立 |
| board log | 特定固件和板级组合运行过 | 其它 profile、版本或平台成立 |
| canonical 裁决 | 概念获准进入 Core | 每个 backend 已完成 |

因此本文不维护 `stable/draft/active` 手工状态表。状态必须由对应契约和最近证据给出。

## 生成结果

生成命令、输出路径和启发式扫描限制只在 [`generated/README.md`](generated/README.md) 维护。
生成文件是源码 inventory，不是 Capability Contract、完整依赖图或准入证据。

## 新增或扩展能力

按 [`capability route`](agent/routes/capability.md) 完成准入与归属判断；本页只在实现和证据入口已经
存在后更新索引，不承载候选契约正文。

## 非目标

- 不把目录分组当作 Core taxonomy。
- 不把 generated graph 当作 system compiler 或完整依赖图。
- 不为所有模块维护人工状态广告。
- 不用 Capability 名义包装 project-specific service、driver 或 backend。
