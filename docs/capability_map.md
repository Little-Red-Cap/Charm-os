# Charm Capability Map

Charm 将系统功能组织为 **Capability（能力）**，并通过能力图进行装配。

这份文档是 **开发者视角的能力索引入口**：
用于回答“Charm 已经有哪些能力、该看哪里、该从哪个例子开始”。

如果你是第一次接触 Charm，建议先阅读：

**README → `docs/overview.md` → `docs/README.md` → 本文 → `docs/architecture_overview.md`**

如果你现在还不知道应该先用哪个能力，先停在下面这张首用决策表。完整能力索引在后面。

## 首用决策表

| 你现在要做什么 | 先用什么 | 默认路径 | 先看哪里 | 什么时候才例外 |
|---|---|---|---|---|
| 先把输出/日志接上 | `out.core` / `out.format` / `out.logger` | `io.console0 -> out.channel -> out.api/out.logger` | [`Examples/io/out/README.md`](../Examples/io/out/README.md) | 只有在 `io.console0` 尚未就绪时，才临时用板级 EarlyConsole |
| 先做命令行/REPL | `io.shell` | `shell/REPL service` | [`Examples/shell/README.md`](../Examples/shell/README.md) | 不要把局部命令直接抬成长期协议 |
| 先装配系统 | `system.init` | `AppHost / CoreSystemChain / init.graph` | [`docs/system/init_graph_contract.md`](system/init_graph_contract.md) | 不要手写初始化顺序 |
| 先把板级 console / clock 接到 Charm | `platform.*` + `BoardCaps / ConsoleCaps / ClockDesc` | `board landing` 到 `io.console0` / `clock` | [`docs/system/system_coordination_contract_v0.md`](system/system_coordination_contract_v0.md) | 只有 pre-graph / fault / 极早期证据才走 EarlyConsole |
| 先让 board service 进入系统协调层 | `ServiceSnapshotContract` / `PowerProfile` | `snapshot/status -> sys services` | [`docs/system/system_coordination_contract_v0.md`](system/system_coordination_contract_v0.md) | 不要把 raw write 或重 runtime 直接塞进系统层 |
| 先接块设备 / 文件系统 | `storage.block` / `fs.vfs` | `block.device -> registry -> fs.vfs` | [`docs/storage/block_device_contract.md`](storage/block_device_contract.md) / [`docs/storage/fs_vfs_mount_rules.md`](storage/fs_vfs_mount_rules.md) | 不要跳过 block 直接谈挂载 |
| 先做 USB 设备 | `usb.device` | `device_driver -> class` | [`docs/usb/usb_arch_plan.md`](usb/usb_arch_plan.md) | 不要先把 host/runtime 全拉进来 |
| 先做设备发现 / 注册 | `device.registry` / `io.registry` | `registry + init.graph` | [`docs/architecture/device_model_overview.md`](architecture/device_model_overview.md) / [`docs/io/io_registry_contract.md`](io/io_registry_contract.md) | 不要绕过 registry 直连全局能力 |

---

## 本文负责什么

本文负责：

- 给出 Charm 的能力索引
- 帮助开发者快速发现可复用能力
- 为每个 Capability 提供代码、文档、示例、状态入口
- 指向自动生成的能力图结果

本文不负责：

- 解释完整架构骨架与依赖红线（见 `docs/architecture_overview.md`）
- 替代专题文档或模块契约文档
- 作为代码扫描结果的唯一真相来源

---

## 能力分层

Charm 使用三层结构组织系统能力：

`Capability Group → Capability → Module`

- **Capability Group**
  能力分组，用于宏观分类。

- **Capability**
  框架提供的一项可复用能力。

- **Module**
  Capability 的实现模块或代码入口。

---

## 使用方式

当你准备新增功能、接入子系统或开始复用框架能力时：

1. 先查 Capability 是否已存在
2. 先看上面的首用决策表，确认默认路径
3. 再看对应 Modules / Docs / Example
4. 若已有能力满足需求，优先复用
5. 若没有，再考虑新增 Capability 或扩展现有能力

## 首用决策表

| 现在想做什么 | 先看什么 | 默认路径 | 什么时候允许例外 |
|---|---|---|---|
| 找“有没有现成能力” | 本文的 Capability Index | `docs/capability_map.md` -> 对应 group -> 对应 docs/example | 只有当现有能力确实不满足时，才考虑新增能力 |
| 判断“能力应该归哪里” | group 说明 + 对应条目 | 先按当前 group 的现行契约理解 | 只有在讨论分层、归属或默认装配时，才转向 architecture 路由 |
| 复用“默认能力入口” | capability 条目里的 Docs / Example | 先走本文给出的现成文档与示例 | 只有当接口缺失或语义不清时，才去追更底层实现 |
| 追“当前主入口是什么” | [`docs/README.md`](README.md) + [`docs/architecture_overview.md`](architecture_overview.md) | 先用总入口，再回到本文 | 只有明确是在做专题设计时，才直接跳专题文档 |

---

## 自动生成结果

本文是 **人工维护的开发者索引**。
与之对应，代码扫描生成的结构化结果默认输出到 `docs/generated/`，说明见：

- `docs/generated/README.md`
- `capability_map.generated.md`
- `capability_graph.generated.mmd`
- `capability_data.generated.json`

建议理解为：

