# Minimal-Kernel Runtime Ledger Facts v0

> `status`: `supporting`

`runtime_ledger.json` 由
[`export_minimal_kernel_runtime_session.py`](../../scripts/export_minimal_kernel_runtime_session.py) 从 runtime
evidence summary 派生。它按 exporter 消费顺序记录 session facts，不解析 Host/QEMU raw log，也不拥有
session verdict。

```text
runtime evidence summary
  -> session exporter
  -> runtime_ledger.json
  -> kernel_runtime_session.summary.json ledger reference
```

## 语义边界

- 根 identity、event shape、phase 顺序和 status 派生以 exporter 为事实源，本文不复制字段表。
- session summary 的 event count 必须与 ledger events 数量一致。
- `standing` 表示 exporter 接受对应输入 fact，`missing` 表示输入缺失或不足；单个 missing 不自行决定
  session 失败。
- `collapsed` 只描述 exporter 已判定失败的 session verdict，下游不得扩大其含义。
- `arch.ingress.seam` 是新 QEMU summary 的 preferred lower-half anchor；旧输入缺失时，下游不能自行推断
  为 standing。

## 消费约束

session、runtime evidence bundle、witness/inspect/compare 和 compiler lifecycle sidecar 可以把 ledger
作为 supporting artifact 或 explain context。消费者不得：

- 修改 phase/status 或从 ledger 发明 compare/collapse 规则；
- 绕过 session summary verdict；
- 把 events 解释为 scheduler trace、thread timeline、profiler 或 failure taxonomy；
- 从 ledger 路径或文件存在推断 runtime 已运行。

compiler lifecycle 的只读投影边界见
[`compiler_lifecycle_summary_sidecar_contract_v0.md`](../compiler/compiler_lifecycle_summary_sidecar_contract_v0.md)。

## 独立格式边界

当前 ledger 没有独立 schema、validator 或 CI gate；session schema 只验证其引用与事件数。若要把 ledger
提升为独立交换格式，必须先增加 schema、validator、兼容策略和失败语义，不能由本文冻结 exporter 的
当前 JSON 布局。
