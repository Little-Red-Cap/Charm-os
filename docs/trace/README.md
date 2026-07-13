# Trace 实现状态

## 文档状态

- `status`: `supporting`
- `scope`: `trace_core`、`service_trace`、`kernel.trace` 与局部 producer adapter
- `source`: `Modules/core/trace_core.cppm`、`Modules/core/service/service_trace*.cppm`

## Core vocabulary

`trace_core` 只定义 event/counter/span 的共享 kind 和 totals 统计辅助，不提供全局 sink、router 或
buffer。Totals 只累计各 kind 数量和 span total/max，不记录 timestamp、producer 或 ID。

## Buffer 与聚合

`service::TraceRecord` 保存 time、32-bit ID、64-bit payload、count 和 kind。
`service::TraceBuffer<Tick, Capacity>` 是固定容量 circular buffer；写满后覆盖最旧记录，没有 overflow
counter。consumer 需要结合 `head()` 与 `size()` 还原顺序，`data()` 不是天然的时间顺序 view。

`service::TraceAggregator<Tick, MaxIds>` 按 ID 聚合 event、counter 和 span。ID slot 用尽后，新 ID 会
复用第一个 stat slot，因此结果不再能区分这些 ID；实现没有 overflow/error 状态。

`kernel.trace` 使用包含 `TaskId` 与 `EventId` 的独立 record/buffer，并支持合并连续相同记录。它与
`service::TraceBuffer` 共享 `TraceKind/TraceStats`，但不是同一种 record ABI。

## Producer 边界

Input/GUI 使用各自的静态 circular buffer、局部 ID 和进程内 sequence；power 使用全局 non-owning
callback，未设置时静默丢弃；service trace bus 只包装 record 指针，不转移 ownership。其它
kernel/runtime trace 也不会自动汇入这些结构。

仓库没有全局 trace ID registry、编号范围或 collision 检查。ID 只在 producer/domain 上下文中有意义；
跨 producer 合并时必须额外保留来源，不能仅按裸整数 ID 聚合。

## 未提供

- thread safety、IRQ/多核并发保证或 memory ordering；
- 统一 clock、timestamp 单位或跨 domain 时间同步；
- callback 非阻塞、无分配或 bounded-time 的运行时执法；
- filter、sampling、routing、serialization 或持久化 schema；
- buffer overflow、aggregator ID overflow 或 sink drop 统计；
- 稳定公共 ABI 或跨版本 ID 兼容承诺。

验证入口：`Examples/system/power_demo`、`Examples/ink/demo` 以及使用各局部 trace buffer 的 host
smoke。通过这些 fixture 不证明真实板 IRQ、多核、时钟或长期 telemetry 行为。
