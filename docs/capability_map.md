# Charm Capability Map

Charm 将系统功能组织为 **Capability（能力）**，并通过能力图进行装配。

这份文档是 **开发者视角的能力索引入口**：
用于回答“Charm 已经有哪些能力、该看哪里、该从哪个例子开始”。

如果你是第一次接触 Charm，建议先阅读：

**README → `docs/overview.md` → `docs/README.md` → 本文 → `docs/architecture_overview.md`**

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

当你准备新增功能、接入子系统或开始复用框架能力时，建议按以下顺序使用本文：

1. 先查 Capability 是否已存在
2. 再看对应 Modules / Docs / Example
3. 若已有能力满足需求，优先复用
4. 若没有，再考虑新增 Capability 或扩展现有能力

---

## 自动生成结果

本文是 **人工维护的开发者索引**。
与之对应，代码扫描生成的结构化结果位于：

- `docs/generated/capability_map.generated.md`
- `docs/generated/capability_graph.generated.mmd`
- `docs/generated/capability_data.generated.json`

建议理解为：

- **本文**：面向人类阅读的能力入口
- **generated/**：面向机器提取的结构化结果

---

## Capability Index

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
- 需要查看代码扫描生成结果：`docs/generated/capability_map.generated.md`
- 需要查看能力依赖图：`docs/generated/capability_graph.generated.mmd`
