# Kernel Runtime Session Witness v0

## 文档状态

- `status`: `supporting`
- `scope`: minimal-kernel 一次运行会话的汇总边界与消费约束
- `authority`: 受 [`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md) 约束

本文件保留既有路径，因为 workflow、canonical world、schema sample 和 front-page fixture 会引用它。
它不是 Charm Core 契约，也不以文档、schema 或 report 的存在证明 runtime 已运行。

## 对象边界

`kernel_runtime_session` 是 host 语义证据与 ARMv7-A QEMU 机器证据的一次汇总对象：

```text
host semantic summary + QEMU machine summary
  -> session exporter
  -> kernel_runtime_session.summary.json
  -> witness / compare consumer
```

它允许上层消费稳定的 session facts，而不重新解析 host 日志、QEMU 串口日志或各 smoke 的私有输出。
它不替代 runtime evidence bundle、witness bundle 或 world compare，也不定义 scheduler trace、进程模型、
用户态 ABI 或真实板行为。

## 权威实现

- schema：[`minimal_kernel.kernel_runtime_session.v0.schema.json`](../../schemas/minimal_kernel.kernel_runtime_session.v0.schema.json)
- exporter：[`export_minimal_kernel_runtime_session.py`](../../scripts/export_minimal_kernel_runtime_session.py)
- sample：[`minimal_kernel.kernel_runtime_session.v0.sample.json`](../../schemas/examples/minimal_kernel.kernel_runtime_session.v0.sample.json)
- ledger 语义：[`minimal_kernel_runtime_ledger_fact_contract_v0.md`](minimal_kernel_runtime_ledger_fact_contract_v0.md)

字段、枚举、failure code 和派生规则以 schema 与 exporter 为准。本文件不复制这些机器契约，避免实现变化后
出现第二份字段真相。

## 证据分层

### Semantic Witness

`semantic_witness` 投影 host cold/warm summary，回答 runtime glue、trap/syscall、task message 与 session API
的 host 语义是否仍成立。它不证明真实 ARM 异常入口、timer IRQ、trap frame capture 或寄存器 writeback。

### Machine Witness

`machine_witness` 投影 ARMv7-A QEMU lower-half summary，回答 exception、interrupt、timer、trap、context
与 runtime loop 的机器入口是否被观测。`arch_ingress_seam` 是 preferred ingress anchor；旧 summary 的
兼容推断只由 exporter 持有。

它不证明真机内存、时钟、外设、BootROM 或板级启动约束。

### Runtime Facts

v0 汇总 `tick`、`trap`、`thread`、`task_syscall` 与 `handoff_continuity`。这些布尔值是 exporter
对输入 summary 的投影，不是仅凭 schema presence 成立的声明。

### Ledger 与 Verdict

`runtime_ledger.json` 记录 exporter 已消费事实的顺序；session summary 的 `ledger` 字段只定位该侧车
并记录 event count。最终 standing、drifted 或 collapsed 结论由 session `verdict` 表达，ledger 不重判 verdict。

## 失败与产物

失败项固定携带 code、domain、layer、focus、required、phase 和 message。合法枚举及 session status
派生规则由 schema/exporter 定义；consumer 不得从 message 文本或 raw log 发明新的判决。

session exporter 的直接产物是：

```text
session/kernel_runtime_session.summary.json
session/runtime_ledger.json
session/report.md
session/check.txt
```

`report.md` 与 `check.txt` 是同一 summary 的人读投影，不是独立运行证据。上层应先消费 summary/verdict，
需要事实顺序时再读取 ledger。

## 验证入口

从仓库根目录运行最小聚合 smoke：

```powershell
./scripts/minimal_kernel_runtime_session_witness_smoke.ps1
```

只读检查已有聚合 summary：

```powershell
./scripts/inspect_minimal_kernel_runtime_session_witness_smoke.ps1 `
  -Summary out/minimal-kernel-runtime-session-witness-smoke/summary.json
```

CI、compare consumer 和其它 runner 的参数由各自脚本及 workflow 维护，本文件不复制完整命令矩阵。
检查通过只证明该次输入与当前契约一致；没有当次 artifact 时，不能从本文推断 Host、QEMU 或真实板已通过。

## 历史材料

早期完整设计、阶段叙事、world compare 展开和 runner 清单归档于
[`../archive/minimal-kernel-runtime-v0/kernel_runtime_session_witness_v0.md`](../archive/minimal-kernel-runtime-v0/kernel_runtime_session_witness_v0.md)。
