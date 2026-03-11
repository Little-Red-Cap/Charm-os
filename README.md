<div align="center">

# Charm

**C++26 Modules · Zero-alloc · constexpr config · Type-level FSM**

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![C++26](https://img.shields.io/badge/C%2B%2B-26-blue.svg?style=flat-square)](https://en.cppreference.com/w/cpp)
<br>
[![Clang Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-clang.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)
[![ARM Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-arm-none-eabi.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)

> 面向 MCU / PC 的统一能力图系统：IO / 系统 / 媒体 / UI **可装配、可裁剪、可验证**。

了解项目 推荐阅读顺序：
[本文章](README.md) →
[文档索引](docs/README.md) →
[入门指南](docs/overview.md) →
[架构能力](docs/capability_map.md) →
[架构总览](docs/architecture_overview.md) 

参与项目 or 开发者 推荐阅读顺序：
[架构能力](docs/capability_map.md) →
[架构总览](docs/architecture_overview.md) →
[协作文档](docs/agent/README.md)

</div>

---

## 为何诞生
**Charm** 是一个面向 **嵌入式** 的软件系统框架。 它尝试解决嵌入式开发中的一个常见问题：

> 每个项目都在重复实现 IO、调度、日志、文件系统、UI 等基础设施。

Charm 的做法是：

**把这些能力统一为可复用的 Capability，并通过能力图进行装配。**

核心思想：

```

Capability Graph

* Non-blocking IO
* Deterministic Assembly

```

---

## Capability Overview

Charm 提供一组可组合的系统能力。

| Domain         | Key Capabilities                            |
|----------------|---------------------------------------------|
| System         | InitGraph · EDA Scheduler · Sync primitives |
| IO             | Channel · Reactor · Registry                |
| Debug / Output | Out formatting · Logging                    |
| Storage        | VFS                                         |
| USB            | USB Device framework                        |
| UI             | Ink (lightweight UI) · Vivid (rich UI)      |
| Media          | Audio pipeline                              |
| Platform       | HAL drivers                                 |

* 完整能力列表 → **[Capability Map](docs/capability_map.md)**




## ✨ 核心特性

<table>
<tr>
<td width="50%">

### 🚀 零成本抽象
- 编译期格式化解析
- 未启用的日志完全消失
- 域过滤编译期决定
- 可选功能按需编译

</td>
<td width="50%">

### 🛡️ 类型安全
- 编译期类型检查
- 参数数量验证
- 无隐式转换陷阱
- 格式字符串验证

</td>
</tr>
<tr>
<td width="50%">

### 🔒 嵌入式友好
- 无异常（`std::expected`）
- 无堆分配
- 无虚函数
- C++ Modules

</td>
<td width="50%">

### 🎨 功能丰富
- 事件驱动内核
- IPC 工具
- UI 系统
- USB / FileSystem

</td>
</tr>
</table>

---


开发友好：可在PC上运行
全部采用C++ Module组织代码
事件驱动型内核
提供IPC工具
组件丰富

## 设计关键词
- **能力图装配**：所有底层能力通过 `init.graph` 注册与启动
- **非阻塞 IO**：`Channel + Reactor + Registry` 三件套统一入口
- **单入口模块**：`charm.foundation / charm.runtime / charm.domain` 约束依赖边界
- **零动态内存**：默认固定容量与零分配策略
- **可观测**：统一 trace/诊断入口


---

## 设计关键词

Charm 的系统设计围绕以下几个原则：

### 能力图装配

所有系统能力通过 `init.graph` 注册并统一启动。

系统初始化过程由能力依赖图驱动。

---


### 非阻塞 IO

统一 IO 模型：

```

Channel
↓
Reactor
↓
Registry

```

所有 IO 组件通过 Reactor 事件分发。

---

### 单入口模块

系统依赖被限制为三层：

```

charm.foundation
↓
charm.runtime
↓
charm.domain

````

禁止跨层依赖。

---

### 零动态内存

默认策略：

- 固定容量
- 无动态分配
- 可选 constexpr 配置

---

### 可观测

系统统一提供：

- trace
- 统计
- 诊断接口

---

## 快速上手

Charm 可以在 PC 与 MCU 上运行。

<details>
<summary><b>运行环境</b></summary>

* [CMake 4.1.2]() （CMake版本会影响构建行为，过低版本对C++ Module支持不好）
* [Ninja]()
* [PC 编译器 Clang/MinGW]() （未测试MSVC）
* [MCU 编译器 GCC-ARM-None 15.2]() （若只运行PC端可不用）
* 代码依赖：无第三方依赖，已集成到源码

</details>

## 推荐验证链

Charm 提供一个最小验证链，用于验证 block.device → VFS → out 的完整能力路径：

```
Win file image → block.file → block.registry
→ FatFsMount → VFS → out
```

运行 demo：

```
fs-block-vfs-demo <disk.img>
```

镜像中需包含一个 FAT 分区与 `hello.txt` 文件。



最小示例：MCU接入UART/PC接入Stdio

### PC (Windows)

<details>
<summary><b>示例</b></summary>

```cpp
// TODO
````

* 构建命令
```shell
// TODO
````



</details>


### MCU (STM32)

<details>
<summary><b>示例</b></summary>

```cpp
// TODO
```

</details>

---

<div align="center">

问题反馈：[GitHub Issues](https://github.com/Little-Red-Cap/Charm-os/issues)
<br>
**⭐ 如果这个项目对你有帮助，请给个 Star！**

[回到顶部](#charm)

</div>
