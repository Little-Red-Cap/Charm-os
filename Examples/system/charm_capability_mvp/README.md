# Charm Capability MVP

## 文档状态

- `status`: `exploration`
- `scope`: Charm MVP 的 Host、QEMU 与 H747 同源证据
- `authority`: [`CONSTITUTION.md`](../../../CONSTITUTION.md) 与
  [`charm_core_contract.md`](../../../docs/architecture/charm_core_contract.md)

本目录保存 portable application、contract projection、composition 与三域 verifier。它位于
`Examples/*`，不构成公共 API 或 Core 晋升。

## MVP 主张

[`mvp_app.hpp`](mvp_app.hpp) 只声明三项 requirement：

| Contract | Role |
|---|---|
| `TextSink` | `report` |
| `Clock` | `monotonic_time` |
| `BlockDevice` | `record_store` |

App 只接收 resolved context，不包含 platform macro、vendor header、HAL handle、target/profile name 或
provider identity。Requirement、Provision、Binding 和 pre-start failure 由
[`mvp_composition.hpp`](mvp_composition.hpp) 统一定义。

## 证据域

| Domain | 变化 | 必须证明 |
|---|---|---|
| Host | Host profile/providers | positive run、完整 resolution/app failure matrix、失败后不继续调用 |
| QEMU | Cortex-M7 firmware profile/providers | 同一源码、相同 semantic result、firmware 内 pre-start failure |
| H747 | board-local profile、UART/tick/RAM block providers | 真实板 positive run 与 missing binding 阻止 App start |

Host 与 QEMU 的 matrix 覆盖每个 requirement position、binding order 和 App failure stage。H747 使用
RAM-backed BlockDevice，避免把 QSPI/eMMC 可用性或写安全混入 MVP。三个 domain 必须给出可比较的
timestamp/checksum；任一域缺失都不能宣称跨环境证据闭合。

## 验证入口

| 范围 | 入口 |
|---|---|
| Host | [`run_host_ci.ps1`](run_host_ci.ps1) |
| QEMU | [`qemu/run_qemu_ci.ps1`](qemu/run_qemu_ci.ps1) |
| H747 build/capture | [`H747 Capability MVP`](../../project/h747-lab/apps/capability_mvp/README.md) |
| portable source boundary | [`verify_portable_source_boundary.ps1`](verify_portable_source_boundary.ps1) |
| Host/QEMU comparison | [`verify_host_qemu_evidence.ps1`](verify_host_qemu_evidence.ps1) |
| three-domain gate | [`verify_cross_environment_evidence.ps1`](verify_cross_environment_evidence.ps1) |

Three-domain verifier 要求真实 board log；缺失时以 `board_evidence_missing` 失败，不退化为 Host/QEMU
成功。编译器版本、token、case count、默认 build path 和当前通过状态由 runner/verifier 输出维护。

Source-boundary verifier 检查 shared headers 不含 target/OS/vendor vocabulary 或 conditional
compilation，并要求三个 harness 各自只包含 canonical `mvp_app.hpp` 一次。Host/QEMU partial gate 必须
显式限定 `-Domains host,qemu`，不能冒充三域证据。

## 边界

通过 MVP 只证明这组局部 contract projection 和 composition 在三个 execution environment 中保持同一
语义；它不自动批准 topology、Provider、Profile、Evidence、RTE 或任一 runtime 进入 Charm Core。
