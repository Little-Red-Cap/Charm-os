# H747 Capability MVP

## 文档状态

- `status`: `exploration`
- `scope`: Charm cross-environment MVP 的 real-board slice
- `public API`: none

本 app 只提供 H747 profile 和 provider，portable App、contract、resolver 与 evidence format 均来自
[`Examples/system/charm_capability_mvp`](../../../../system/charm_capability_mvp/README.md)。

## Provider

| Contract | H747 implementation |
|---|---|
| `TextSink` | blocking UART1 console |
| `Clock` | H747 tick，首次读取锚定 deterministic evidence epoch |
| `BlockDevice` | 4 个 volatile 512-byte RAM blocks |

RAM BlockDevice 用于隔离 capability resolution/App semantics 与 QSPI/eMMC bring-up。外围 `init.graph`
只负责 console/app 初始化顺序，不是 Capability Binding model；Requirement、Provision、Binding、
ProfileView 和 pre-start failure 仍由 shared composition 定义。

## 验证

从 `Examples/project/h747-lab` 复用 preset `h747-lab-capability-mvp-debug` 与
`build-h747-lab-capability-mvp-debug`，再运行
[`capture-capability-mvp-board-smoke.ps1`](../../tools/capture-capability-mvp-board-smoke.ps1)。

Board capture 必须同时证明 positive run 和 missing required binding 时 `start_count=0`。随后由
[`verify_cross_environment_evidence.ps1`](../../../../system/charm_capability_mvp/verify_cross_environment_evidence.ps1)
比较 Host、QEMU 与 H747 的 timestamp/checksum。Build-only、缺失 board log 或单独 Host/QEMU green
均不满足三域证据。

当前通过状态、UART token、工具链版本和 log path 以当次 capture/verifier 输出为准，不写入本文。
