# M3 Observability & Performance (Draft)

> `status`: `archived`。CSV 字段与 observability API 已发生变化。

## 原范围

该阶段把 trace、snapshot JSON、task/event/source diagnostics、alert、replay、dedup/debounce/coalesce
和 drop policy 视为默认关闭的可选能力，并提出：

1. feature 由 config flag 控制；
2. 实验接口标为 optional；
3. 只提供最小 M3 demo。

原 freeze 声称 trace buffer/format、replay 与 alert hook 稳定，但没有持续约束当前源码的权威。

## 原导出边界

- `kernel.scheduler` 产生 snapshot/task/trace observation；
- `kernel.scheduler_export` 负责 JSON/CSV presentation；
- 当时 CSV header 为 `trace_v1,t,task,id,payload,count,kind`。

当前 CSV 已包含额外 SSU 字段。任何格式依赖必须读取当前 `scheduler_export.cppm` 和相应 smoke，
不能使用本阶段草案。
