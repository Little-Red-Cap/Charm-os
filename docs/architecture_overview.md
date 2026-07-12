# Charm 实现地图

## 文档角色

本文是 `supporting` 实现地图，只回答当前代码放在哪里、从哪些入口进入、主要运行路径怎样连接。它不定义 Charm Core，也不维护逐模块功能清单或“当前全部测试已通过”之类易失效状态。

权威顺序：

1. [`../CONSTITUTION.md`](../CONSTITUTION.md)
2. [`architecture/charm_core_contract.md`](architecture/charm_core_contract.md)
3. [`architecture/README.md`](architecture/README.md)
4. 本页及专题契约

旧版详细盘点中的分层、迁移和阶段状态见 [`archive/architecture-inventory-v0/README.md`](archive/architecture-inventory-v0/README.md)。

## 仓库结构

| 路径 | 当前用途 |
|---|---|
| `Modules/core` | util、semantic、init、trace、固定容量 service、通用算法 |
| `Modules/system` | kernel、boot、bring-up、device discovery、ModuleX、power、RTOS 适配 |
| `Modules/io` | channel、registry、reactor、HAL、block、FS、USB、network、POSIX、shell、out |
| `Modules/platform` | board/platform 描述与 host 平台实现 |
| `Modules/media` | audio 与 media stream |
| `Modules/gfx` | framebuffer、canvas、font 等图形基础件 |
| `Modules/ui` | Ink、Vivid 及共享 UI 代码 |
| `Modules/control` | 控制领域代码；不属于 Core |
| `Modules/thirdparty` | vendored 第三方源码 |
| `Examples` | 可运行样本、板级工程和语义验证 |
| `Backends` | capability/backend 实验与契约；不自动进入 Core |
| `Draft` | 未冻结探索 |
| `targets` | 架构/目标相关 lower-half 与构建材料 |

目录名只表示代码所有权和当前组织，不自动授予 Core 身份。

## 导入入口

当前稳定聚合入口：

- `charm.core`
- `charm.system`
- `charm.io`
- `charm.net`
- `charm.media`
- `charm.ui.ink`
- `charm.ui.vivid`

聚合入口较宽；只需要一个能力时应直接 import 叶子模块。入口的完整状态由 [`architecture/entry_surface_contract.md`](architecture/entry_surface_contract.md) 约束：

- `charm.foundation` 是兼容 facade；
- `charm.runtime` 是无 re-export 的退役 tombstone；
- `charm.domain` 是历史名称，不是当前模块入口。

不要从“某个模块能被 import”推导其已成为稳定公共契约。

## 装配与依赖

### 静态系统

板级已知能力走：

```text
BoardCaps -> init.graph nodes -> registry/service -> App
```

`init.graph` 当前硬约束包括固定容量、provider 唯一、依赖解析、phase 顺序和非阻塞 init。详见 [`system/init_graph_contract.md`](system/init_graph_contract.md)。

### 动态设备

运行期发现设备走：

```text
Bus -> DeviceDesc -> Registry/Driver -> stable slot or manager -> consumer
```

它不能替代静态 bring-up。详见 [`architecture/driver_model.md`](architecture/driver_model.md)。

### 依赖事实

- Core 身份由 Constitution 裁决，不由目录层次反推。
- `Modules/core` 应保持平台无关，不依赖 system/io/media/ui 的具体实现。
- UI、media、project 和 backend 不应反向定义 Core 词汇。
- `CHARM_ENABLE_DEPENDENCY_WHITELIST` 当前主要封锁历史 facade import，不是完整模块 DAG 证明器。
- 实际依赖以 module import、CMake source collection 和可运行消费者为准。

旧 `Foundation -> Runtime -> Domains` 可作实现分区记忆，但不是 Charm Core 模型，也没有被单一自动化 gate 完整证明。

## 主要运行路径

| 目标 | 当前路径 | 首选入口 |
|---|---|---|
| 静态 bring-up | board caps -> init graph -> service/registry | [`system/README.md`](system/README.md) |
| 字节 IO | HAL/backend -> channel -> registry/reactor -> protocol | [`io/README.md`](io/README.md) |
| block 与文件系统 | block device -> block registry -> VFS/mount | [`storage/README.md`](storage/README.md) |
| resident App | received/store image -> loader -> AppRuntime -> capability table | [`architecture/resident_image_platform_v1_contract.md`](architecture/resident_image_platform_v1_contract.md) |
| POSIX 兼容 | image/process facade -> same-address-space runtime | [`system/posix_support_overview.md`](system/posix_support_overview.md) |
| minimal kernel | host semantic evidence + ARMv7-A/QEMU machine evidence | [`system/minimal_kernel_runtime_evidence_bundle_contract.md`](system/minimal_kernel_runtime_evidence_bundle_contract.md) |
| UI | UI runtime -> render/input backend capability | [`ui/README.md`](ui/README.md) |
| Audio | source/decoder/stream/sink backend | [`audio/README.md`](audio/README.md) |

这些是实现路径，不是所有平台都已通过的兼容性声明。

## 专题入口

- Core 治理与语义审计：[`architecture/README.md`](architecture/README.md)
- Capability 地图：[`capability_map.md`](capability_map.md)
- 入口与依赖：[`architecture/entry_surface_contract.md`](architecture/entry_surface_contract.md)、[`architecture/dependency_contract.md`](architecture/dependency_contract.md)
- IO：[`io/README.md`](io/README.md)
- System、kernel、QEMU、POSIX：[`system/README.md`](system/README.md)
- Storage：[`storage/README.md`](storage/README.md)
- Input：[`input/README.md`](input/README.md)
- USB：[`usb/README.md`](usb/README.md)
- UI：[`ui/README.md`](ui/README.md)
- Audio：[`audio/README.md`](audio/README.md)
- Project/build：[`project/README.md`](project/README.md)

逐模块细节应放在对应目录 README 或专题契约，不回填到本页。

## 证据规则

“模块存在”“有 demo”“有脚本”分别只证明代码、样本或入口存在。声称某条路径可用时，至少给出：

- 对应源码或 schema；
- 实际 build/run 命令；
- 最近一次结果与适用平台；
- 尚未覆盖的失败路径。

易变的 build 状态、性能数字、当前 UI 待办和单板 bring-up 结果应放进 evidence log、专题 README 或 tracking 文档，不放在全局实现地图。

## 当前边界

- 尚无完整依赖 DAG gate 可以证明所有跨目录 import 合法。
- `charm.*` 聚合入口仍较宽，不代表其中每个导出都是 Core。
- POSIX、minimal-kernel、resident runtime、ModuleX 与 boot 都处于不同成熟度，不能合并成“Charm OS 已完成”。
- Host、QEMU 与 real board 提供不同等级证据，不能互相替代。
- Player、具体板级工程和 backend 是消费者/实现压力，不反向定义平台核心。
