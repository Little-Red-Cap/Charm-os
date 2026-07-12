# Charm Core 全仓语义审计基线

## 文档状态

- `status`: `supporting`
- `baseline`: `dev@634269c9`
- `audited_at`: `2026-07-13`
- `authority`: [`CONSTITUTION.md`](../../CONSTITUTION.md) 与
  [`charm_core_contract.md`](charm_core_contract.md)

本文把 Constitution 的首批裁决映射到当前源码、CMake、示例和测试。它不新增 Core 术语，
也不通过解释现有代码为现有代码补发准入资格。

## 0. 基线后进展

[`Examples/system/charm_capability_mvp`](../../Examples/system/charm_capability_mvp/README.md)
现已使用同一 App、Contract 与 resolver 完成 Host 和真实 QEMU `mps2-an500` 两域。两域均输出
`timestamp=424242 checksum=0x49b880f0`，且 missing binding 都在 App 启动前得到
`start_count=0`。第 2 节对旧 Backends QEMU/board metadata smoke 的判断仍然成立；新增 QEMU
firmware 是独立的真实执行证据。共享源码边界现已由脚本检查平台词、条件编译和三个 harness
对唯一 `mvp_app.hpp` 的消费关系。H747 foundation target 已构建，但尚无真实 UART capture，
所以真实板域仍未完成。

## 1. 审计方法

证据按以下顺序判断：

1. 实际公开类型、module export 与 CMake target；
2. 可运行 smoke 及其真实执行环境；
3. supporting 契约和 README；
4. exploration、roadmap 和历史材料。

同名不等于同义。只有名字相同而行为、错误或消费方不同的载体，必须分别裁决。

本次只建立事实基线和迁移顺序，不移动目录、不改公共 API、不删除历史 smoke。

## 2. 总结论

### 2.1 Core 最小关系尚未形成稳定实现

`Modules/` 当前没有一套共同的 `Requirement / Provision / Binding / ResolutionFailure`
公共语义。最接近的实现位于：

- [`Backends/contract/capability_topology.hpp`](../../Backends/contract/capability_topology.hpp)；
- 14 个 `Examples/system/rte_*` 与 `charm_spine_*` 的局部 `main.cpp`。

RTE/Spine smokes 中 `Requirement`、`Provided`、`ComponentDesc` 等类型在 14 个文件中重复定义，
`ProfileBinding` 与 `ContextView` 在 13 个以上文件中重复定义。它们证明若干关系可以用普通 C++
表达，但没有形成稳定所有权、单一语义或可供三环境共用的实现。

### 2.2 Backends v1 是有效证据，但不是 MVP

`Backends/run-backends-v1-smoke.ps1` 的 10 项 host gate 在本基线全部通过。它证明：

- topology header 可做编译期关系检查；
- console 与 block contract candidate 可以单独编译；
- host provider 可以执行基本 console/block 行为；
- QEMU/board metadata 可以被 host 程序检查；
- system bridge/provider smokes 当前可运行。

它没有证明：

- QEMU reference smoke 在 QEMU 内执行；
- board reference smoke 在真实板执行；
- Host、QEMU、真实板运行同一应用源码；
- 三个环境消费同一个 `TextSink / Clock / BlockDevice` 定义；
- 缺少 required capability 时产生共同的启动前 resolution failure。

`Backends/host/reference_smoke/main.cpp`、`qemu/reference_smoke/main.cpp` 和
`board/reference_smoke/main.cpp` 仍分别定义自己的 `cap::TextSink`。Host reference 还定义了一个
比 `Backends/contract/block_storage.hpp` 更窄的 `cap::BlockDevice`。绿色 gate 不能覆盖这些差异。

### 2.3 当前 `core` 目录不等于宪法意义的 Core

[`Modules/core/charm.core.cppm`](../../Modules/core/charm.core.cppm) 当前公开聚合：

- `semantic.core`；
- `init.graph`；
- init recipe/plan/materialize/observe；
- 容器、服务、算法和 trace。

其中 [`Modules/core/semantic/semantic.core.cppm`](../../Modules/core/semantic/semantic.core.cppm)
包含 Artifact、Evidence、Projection、Witness、Explain、Handoff 与 reflection extraction 等
System Compiler 词汇；`init.graph` 是启动 DAG 的具体实现。根据 Constitution，它们分别属于
工具/派生表示或局部实现，不能因为被 `charm.core` re-export 就获得 Core Primitive 身份。

