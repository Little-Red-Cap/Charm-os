# Charm Capability Map

Charm 将系统功能组织为 **Capability（能力）**，并通过能力图进行装配。

一个 Capability 是框架提供的一项 **可复用系统能力**，通常由若干模块实现。

# 能力分层

Charm 使用三层结构组织能力：

```

Capability Group → Capability → Module

```

- **Capability Group**  
  能力分组，用于宏观分类。

- **Capability**  
  框架提供的一项可复用能力。

- **Module**  
  Capability 的实现组件，通常对应源码目录。

---

# Capability Groups

当前 Charm 的能力域：

| Group    | 描述           |
|----------|--------------|
| Core     | 核心算法与基础设施    |
| System   | 系统装配、调度与模块管理 |
| IO       | IO 抽象与事件驱动系统 |
| Storage  | 文件系统与数据存储    |
| USB      | USB 设备框架     |
| UI       | 用户界面系统       |
| Media    | 多媒体处理        |
| Platform | 硬件抽象层        |

---

# Capability Map

## System

| Capability  | 描述        | 主要模块            |
|-------------|-----------|-----------------|
| InitGraph   | 系统能力装配图   | system/init     |
| EDA         | 事件驱动调度模型  | kernel/eda      |
| SyncWait    | 同步与等待原语   | kernel/sync     |
| DeviceModel | 设备驱动注册模型  | device.registry |
| ModuleX     | 模块加载与依赖管理 | module.loader   |
| Bootloader  | 固件升级与启动策略 | boot.*          |

---

## IO

| Capability | 描述         | 主要模块        |
|------------|------------|-------------|
| Channel    | 非阻塞字节通道    | io.channel  |
| Reactor    | 事件驱动 IO 分发 | io.reactor  |
| Registry   | IO 能力注册中心  | io.registry |
| Out        | 统一输出与日志系统  | out.core    |
| Shell      | 命令行接口      | io.shell    |
| HAL        | 硬件抽象层      | io.hal      |

---

## Storage

| Capability | 描述     | 主要模块   |
|------------|--------|--------|
| VFS        | 虚拟文件系统 | fs.vfs |

---

## USB

| Capability | 描述        | 主要模块       |
|------------|-----------|------------|
| USBDevice  | USB 设备协议栈 | usb.device |

---

## UI

| Capability | 描述       | 主要模块     |
|------------|----------|----------|
| Ink        | 轻量 UI 系统 | ui.ink   |
| Vivid      | 富 UI 系统  | ui.vivid |

---

## Media

| Capability | 描述     | 主要模块        |
|------------|--------|-------------|
| Audio      | 音频处理管线 | media.audio |

---

## Core

| Capability | 描述      | 主要模块       |
|------------|---------|------------|
| Trace      | 系统追踪与诊断 | trace.core |
| Algorithms | 基础算法库   | alg.*      |

---

# 使用 Capability Map

开发新功能时建议流程：

1. 查询 Capability Map
2. 判断是否已有可复用能力
3. 优先复用现有 Capability
4. 若无，再新增 Capability

---

# 文档入口

推荐阅读顺序：

```

README
↓
docs/overview.md
↓
docs/capability_map.md
↓
docs/architecture_overview.md

```

---

# 演进

Capability Map 会随着 Charm 的发展持续扩展。

未来可能新增能力：

- Network stack
- OTA framework
- GPU / display pipeline
- Distributed capability graph
