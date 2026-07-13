# Trace 文档入口

## 文档状态

- `status`: `supporting`
- `scope`: trace vocabulary、buffer 与局部 adapter
- `authority`: 当前 trace modules

当前实现边界见 [`trace_core_entry.md`](trace_core_entry.md)。具体事件 ID 由各 producer module 的
enum 或调用点定义；仓库没有全局 trace ID registry 或编号分区契约。

Trace 只提供诊断数据结构，不属于 Charm Core，也不定义日志格式、产品 telemetry 或持久化协议。
