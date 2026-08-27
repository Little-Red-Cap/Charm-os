# Charm Capability MVP

## 文档状态

- `status`: `exploration`
- `scope`: Charm MVP 的 Host/QEMU 当前证据与 H747 历史证据
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
| QEMU | Cortex-M7 firmware profile/providers | 同一源码、相同 semantic result、firmware 内 pre-start failure |
| H747 | 历史 board-local profile | 本轮不修改、不构建、不复验 |

Host 与 QEMU 的 matrix 覆盖每个 requirement position、binding order 和 App failure stage。H747 工程
当前损坏，旧 board log 只保留为历史材料，不能证明当前 relation v1 源码兼容或三域闭环。

## 验证入口

| 范围 | 入口 |
|---|---|
| Host | [`run_host_ci.ps1`](run_host_ci.ps1) |
| QEMU | [`qemu/run_qemu_ci.ps1`](qemu/run_qemu_ci.ps1) |
| portable source boundary | [`verify_portable_source_boundary.ps1`](verify_portable_source_boundary.ps1) |
| Host/QEMU comparison | [`verify_host_qemu_evidence.ps1`](verify_host_qemu_evidence.ps1) |
| three-domain gate | [`verify_cross_environment_evidence.ps1`](verify_cross_environment_evidence.ps1) |

Three-domain verifier 保留为未来 H747 修复后的重新认证入口，不属于当前 gate。编译器版本、token、
case count、默认 build path 和当前通过状态由 Host/QEMU runner/verifier 输出维护。

Source-boundary verifier 检查 shared headers 不含 target/OS/vendor vocabulary 或 conditional
compilation。当前默认检查 Host/QEMU；未来三域复验必须显式传
`-Domains host,qemu,h747` 并同时运行真实 board verifier。

## 边界

当前 gate 证明局部 Contract projection 和 relation v1 在 Host/QEMU 保持同一语义；它不批准
Provider、Profile、Evidence 或任一 runtime 进入 Charm Core，也不声明 H747 当前兼容。