这是一项真实的所有权债务，但当前有多个 kernel module 直接依赖 `semantic.core`。在完成依赖
替代和回归证明前，不应粗暴删除或移动。

## 3. 首批概念映射

| 概念 | Constitution 裁决 | 当前真实载体 | 当前结论 | 后续处置 |
|---|---|---|---|---|
| Capability Contract | `Core Primitive` | Backends console/block headers；大量 smoke 局部 `cap::*` | 候选定义彼此不一致，且所有权落在 Backend 下 | MVP 先只保留一份中立定义，通过三环境证据后再申请公共提升 |
| Requirement | `Core Primitive` | Backends topology；14 组 RTE/Spine 局部类型 | 普通 C++ 表达已证明，稳定实现未成立 | 提取最小 `contract + role` 关系，不携带 provider identity |
| Provision | `Core Primitive` | 当前主要名为 `Provided` / `ProviderSet` | 名称把“关系”与“提供方实体”混在一起 | MVP 使用明确 Provision 关系；Provider descriptor 留在实现侧 |
| Binding | `Core Derived` | `ProfileBinding`、`RuntimeBinding`、`init.binding`、UDP `Binding` | 至少四种不同语义同名 | Core 只保留 Requirement 到 Provision 的选择关系；其它名称必须限定作用域 |
| ResolvedBinding / BindingSnapshot | `Stable Boundary` | Backends selected-binding evidence；RTE materialize 结果 | 没有共同结果类型或失败模型 | MVP 增加可审查结果和稳定 `ResolutionFailure` |
| Provider | `Core Derived` 角色 | `ProviderDesc/Meta/Ref`、具体 provider class、init graph 局部变量 | 角色与实体大量混用 | 不新增公共 Provider 基类/Manager；descriptor 与 handle 保持实现局部 |
| Component | `Stable Boundary` | 14 个 smoke 的 `ComponentDesc` | 只有实验性静态描述，没有共同消费方 | MVP 不依赖 Component 宇宙模型；确有装配需要时再举证 |
| Profile | `Stable Boundary` | RTE type、H747 `profile.cmake`、CMake target profile、领域参数 | 一个词覆盖组合选择、构建参数和调优参数 | 只把 Product/Profile Binding 选择视为该边界；其余名称保持限定 |
| Execution Environment | `Stable Boundary` | Backends host/qemu/board；真实 QEMU 与 H747 工程 | 三类入口存在，但尚未运行同一 MVP | 分别建立真实 Host、QEMU、board evidence，不用 host metadata 冒充执行证据 |
| Evidence | `Stable Boundary` | `semantic.core`、backend evidence、各 smoke frame、板级日志 | 多套 schema，无共同 MVP 语义 | 先固定 MVP 最小可比结果，不建立全仓 Evidence 宇宙模型 |
| Interface | `Implementation / Tool` | C++ concepts、headers、modules、C ABI table | 多种投影并存是正常事实 | 每个投影必须回指同一 Contract；不得用方法集合替代行为契约 |
| Backend | `Implementation / Tool` | `Backends/host|qemu|board` | 实现边界成立 | Capability Contract 不应由 Backend 身份拥有；后续拆开中立契约与实现 metadata |
| Driver | `Implementation / Tool` | `Modules/io/driver`、HAL 与 board adapter | 具体实现，不是应用语义 | 保持实现侧；只通过 Provision 参与组合 |
| Compiler | `Implementation / Tool` | scripts、System Compiler 文档、reflection extraction | 工具链很大，但不是 MVP 前置 | 停止扩面；只在直接验证或生成已获准语义时使用 |
| IR | `Implementation / Tool` | world/artifact JSON 与 compiler 工具内部模型 | 没有资格定义 Core | 保持工具内部或 exploration |
| Graph | `Implementation / Tool` | `init.graph`、materialized graph、runtime topology | 多种图表达不同问题 | 保持限定名称；禁止升级成统一 Core Graph |
| Loader | `Implementation / Tool` | Resident ELF/ModuleX loader | 已有独立证据链 | 继续作为 deployment projection，不进入 MVP 身份 |
| Runtime | `Rejected / Deferred` | kernel runtime、AppRuntime、POSIX runtime 等 | 名词过宽且行为不同 | 必须使用限定名称，不新增统一 `Runtime` Core 类型 |
| RTE | `Rejected / Deferred` | host-only duplicated smokes 与 exploration 文档 | 有语义证据，没有独立存在必要性 | 将可复用关系并回 MVP；其余 smoke 逐步退役，不提升 RTE API |
| BSP / BoardFacts / Product / Workspace | `Project Fact` | H747 profiles、targets、linker、vendor SDK | 明确属于项目组合输入 | 为外部 Project 消费契约保留，不迁入 Core |
| Resident ELF / ModuleX | `Implementation / Tool` | App ABI、loader、store、dev_loader | 已验证的部署机制 | 复用 MVP App，但不作为 MVP 成立条件 |
| Charm OS | `Project Fact` | 当前没有独立发行物 | 尚不存在 | 不为假想产品预建 Core 语义 |

