# trace_core 统一诊断入口（约束版）

目标：让所有系统诊断事件通过 **同一条最小入口** 写入，避免格式化、策略与分层混乱。

## 1. 入口语义（固定）

trace_core 只允许四种语义：
- event：一次性事件（发生了什么）
- counter：计数累加（累积数量）
- span_begin：区间开始（测时）
- span_end：区间结束（测时）

禁止在 trace_core 内做格式化、过滤、路由、采样策略。

## 2. 写入规则（硬约束）

- 写入必须是 **fire-and-forget**（不阻塞、不分配）
- payload 只允许 POD 数值
- id 必须稳定且可复现（跨版本尽量保持）

## 3. 建议字段约定

- `id`：语义稳定的事件编号
- `payload`：数值型补充（例如队列长度、耗时、原因枚举）
- `count`：counter 累加量（默认 1）

## 4. 推荐使用层

- Kernel：调度、队列、等待、定时器、事件分发
- Service：buffer/stream/trace_bus
- UI/Audio：只在必要处写入高层指标（如 FPS、underrun）

## 5. 禁止项

- 不允许调用 out.format/out.logger
- 不允许在 trace_core 内做字符串拼装或解析
- 不允许从 Domain 反向依赖 Runtime 做 trace
