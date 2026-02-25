# 事件队列双后端策略

## 目标

保持 **同一 API**，根据配置选择不同的事件队列后端：

- **RingQueue 后端**：固定环形队列，低开销
- **ListQueue 后端**：链表队列，支持动态优先级与更灵活的调度

## 配置开关

在 `kernel.config` 中：

- `KernelConfig::enable_dynamic_priority`
  - 开启动态优先级（默认 false）
  - **会自动启用 ListQueue 后端**

- `KernelConfig::enable_event_queue_list`
  - 强制使用 ListQueue 后端（默认 false）
  - 用于后续扩展或对比测试

选择规则：

```
use_list_queue = enable_dynamic_priority || enable_event_queue_list
```

## 行为差异

| 后端 | 特点 | 适用 |
| --- | --- | --- |
| RingQueue | 固定容量、低开销 | 固定优先级、资源敏感 |
| ListQueue | 动态优先级、可移动任务 | 需要调度弹性 |

## 注意事项

- 两种后端提供一致的 `push/pop/cancel/drop` 能力。
- 当使用 ListQueue 时，`queue_depth()` 与 `max_queue` 统计来自统一计数。
