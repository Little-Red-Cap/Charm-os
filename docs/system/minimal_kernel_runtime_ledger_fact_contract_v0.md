# Minimal-Kernel Runtime Ledger Facts v0

> status: supporting
>
> `runtime_ledger.json` 由
> [`../../scripts/export_minimal_kernel_runtime_session.py`](../../scripts/export_minimal_kernel_runtime_session.py)
> 从 runtime evidence summary 派生。本文描述当前 exporter 输出，不声明独立 ledger
> schema 或 verdict owner。

## 角色

```text
runtime evidence summary
-> session exporter
-> runtime_ledger.json
-> kernel_runtime_session.summary.json ledger reference
```

Session 是否成立由 `kernel_runtime_session.summary.json.verdict` 决定。Ledger 只按顺序
记录 exporter 已消费的 summary facts；它不解析 raw Host/QEMU log，也不补充 verdict。

## 根对象

| 字段 | 语义 |
|---|---|
| `schema` | 固定为 `minimal_kernel.kernel_runtime_session.runtime_ledger/v0` |
| `generated_at` | exporter 生成时的 UTC 时间 |
| `session_id` | 与 session summary 相同 |
| `source_summary` | 输入 runtime evidence summary 路径 |
| `events` | 按 exporter 固定顺序生成的事件数组 |

Ledger 根对象没有 `event_count`。Session summary 的
`ledger.event_count` 必须等于 `runtime_ledger.events.length`。

## Event

每个 event 包含：

| 字段 | 约束 |
|---|---|
| `index` | 从 0 开始，等于数组位置 |
| `phase` | 下表中的固定 phase |
| `domain` | `semantic / machine / runtime / bundle` |
| `status` | `standing / missing / collapsed` |
| `source` | exporter 已消费的 summary 路径或 `null`，不能指向 raw log |
| `focus` | 受影响的最小标签集合 |

固定顺序：

| Index | Phase | Domain |
|---|---|---|
| 0 | `semantic.host.cold` | `semantic` |
| 1 | `semantic.host.warm` | `semantic` |
| 2 | `machine.qemu.lower_half` | `machine` |
| 3 | `arch.ingress.seam` | `machine` |
| 4 | `runtime.tick` | `runtime` |
| 5 | `runtime.trap` | `runtime` |
| 6 | `runtime.thread` | `runtime` |
| 7 | `runtime.task_syscall` | `runtime` |
| 8 | `runtime.handoff_continuity` | `runtime` |
| 9 | `session.verdict` | `bundle` |

`standing` 表示对应 summary fact 被 exporter 接受；`missing` 表示缺失或不足；
`collapsed` 当前只用于失败的 `session.verdict`。单个 phase 为 `missing` 不自动使 session
失败，兼容和 collapse 判断仍由 exporter 持有。

`arch.ingress.seam` 是新 QEMU summary 的首选 lower-half anchor。旧 summary 缺失时可
记录 `missing`，但不能由下游自行推断为 `standing`。

## 消费规则

允许的消费方式：

- session summary 通过 `ledger.runtime_ledger` 和 `ledger.event_count` 引用；
- runtime evidence bundle 将它作为 session sidecar 出口；
- witness/inspect/compare 工具把它作为 supporting artifact 或 explain context；
- compiler lifecycle sidecar 可只读投影为 `observed`，边界见
  [`../compiler/compiler_lifecycle_summary_sidecar_contract_v0.md`](../compiler/compiler_lifecycle_summary_sidecar_contract_v0.md)。

消费者不得重新解析 raw log、修改 phase/status、从 ledger 产生新 compare/collapse rule，
或把 events 当作 scheduler trace、thread timeline、profiler 与 failure taxonomy。

## 验证边界

当前仓库没有独立 `runtime_ledger` schema、validator 或 CI gate。Session exporter 写出
ledger，并在 session summary 中投影路径与事件数；session summary 再由
`minimal_kernel.kernel_runtime_session.v0.schema.json` 验证。

因此 ledger 字段和顺序的事实源是 exporter。若要让 ledger 成为独立交换格式，必须先
增加 schema、validator、兼容策略和失败语义，不能仅靠本文冻结。
