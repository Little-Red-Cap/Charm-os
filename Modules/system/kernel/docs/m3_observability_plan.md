# M3 Observability & Performance (Draft)

## Scope
Optional modules and features only. Default OFF in main route.

## Optional Modules
- kernel.trace
- diagnostics outputs (snapshot json, tasks json, events json, source json)
- alert hooks & thresholds
- replay
- dedup/debounce/coalesce
- drop policy

## Actions
1) Keep features behind Config flags (default false)
2) Mark experimental interfaces as Optional in docs
3) Provide minimal M3 demo only (main_m3.cpp)

## Freeze
- trace buffer and format APIs remain stable
- replay API stable
- alert hook signature stable

## Trace Export (Structured)
- `kernel.scheduler` owns structured observations (`snapshot`, `task_snapshot`, `trace_snapshot`)
- `kernel.scheduler_export` owns presentation helpers such as `format_trace_json()` and `format_trace_csv()`
- CSV header: `trace_v1,t,task,id,payload,count,kind`
