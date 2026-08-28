# Charm Capability MVP

## 文档状态

- `status`: `exploration`
- `scope`: Charm MVP 的 Host 与 QEMU 当前证据
- `authority`: [`CONSTITUTION.md`](../../../CONSTITUTION.md) 与
  [`charm_core_contract.md`](../../../docs/architecture/charm_core_contract.md)

本目录保存 portable application、contract projection、project-local resolver 与证据 verifier。
关系记录来自公共 [`relations.hpp`](../../../Modules/core/capability/relations.hpp)；具体 Contract、
resolver、Profile 和 app context 仍是本示例局部实现。

## MVP 主张

[`mvp_app.hpp`](mvp_app.hpp) 只声明三项 requirement：

| Contract | Requirement Key |
|---|---|
| `TextSink` | `report` |
| `Clock` | `monotonic_time` |
| `BlockDevice` | `record_store` |

App 只接收 resolved context，不包含 platform macro、vendor header、HAL handle、target/profile name 或
provider identity。Requirement、Provision、Binding 和 pre-start failure 由
[`mvp_composition.hpp`](mvp_composition.hpp) 统一定义。

Contract、Requirement 和 Provision 使用三个不同的 project-local `enum class`。Requirement key
作用域化原 role；Binding 指向 Provision key，不指向 provider identity 或数组下标。

## 证据域

| Domain | 变化 | 必须证明 |
|---|---|---|
| Host | Host profile/providers | positive run、完整 resolution/app failure matrix、失败后不继续调用 |
| QEMU | `virt` 固件与局部 providers | 同一 app positive run、可比较 evidence、missing binding 在启动前失败 |

Host matrix 覆盖每个 requirement position、binding order 和 App failure stage。QEMU 当前只覆盖正例和
missing binding，不等同于 Host 完整失败矩阵。真实板和旧 board log 不属于当前 OnlyCore 证据范围。

## 验证入口

| 范围 | 入口 |
|---|---|
| Host | [`run_host_ci.ps1`](run_host_ci.ps1) |
| QEMU | [`qemu/run_qemu_ci.ps1`](qemu/run_qemu_ci.ps1) |
| portable source boundary | [`verify_portable_source_boundary.ps1`](verify_portable_source_boundary.ps1) |

编译器版本、case count、默认 build path 和当前通过状态由 Host runner 输出维护。

Source-boundary verifier 检查 shared headers 和 Host consumer 不含 target/OS/vendor vocabulary 或
conditional compilation。

## 边界

本地 runner 已证明同一 portable app 在 Host 和 QEMU 正例中运行，并在两域对 missing binding 作相同启动前
分类；workflow 已定义对应远端 jobs，但首次 Actions 结果须以远端 run 为准。该证据不批准 Provider、Profile、
Evidence 或任一 runtime 进入 Charm Core，也不声明真实板兼容。
