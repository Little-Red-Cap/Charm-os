# Charm Capability Relations v1 准入记录

## 文档状态

- `status`: `supporting`
- `decision`: `admitted`
- `scope`: `Requirement / Provision / Binding / ResolvedBinding / ResolutionFailure` 的首个公共 C++ 投影
- `authority`: [`CONSTITUTION.md`](../../CONSTITUTION.md) 与
  [`charm_core_contract.md`](charm_core_contract.md)

公共入口是 [`relations.hpp`](../../Modules/core/capability/relations.hpp)。该 header 不新增 Core 原语，
只投影 Constitution 已裁决的关系。

## 六问

1. **实现替换**：关系记录不包含 provider object、HAL、Backend 或 resolver；替换所有实现后仍成立。
2. **消费方必要性**：同一应用在 Host/QEMU 中必须区分需求、可用供给和本次选择，不能只依赖实现指针。
3. **独立可证明性**：Capability MVP 的运行时 resolver 与 Backend/reference 的 constexpr 关系检查是两套实现；集中矩阵覆盖正反例。
4. **平台无关性**：公共 header 只依赖 scoped enum key 和值关系，不包含 OS、MCU、vendor 或产品名称。
5. **低例外预算**：没有 optional method、动态分配、字符串匹配、全局 registry 或平台分支。
6. **浅概念依赖**：只依赖 Capability Contract、Requirement、Provision 和 Binding；projection 不反向定义这些语义。

## 裁决

| public type | verdict | boundary |
|---|---|---|
| `Requirement` | `Core Primitive` projection | project-local requirement key 指向 contract key |
| `Provision` | `Core Primitive` projection | project-local provision key 指向 contract key |
| `Binding` | `Core Derived` projection | requirement key 指向 provision key |
| `ResolvedBinding` | `Stable Boundary` | 已验证的 Binding 结果物 |
| `ResolutionFailure` | `Stable Boundary` | 启动前稳定失败分类 |

`RequirementKey` 编码消费方作用域内的 role；不建立公共 `RoleKey`。`ProvisionKey` 标识 Provision，
不等于 Provider Instance。项目 key 的底层数字只在当前 project/profile/image 内有效。

## 排除项

- resolver、materializer、Profile 和 app context 不进入该 header；
- Provider base/manager/registry、Backend、Adapter、HAL 与 evidence collector 不获准；
- string label 只用于 evidence/explain，不参与 equality、resolution、持久化或跨进程协议；
- `TextSink`、`Clock`、`BlockDevice` 等具体 Contract 仍需各自准入；
- `.cppm` facade、稳定 wire ID、全局数字注册表和 generator 延期。

## 当前证据范围

Host Clang/GCC、Host sanitizer、真实 QEMU Cortex-M7 和 Backend reference smokes 是本轮门禁。H747
工程当前损坏，未修改、未构建、未烧录；旧 board evidence 仅作历史材料，不证明当前源码兼容性。
