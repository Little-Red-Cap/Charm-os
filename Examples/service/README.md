# Service 示例入口

本目录收纳 Service / signal-state / dispatcher 相关示例。

## 当前示例

### `service_core`

偏向 Service 核心能力验证，适合先看最小服务栈怎么组织。

### `service_ds_demo`

偏向数据结构/服务组合的演示样例。

### `service_signal_state_demo`

入口：

- [`service_signal_state_demo/README.md`](service_signal_state_demo/README.md)

这是当前最值得先看的示例，主要演示 signal/state v0 与 deferred signal 的边界。

## 建议阅读顺序

1. `service_signal_state_demo/README.md`
2. `service_core`
3. `service_ds_demo`

## 使用提醒

- 这里的示例更偏服务语义和状态/信号分发，不要把它们误当成 UI 或 IO 总入口。
