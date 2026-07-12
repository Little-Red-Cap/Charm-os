# Minimal Kernel Runtime Ledger Fact Contract v0

## 定位

`minimal_kernel.kernel_runtime_session.runtime_ledger/v0` 是 `kernel_runtime_session` 的 session fact ledger。

它记录 session exporter 已经消费并归纳过的 summary facts，用来说明一次最小内核运行会话的事实发生顺序。它不是新的 schema 对象，不是独立 validator 目标，也不是 QEMU 串口日志 parser。

v0 的核心边界是：

```text
runtime evidence summary facts
  -> session exporter
  -> runtime_ledger.json
  -> kernel_runtime_session.summary.json ledger projection
```

上层可以消费 `runtime_ledger.json` 中已经出口的 ledger facts，也可以通过 `kernel_runtime_session.summary.json` 的 `ledger` 字段定位这份 ledger；上层不得为了补判决而回读 raw host log、raw QEMU log、raw session smoke log 或 raw runtime/session/world-compare 原始证据。
可选 compiler lifecycle sidecar 会把 `runtime_ledger.json` 只读投影为 `observed`，见
[`compiler_lifecycle_summary_sidecar_contract_v0.md`](../compiler/compiler_lifecycle_summary_sidecar_contract_v0.md)；
该投影不改变本 ledger contract 的字段、事件顺序或 session verdict。

## 根对象

`runtime_ledger.json` v0 根对象字段固定为：

```text
schema / generated_at / session_id / source_summary / events
```

```json
{
  "schema": "minimal_kernel.kernel_runtime_session.runtime_ledger/v0",
  "generated_at": "2026-05-11T00:00:00Z",
  "session_id": "minimal_kernel_runtime.armv7a_qemu.debug",
  "source_summary": "out/minimal-kernel-runtime-evidence/summary.json",
  "events": []
}
```

字段语义：

- `schema`：ledger fact contract 的版本标识。
- `generated_at`：session exporter 生成 ledger 的 UTC 时间。
- `session_id`：与 `kernel_runtime_session.summary.json.session_id` 相同的会话标识。
- `source_summary`：session exporter 消费的 runtime evidence summary。
- `events`：按发生顺序排列的 fact events。

`runtime_ledger.json` 根对象 v0 不声明 `event_count` 字段。`kernel_runtime_session.summary.json.ledger.event_count` 是 session summary 对引用 ledger 的投影计数，必须满足：

```text
kernel_runtime_session.summary.json.ledger.event_count
  == runtime_ledger.json.events.length
```

简称为：

```text
ledger.event_count == runtime_ledger.events.length
```

## Event 对象

每条 event v0 字段固定为：

```text
index / phase / domain / status / source / focus
```

```json
{
  "index": 0,
  "phase": "semantic.host.cold",
  "domain": "semantic",
  "status": "standing",
  "source": "out/minimal-kernel-runtime-evidence/host/ci/summary.json",
  "focus": ["host", "cold"]
}
```

字段语义：

- `index`：从 0 开始的稳定顺序号，必须与 event 在 `events` 数组中的位置一致。
- `phase`：session fact phase 名称。
- `domain`：phase 所属事实域。
- `status`：该 phase 在当前 session facts 中的 standing 状态。
- `source`：session exporter 已消费的 summary artifact 路径；允许为 `null`，但不得指向 raw log 作为语义来源。
- `focus`：该 event 影响的最小关注面标签。

## Status Vocabulary

v0 只承认三个 status：

```text
standing / missing / collapsed
```

- `standing`：该 phase 的 summary fact 已被 exporter 观测并支撑当前 session 事实。
- `missing`：该 phase 在当前输入 summary 中缺失或不足以支撑对应 fact。
- `collapsed`：该 phase 对应的汇总结论已经坍塌，当前 v0 主要用于 `session.verdict`。

`missing` 不自动等于 session collapse。例如旧 QEMU summary 缺少 `arch_ingress_seam` 时，`arch.ingress.seam` event 可以是 `missing`；session 是否仍为 `standing` 由 session exporter 对 machine/runtime facts 与 failures 的既有规则决定。

