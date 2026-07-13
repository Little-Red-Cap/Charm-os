# KernelConfig 与事件队列

## 文档状态

- `status`: `supporting`
- `scope`: `kernel.config` 与两种 event queue backend 的当前行为
- `source`: `config.cppm`、`event_queue.cppm`、`event_queue_list.cppm`

仓库当前没有名为 Minimal、LowPower 或 Throughput 的稳定 kernel profile。本页只记录源码默认值
和编译期约束；具体 target 通过自己的 Config 类型选择功能。

## 默认配置

| 类别 | 默认值 |
|---|---|
| timer / dynamic priority / list queue | disabled |
| priority levels / event capacity / timer capacity | `4 / 64 / 16` |
| dispatch budget | `0` |
| boost/filter/coalesce/rate-limit/task-boost | disabled |
| trace / alert | disabled，capacity/threshold 为 `0` |
| wakeup batch | `1` |
| SSU demand warn/error | `50 / 10` permille |

`validate_config<Config>()` 当前强制：

- priority levels 至少 `1`，event capacity 至少 `8`；
- timer 启用时 capacity 至少 `1`；timer merge 依赖 timer；
- event boost 需要非零 mask；debounce 需要非零 window；
- coalesce 需要 dedup、debounce 或 timer merge 之一；
- trace 需要非零 capacity；alert 至少有一个非零 warn/error threshold；
- wakeup batch 至少 `1`；SSU threshold 不超过 `1000`，且 error 不大于 warn。

配置字段存在不表示对应行为已在所有 scheduler/backend 路径验证。

## Queue backend

```cpp
use_list_queue = enable_dynamic_priority || enable_event_queue_list;
```

| backend | 存储 | 主要行为 |
|---|---|---|
| `EventQueue<Capacity>` | 单个固定 ring array | FIFO push/pop、coalesce、cancel、按 task/tag 删除 |
| `EventQueueList<Capacity, TaskCount, PriorityLevels>` | 固定 node/free/ready arrays | 每 task event list、每 priority ready list、运行期调整 ready priority |

两者均不动态分配。容量满时支持 `drop_newest`；`drop_oldest` 的具体淘汰范围由各 backend 实现决定。
List backend 的 `size()` 是总 node 数，ring backend 的 `size()` 是该 queue 的元素数。

## 边界

- Config 选择是编译期行为，不是运行期 policy service。
- backend 名称和建议容量不构成跨 target 性能保证。
- alert、trace、filter 顺序和 scheduler 统计以当前 scheduler 源码与当次测试为准，不在本页复制。
