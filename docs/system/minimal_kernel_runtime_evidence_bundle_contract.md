# Minimal Kernel Runtime Evidence Bundle Contract

## 文档状态

- `status`: `supporting`
- `scope`: minimal-kernel Host 与 ARMv7-A QEMU 运行证据的聚合边界
- `authority`: 受 [`minimal_kernel_runtime_bridge_contract.md`](minimal_kernel_runtime_bridge_contract.md) 约束

本文件是 minimal-kernel runtime 证据的默认入口。它规定不同证据域如何聚合，不定义 Charm Core，
也不以 bundle、schema、report 或 CI workflow 的存在证明 runtime 已执行。

## 聚合主链

默认入口为 [`minimal_kernel_runtime_evidence_bundle.ps1`](../../scripts/minimal_kernel_runtime_evidence_bundle.ps1)：

```text
host dual bundle       -> semantic evidence
ARMv7-A QEMU bundle    -> machine evidence
session exporter       -> kernel_runtime_session + runtime ledger
witness bundle         -> optional evidence projection
                       -> root summary.json / report.md / check.txt
```

脚本允许显式跳过 session 或 witness projection；Host 与 QEMU 仍是根 summary 的基础输入。
上层自动化应优先读取根 `summary.json`，不要从日志文本或 Markdown report 反推 verdict。

## 证据边界

### Host Semantic

Host dual bundle 覆盖 cold configure/build/run 与 warm reuse，证明 runtime host verifier 的语义和复用路径。
它不证明 ARM 异常入口、trap frame、IRQ、timer 或真实上下文切换。

Host 子契约见 [`minimal_kernel_host_smoke_bundle_contract.md`](minimal_kernel_host_smoke_bundle_contract.md)。

### ARMv7-A QEMU Machine

QEMU bundle 聚合当前 lower-half cases，并以其 summary 中的 case count、status 和 artifact refs 为事实来源。
它证明对应 QEMU 机器入口在该次运行中被观测，不证明 Host stub 完整性，也不证明真实板内存、时钟或外设。

### Kernel Runtime Session

session exporter 将 Host semantic witness、QEMU machine witness、runtime facts、ledger 引用和 verdict
投影到 `kernel_runtime_session.summary.json`。`arch_ingress_seam` 是当前 preferred lower-half ingress anchor；
旧 summary 的兼容推断只由 exporter 持有。

Session 与 ledger 的局部边界见
[`kernel_runtime_session_witness_v0.md`](kernel_runtime_session_witness_v0.md)。

### Witness Projection

witness bundle 可以消费根 summary 与 session，将其投影为已有 witness entry。它不增加 runtime facts，
也不能把缺失的 Host/QEMU 证据补成 standing。

## 根对象与产物

根机器对象遵守
[`minimal_kernel.runtime_evidence_bundle.summary.v1.schema.json`](../../schemas/minimal_kernel.runtime_evidence_bundle.summary.v1.schema.json)。
其稳定职责是：

- 记录 Host 与 QEMU 子 bundle 的状态、计数和 artifact refs；
- 可选引用 session 与 witness bundle；
- 汇总 `result` 与 `violations`；
- 定位人读 report/check 与子证据目录。

默认输出根包含 Host、QEMU、session、witness 子目录，以及根 `summary.json`、`report.md`、`check.txt`
和阶段日志。具体文件名与可选 sidecar 由脚本/schema 维护，本文件不复制完整目录树。

`report.md` 与 `check.txt` 是根 summary 的人读投影；case log 是下钻证据。它们都不能绕过 summary
和对应子对象的 verdict。

## 失败语义

聚合脚本允许子阶段失败后继续生成可诊断工件。以下情况会写入 `violations`：

- Host、QEMU、session、witness 或可选 sidecar 返回非零；
- 预期 summary 缺失或不可读取；
- 子 summary 状态、case 完整性或引用工件不满足当前检查；
- session verdict 不是 standing。

存在 violation 时，根 `result` 必须为 `fail`；report/check 写出后，脚本仍以失败结束。保留工件
是为了诊断，不表示失败被接受。

## 验证入口

从仓库根目录运行总入口，并把全部产物放在一个明确的输出根：

```powershell
./scripts/minimal_kernel_runtime_evidence_bundle.ps1 `
  -OutputRoot out/minimal-kernel-runtime-evidence
```

对已有 bundle 做结构与引用检查：

```powershell
python ./scripts/validate_minimal_kernel_runtime_evidence.py `
  --bundle-root out/minimal-kernel-runtime-evidence
```

CI 入口为 [`.github/workflows/minimal-kernel-runtime-evidence.yml`](../../.github/workflows/minimal-kernel-runtime-evidence.yml)。
runner 参数、工具安装、timeout 与 artifact 发布由 workflow 和脚本维护，不在本契约重复。

通过一次 bundle 只证明该次 Host/QEMU 输入满足当前证据契约；它不等于真实板验证、完整 OS、
用户态隔离、进程模型或产品级运行时已经成立。
