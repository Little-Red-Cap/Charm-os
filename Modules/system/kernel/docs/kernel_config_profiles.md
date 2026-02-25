# KernelConfig 推荐组合与行为说明

本页用于补齐 KernelConfig 的“推荐组合”和关键行为约束，避免默认全关导致误用。

## 推荐配置组合

### 1) 最小可运行（Minimal）

适合最小内核验证与单元测试。

```
enable_timer = false
enable_trace = false
enable_dynamic_priority = false
enable_event_queue_list = false
priority_levels = 1..2
evtq_capacity = 16..64
```

说明：
- 只保留 ring queue，时延最小、实现最简单。
- 适合 bring-up 与最小回归测试。

### 2) 低功耗/低抖动（LowPower）

适合 MCU 端，强调 idle 等待与较少 wakeup。

```
enable_timer = true
enable_trace = false
enable_dynamic_priority = false
enable_event_queue_list = false
priority_levels = 2..4
evtq_capacity = 64..256
```

说明：
- 配合 `IdlePolicy`，`run_budget()` 空转时调用 `Caps::Wakeup::wait()`。
- 仍使用 ring queue，保证可预测性。

### 3) 高吞吐/可观测（Throughput/Trace）

适合 PC 验证与调优场景。

```
enable_timer = true
enable_trace = true
enable_dynamic_priority = true  // 或 enable_event_queue_list = true
priority_levels = 4..8
evtq_capacity = 128..512
trace_capacity = 256..1024
```

说明：
- 动态优先级模式使用 ListQueue 后端。
- `queue_depth()` 与 `max_queue` 来自统一计数，不会被置零。

## 关键行为/约束

### 队列告警阈值策略

- 动态优先级（ListQueue）模式下，`queue_depth()`/`max_queue` 表示**总队列深度**。  
  推荐阈值（基于 `evtq_capacity`）：  
  - `alert_queue_warn = evtq_capacity * 0.7`  
  - `alert_queue_err  = evtq_capacity * 0.9`
- RingQueue 模式下，`max_queue` 是**单优先级队列深度**，阈值应按单队列容量设置。

### EventId 扩展

- `EventId` 的“最后一个枚举值”建议显式命名为 `count` 或 `max`，用于数组长度。
- 若继续使用 `user0/user1` 方案，应在文档中明确“新增 event 需同步调整计数”。

### ThreadBlockingTask 放行事件

- 目前 `ThreadBlockingTask` 在 blocked 状态仅放行少数事件（sync/init/terminate）。
- 建议把放行策略文档化，并预留“自定义放行 mask”的扩展点（便于 Audio/AT 等子系统）。

### 动态优先级 queue_depth

- 动态优先级（ListQueue）模式下，`queue_depth()` 仍返回真实深度。
- 若使用非统计型队列实现，应在此处明确返回不可用值并记录到诊断输出。