## 4. MVP 三项 Capability 审计

| Contract | 当前状态 | 缺口 |
|---|---|---|
| `TextSink` | Backends 有 candidate header；仓库至少有 21 个局部 `struct TextSink` | 方法、返回类型和 flush 要求尚未统一；三环境未消费同一类型 |
| `Clock` | 至少 14 个 RTE/Spine smoke 有局部定义 | Backends contract 尚无 Clock；没有跨环境共同错误与单位语义 |
| `BlockDevice` | Backends candidate 与现有 `Modules/io/fs::BlockDevice` 并存 | 一个面向 capability，一个面向 FS 结构；Host reference 又使用更窄副本 |

下一步不能简单挑一个现有类型改名为 canonical。必须先固定应用真正依赖的最小行为、错误和单位，
再让 Host、QEMU、board 三个实现共同满足。

## 5. 同名冲突的处置规则

- `init::Graph` 继续表示 init DAG，不得简称为 Charm Graph。
- `init.binding` 当前是 init node/capability-name helper，不是 Core Binding。
- `net::udp::Binding` 是 endpoint 路由记录，不是 Capability Binding。
- `charm_apply_target_profile()` 只表达 toolchain/build flags。
- `h747_lab_add_profile()` 只表达 H747 Project 的 firmware composition manifest。
- `PlayerProfile` 等领域参数必须保留领域前缀。
- `semantic.core` 当前名称和目录位置不构成宪法准入，后续应从 `charm.core` 身份中解耦。

## 6. 执行顺序

### A. 立即执行

1. 用治理脚本锁住 canonical 定位、裁决表、旧路线状态和本地链接。
2. 保持 Backends/RTE/System Compiler 为证据来源，不继续扩张其公共词汇。
3. 为 MVP 建立一份非公共、可审查的共同 Contract 与 App source，先证明语义再申请提升。

### B. MVP 实现切片

1. 固定 `TextSink / Clock / BlockDevice` 的行为、错误和单位。
2. 固定 `Requirement / Provision / Binding / ResolutionFailure` 的最小普通 C++ 表达。
3. 同一 App source 只通过 Requirement 获取能力。
4. 先完成 Host 正例和缺失 capability 负例。
5. 再让真实 QEMU firmware 与 H747 profile 复用同一 App 和 Contract。
6. 最后比较三环境 Evidence，确认不是三套相似 demo。

### C. 证据成立后再做

1. 决定最小语义的正式模块与目录所有权。
2. 从 `charm.core` 移除 System Compiler/Graph 身份暗示，同时保持局部 import 可迁移。
3. 合并或退役重复 RTE/Spine smokes。
4. 定义外部 Project package/export 消费契约。

## 7. 本基线明确不宣称

- 不宣称 Backends v1 已经完成跨环境 MVP。
- 不宣称 `semantic.core`、RTE 或 init.graph 已获准成为 Core。
- 不宣称已有 TextSink、Clock、BlockDevice 中任一版本已是最终契约。
- 不宣称 host 上检查 QEMU/board metadata 等于 QEMU/board 执行。
- 不以文档数量、测试数量或目录名称替代同源三环境证据。
