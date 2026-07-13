# Charm 实现地图

## 文档状态

- `status`: `supporting`
- `scope`: 当前源码分区、公共入口与主要运行路径
- `authority`: [`CONSTITUTION.md`](../CONSTITUTION.md)

本文回答代码放在哪里、从哪里进入。它不定义 Charm Core，也不记录易失效的构建或测试进度。
旧版阶段盘点见 [`architecture-inventory-v0`](archive/architecture-inventory-v0/README.md)。

## 仓库结构

| 路径 | 用途 |
|---|---|
| `Modules/core` | util、semantic、init、trace、service 与通用算法 |
| `Modules/system` | kernel、boot、bring-up、device、ModuleX、power 与 RTOS 适配 |
| `Modules/io` | channel、reactor、HAL、block、FS、USB、network、POSIX、shell 与 out |
| [`Modules/platform`](../Modules/platform/README.md) | platform/board 描述与 host 实现 |
| `Modules/media`、`Modules/gfx`、`Modules/ui` | media、图形与 UI 子系统 |
| `Modules/control` | 控制领域代码，不属于 Core |
| `Modules/thirdparty` | vendored 第三方源码 |
| `Examples` | 样本、板级工程与语义验证 |
| `Backends` | capability/backend 实验与契约 |
| `Draft` | 未冻结探索 |
| `targets` | 架构 lower-half 与目标构建材料 |

目录只表示当前所有权，不授予 Core 身份。

## 导入入口

CMake 台账当前分类的稳定聚合入口是：`charm.core`、`charm.system`、`charm.io`、
`charm.net`、`charm.media`、`charm.media.audio`、`charm.ui.ink` 和 `charm.ui.vivid`。

叶子 module 能准确表达依赖时优先使用叶子 module。兼容、退役和内部入口见
[`entry_surface_contract.md`](architecture/entry_surface_contract.md)；实际依赖以 module import 和
CMake source collection 为准。

## 装配路径

板级已知能力通过静态初始化路径进入系统：

```text
board facts -> init.graph -> registry/service -> consumer
```

运行期发现设备使用独立的动态路径：

```text
bus -> DeviceDesc -> registry/driver -> stable slot or manager -> consumer
```

两者不能互相替代。静态约束见 [`init_graph_contract.md`](system/init_graph_contract.md)，动态设备
边界见 [`driver_model.md`](architecture/driver_model.md)。

## 主要运行路径

| 目标 | 路径 | 入口 |
|---|---|---|
| 静态 bring-up | board facts -> init graph -> service/registry | [`system/README.md`](system/README.md) |
| 字节 IO | HAL/backend -> channel -> registry/reactor -> protocol | [`io/README.md`](io/README.md) |
| block / FS | block device -> block registry -> VFS/mount | [`storage/README.md`](storage/README.md) |
| resident App | image source/store -> loader -> AppRuntime -> capability table | [`resident image contract`](architecture/resident_image_platform_v1_contract.md) |
| POSIX | image/process facade -> same-address-space runtime | [`POSIX overview`](system/posix_support_overview.md) |
| minimal kernel | host semantics + ARMv7-A/QEMU machine evidence | [`runtime evidence`](system/minimal_kernel_runtime_evidence_bundle_contract.md) |
| UI / Audio | runtime -> display/input/audio backend | [`ui/README.md`](ui/README.md)、[`audio/README.md`](audio/README.md) |

表中路径只描述实现组织，不声明所有平台均已验证。

## 证据边界

- module、demo、schema 或脚本存在，只证明对应制品存在。
- Host、QEMU、real board、build-only 与 run evidence 不能互相替代。
- UI、Player、具体 board 和 backend 是消费者或实现压力，不反向定义 Core。
- 完整依赖 DAG、跨平台兼容和产品成熟度不能从本地图推导。

专题入口继续从 [`docs/README.md`](README.md) 进入；依赖规则见
[`dependency_contract.md`](architecture/dependency_contract.md)。
