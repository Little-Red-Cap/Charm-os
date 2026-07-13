# Charm Core 实现冲突审计

## 文档状态

- `status`: `supporting`
- `scope`: 当前源码命名、所有权和证据与 Core 裁决的冲突
- `authority`: [`CONSTITUTION.md`](../../CONSTITUTION.md) 与
  [`charm_core_contract.md`](charm_core_contract.md)

本文不复述首批裁决，不记录开发进度，也不为现有代码补发 Core 准入资格。运行状态必须查看当次
源码、CMake target 和测试输出。

## 审计规则

- 公开类型、module export 和真实 consumer 优先于 README 与目录名。
- Host fixture、metadata parser 和 build-only target 不能替代对应 execution environment 的运行证据。
- 同名对象只有在行为、失败和消费方一致时才能视为同一语义。
- 代码被广泛使用只说明迁移成本，不说明它属于 Core。
- 数量、日志 token 和某次 commit 状态不写入长期语义审计。

## 当前冲突

| 范围 | 源码事实 | 裁决影响 |
|---|---|---|
| Core relations | `Modules/` 没有共同的 `Requirement / Provision / ResolvedBinding / ResolutionFailure` 类型 | Constitution 允许这些关系，不代表稳定实现已经存在 |
| topology candidate | [`Backends/contract/capability_topology.hpp`](../../Backends/contract/capability_topology.hpp) 定义 `Requirement`、`Provided`、`ProfileBinding`、`RuntimeBinding` 和 `ContextView` | 它位于 Backend 并混合 compile-time metadata 与 runtime ref，仍是候选实现 |
| Capability contracts | TextSink、BlockDevice、Clock 在 Backends、Modules 和 fixtures 中存在不同形状 | 同名不能自动合并，也不能任选一份改名为 canonical |
| `charm.core` | [`charm.core.cppm`](../../Modules/core/charm.core.cppm) 聚合 util、`semantic.core`、init、trace、container 和 algorithm | 聚合便利不等于这些 module 都是 Core Primitive |
| semantic tooling | [`semantic.core`](../../Modules/core/semantic/semantic.core.cppm) 包含 Verdict、Witness、FailureDomain 和 reflection extraction | 当前有真实 consumer，但语义属于 evidence/tooling；目录位置不构成准入 |
| overloaded names | Binding、Profile、Graph、Runtime、Evidence 在 init、network、project、kernel 和 tools 中表达不同问题 | 必须保留限定名称，禁止构造统一宇宙类型 |
| project facts | BSP、board profile、linker、vendor SDK、ELF/ModuleX loader 都有真实实现 | 它们可重要且稳定，但仍是 Project Fact 或 Implementation/Tool |

## 具体边界

### Contract 与投影

C++ concept、header、module、C ABI 或 RPC table 只能投影 Capability Contract。若两个环境的方法名
相同但错误、单位、生命周期或缺失能力行为不同，它们尚未证明同一 Contract。

### Binding 与 Provider

Core `Binding` 只表示 Requirement 到 Provision 的选择关系。`init.binding`、UDP endpoint binding、
runtime object reference 和 CMake profile selection 必须使用各自限定语义。Provider 只是 Provision
中的角色，不据此批准公共 Provider base、manager 或 global registry。

### Evidence

Host/QEMU/real-board 是不同证据域。Host 程序读取 QEMU/board metadata 仍是 Host 证据；build-only
只证明构建。相同 App/Contract 是否跨域成立，必须由各域实际执行和相同失败类别证明。

### 所有权债务

`charm.core` 与 `semantic.core` 的当前 import 已被多个 module 消费，不能仅因命名不符合 Constitution
就直接移动或删除。修正所有权前必须先识别 consumer、提供替代 import，并保持行为回归。

## 审查输出

涉及新公共名词时，评审必须给出 Constitution 六问、真实 consumer、唯一裁决等级、失败语义和
至少一个反例。证据不足时保留在局部 implementation 或 exploration，不新增 Core API。

当前 exploration MVP 已取得 Host、真实 QEMU 与 H747 实板的同源运行证据；该事实只满足跨环境
举证前置条件，不代表现有 topology、Capability interface、`semantic.core`、RTE、init graph
或任一 Runtime 已获准成为 Charm Core。