## Domain Vocabulary

v0 只承认四个 domain：

```text
semantic / machine / runtime / bundle
```

- `semantic`：host 侧语义证据。
- `machine`：ARMv7-A QEMU lower-half 机器证据。
- `runtime`：tick、trap、thread、task syscall、handoff continuity 等运行会话事实。
- `bundle`：session verdict 与 bundle-level 收口事实。

## Phase Vocabulary

v0 phase 顺序固定如下：

| Index | Phase | Domain | Focus |
| --- | --- | --- | --- |
| 0 | `semantic.host.cold` | `semantic` | `host`, `cold` |
| 1 | `semantic.host.warm` | `semantic` | `host`, `warm` |
| 2 | `machine.qemu.lower_half` | `machine` | `qemu`, `lower-half` |
| 3 | `arch.ingress.seam` | `machine` | `exception`, `interrupt`, `timer`, `trap`, `context`, `runtime_loop` |
| 4 | `runtime.tick` | `runtime` | `timer`, `tick` |
| 5 | `runtime.trap` | `runtime` | `trap`, `svc` |
| 6 | `runtime.thread` | `runtime` | `thread`, `context` |
| 7 | `runtime.task_syscall` | `runtime` | `task`, `syscall` |
| 8 | `runtime.handoff_continuity` | `runtime` | `handoff`, `continuity` |
| 9 | `session.verdict` | `bundle` | `session` |

`arch.ingress.seam` 是 preferred lower-half ingress anchor。新 ARMv7-A QEMU summary 应让该 event 为 `standing`，并让 `kernel_runtime_session.summary.json.machine_witness.standing_cases` 包含 `arch_ingress_seam`。旧 summary 缺少该 case 时，event 可以为 `missing`，兼容 fallback 仍由 session exporter 持有。

## 消费边界

`runtime_ledger` 的合法消费者包括：

- `kernel_runtime_session.summary.json`：通过 `ledger.runtime_ledger` 与 `ledger.event_count` 引用 ledger。
- runtime evidence bundle：把 `session/runtime_ledger.json` 作为 session 侧车 artifact 出口。
- system compiler witness bundle：通过 `kernel_runtime_session` witness entry 间接暴露 ledger。
- runtime-session inspect compare consumer：把 ledger 作为 supporting artifact 和 explain hop 上下文。

这些消费者只能消费 exporter 已经写入 ledger 的 fact events。它们不得：

- 重新解析 QEMU 串口日志。
- 重新解析 host smoke 原始日志。
- 用 ledger 替代 `kernel_runtime_session.summary.json` 的 verdict。
- 用 ledger 新增 compare verdict、drift rule 或 collapse rule。
- 把 ledger event 当作 scheduler trace、thread timeline 或 failure taxonomy。

## 与 Session Summary 的关系

`kernel_runtime_session.summary.json` 是 session 的共同被证明对象；`runtime_ledger.json` 是它的事实顺序侧车。

二者关系固定为：

```text
session summary 定义本次运行会话是否 standing
runtime ledger 记录 exporter 已消费事实的顺序
runtime evidence bundle 负责把二者作为 artifact 共同出口
```

因此，上层如果需要判断 session 是否 standing，应消费 `kernel_runtime_session.summary.json.verdict` 与既有 witness/compare 对象；如果需要解释 session facts 的顺序，应消费 `runtime_ledger.json.events`。

## 非目标

本 contract v0 不做：

- 不新增 `runtime_ledger` schema 文件。
- 不新增独立 ledger validator、checker、smoke 或 CI gate。
- 不新增 compare verdict、drift rule 或 collapse rule。
- 不定义完整 failure code 表。
- 不把 ledger 变成 raw log parser。
- 不把 ledger 变成 scheduler trace 或 runtime profiler。
- 不实现 C++ `ArchExceptionPort`、`ArchInterruptPort`、`ArchTimerPort`、`ArchContextPort`。

如果后续需要把 ledger 做成独立可验对象，应另开 `Runtime Ledger Schema/Check v0`，而不是在本 contract 中提前冻结机器 schema。