- **本文**：面向人类阅读的能力入口
- **generated/**：面向机器提取的结构化结果

---

## 完整能力索引

### 分组说明

| Group | 说明 |
|---|---|
| Core | 核心算法、基础设施与可观测能力 |
| System | 系统装配、调度、设备模型、模块管理 |
| IO | 非阻塞 IO、事件分发、输出与硬件抽象 |
| Storage | 文件系统与存储抽象 |
| USB | USB 设备侧能力 |
| UI | 用户界面能力 |
| Media | 多媒体处理能力 |
| Platform | 平台与板级底座能力 |

---

### Core

| Capability | Description | Modules | Docs | Example | Status |
|---|---|---|---|---|---|
| Trace | 系统追踪、统计与诊断能力 | `trace.core` | `docs/trace/trace_core_entry.md` | — | draft |
| Algorithms | 通用算法与数据处理基础能力 | `alg.core` | — | — | draft |

---

### System

| Capability | Description | Modules | Docs | Example | Status |
|---|---|---|---|---|---|
| InitGraph | 系统能力统一装配入口 | `system.init` | `docs/system/init_graph_contract.md` | — | stable |
| EDA | 事件驱动调度模型 | `kernel.eda` | — | — | stable |
| Sync | 同步与等待原语 | `kernel.sync` | — | — | draft |
| DeviceModel | 设备/驱动注册与组织模型 | `device.registry` | `docs/architecture/device_model_overview.md` | — | draft |
| ModuleX | 模块加载、链接与依赖管理 | `module.loader` | — | — | draft |
| Bootloader | 启动、升级与引导策略 | `boot.*` | `docs/boot/bootloader_overview.md` | — | draft |
| POSIXCompat | 面向 Linux 用户态程序的最小兼容执行面，覆盖 fd/pipe/spawn/wait/ELF/errno，代码位于 `Modules/io/posix` | `posix.api`, `posix.proc`, `posix.exec_context`, `posix.exec_loader`, `posix.elf_hostcall` | `docs/system/posix_support_overview.md` | `Examples/kernel/posix/qemu` | draft |

---

### IO

| Capability | Description | Modules | Docs | Example | Status |
|---|---|---|---|---|---|
| Channel | 非阻塞字节通道抽象 | `io.channel` | `docs/io/io_channel_contract.md` | — | stable |
| Reactor | 事件驱动 IO 分发能力 | `io.reactor` | `docs/io/io_reactor_contract.md` | — | stable |
| Registry | 能力注册与发现 | `io.registry` | `docs/io/io_registry_contract.md` | — | stable |
| Out | 统一输出、格式化与日志能力 | `out.core` | — | — | stable |
| Shell | 命令行、REPL 与交互入口 | `io.shell` | — | — | draft |
| HAL | 硬件抽象层能力 | `io.hal` | `docs/io/io_layering_overview.md` | — | draft |
| Input | 原始输入采样与事件泵链路 | `input.service` | `docs/input/input_layering_decision.md` | `input_pump_win_demo` | draft |

---

### Storage

| Capability | Description | Modules | Docs | Example | Status |
|---|---|---|---|---|---|
| BlockDevice | 块设备抽象与适配 | `storage.block` | `docs/storage/block_device_contract.md` | `fs_block_vfs_demo` | draft |
| VFS | 虚拟文件系统与挂载入口 | `fs.vfs` | `docs/storage/fs_vfs_mount_rules.md` | `fs_block_vfs_demo` | draft |

---

### USB

| Capability | Description | Modules | Docs | Example | Status |
|---|---|---|---|---|---|
| USBDevice | USB 设备协议与装配能力 | `usb.device` | `docs/usb/usb_arch_plan.md` | — | draft |

---

### UI

| Capability | Description | Modules | Docs | Example | Status |
|---|---|---|---|---|---|
| Ink | 轻量级 UI 能力 | `ui.ink` | `docs/ui/player_ui.md` | — | draft |
| Vivid | 富 UI 能力 | `ui.vivid` | — | — | draft |

---

### Media

| Capability | Description | Modules | Docs | Example | Status |
|---|---|---|---|---|---|
| Audio | 音频处理与播放管线 | `media.audio` | `docs/system/av_pipeline_overview.md` | — | draft |

---

### Platform

| Capability | Description | Modules | Docs | Example | Status |
|---|---|---|---|---|---|
| PlatformCaps | 板级能力与平台接入底座 | `platform.*` | — | — | draft |

---

## Status 说明

建议统一使用以下状态值：

- `stable`：接口与用法已基本稳定，推荐优先复用
- `draft`：已有能力与实现，但接口或文档仍在演进
- `planned`：有明确方向，但尚未形成稳定可复用入口
- `internal`：内部支撑能力，不建议直接作为外部能力依赖

---

## 维护约定

新增或调整 Capability 时，至少同步检查：

- 本文的 Capability Index 是否需要更新
- `docs/generated/*` 是否需要重新生成
- README 中的 Capability Overview 是否需要调整
- `docs/architecture_overview.md` 是否仍只保留架构骨架而未重新膨胀
- 相关示例是否需要补链

---

## 下一步阅读

- 需要快速找文档：`docs/README.md`
- 需要理解系统骨架与依赖红线：`docs/architecture_overview.md`
- 需要查看代码扫描生成结果：`docs/generated/README.md`
- 需要查看能力依赖图：`docs/generated/README.md`
