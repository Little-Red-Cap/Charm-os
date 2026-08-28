# Charm Capability Relations v1 准入记录

## 文档状态

- `status`: `supporting`
- `decision`: `admitted`
- `scope`: `Requirement / Provision / Binding` 的首个公共 C++ 投影
- `authority`: [`CONSTITUTION.md`](../../CONSTITUTION.md) 与
  [`charm_core_contract.md`](charm_core_contract.md)

公共入口是 [`relations.hpp`](../../Modules/core/capability/relations.hpp)。该 header 不新增 Core 原语，
只投影 Constitution 已裁决的关系。

## 六问

1. **实现替换**：关系记录不包含 provider object、HAL、Backend 或 resolver；替换所有实现后仍成立。
2. **消费方必要性**：关系示例与 MVP 是两个独立 consumer；二者都必须区分需求、可用供给和本次选择，
   不能只依赖实现指针。
3. **独立可证明性**：关系示例与 MVP 各自消费公共关系投影，安装 smoke 只通过 `find_package` 消费导出的
   `Charm::core`。Clang/GCC 均编译公共投影；关系示例要求三种 key domain 混用在编译期被拒绝；MVP 在
   Host 与 QEMU 两个运行环境执行同一 app，并对 missing binding 保持相同的启动前失败分类。
4. **平台无关性**：公共 header 只依赖 scoped enum key 和值关系，不包含 OS、MCU、vendor 或产品名称。
5. **低例外预算**：没有 optional method、动态分配、字符串匹配、全局 registry 或平台分支。
6. **浅概念依赖**：只依赖 Capability Contract、Requirement、Provision 和 Binding；projection 不反向定义这些语义。

关系投影要求显式提供 key 与 contract/provision key；三个关系类型没有默认构造，因此项目 enum 的 `0`
值不会被误当成有效关系。

## 裁决

| public type | verdict | boundary |
|---|---|---|
| `Requirement` | `Core Primitive` projection | project-local requirement key 指向 contract key |
| `Provision` | `Core Primitive` projection | project-local provision key 指向 contract key |
| `Binding` | `Core Derived` projection | requirement key 指向 provision key |

`RequirementKey` 编码消费方作用域内的 role；不建立公共 `RoleKey`。`ProvisionKey` 标识 Provision，
不等于 Provider Instance。项目 key 的底层数字只在当前 project/profile/image 内有效。

## 排除项

- resolver、materializer、Profile 和 app context 不进入该 header；
- `ResolvedBinding` 当前与 `Binding` 字段完全相同，consumer 直接使用已验证 Binding 即可；在出现独立
  布局、生命周期或传输需求前，不提供公共类型；
- `ResolutionFailure` 的 duplicate、unknown、mismatch 和 endpoint validity 分类由具体 resolver 拥有；
  当前完整失败矩阵仍只有 Host 局部证据，不提供公共枚举；
- Provider base/manager/registry、Backend、Adapter、HAL 与 evidence collector 不获准；
- string label 只用于 evidence/explain，不参与 equality、resolution、持久化或跨进程协议；
- `TextSink`、`Clock`、`BlockDevice` 等具体 Contract 仍需各自准入；
- `.cppm` facade、稳定 wire ID、全局数字注册表和 generator 延期。

## 当前证据范围

OnlyCore 当前门禁覆盖 Host Clang/GCC 关系示例、MVP Host 失败矩阵、Clang sanitizer、安装后消费，
`arm-none-eabi-g++` freestanding build-only 消费，以及 QEMU `virt` 正例和 missing binding。QEMU 尚未覆盖
Host 的完整失败矩阵，build-only 与 QEMU 也不能替代真实板；因此结果物与失败分类仍不升级为公共投影。
