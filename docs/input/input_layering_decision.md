# Input 分层边界

> status: supporting
>
> 本文描述当前 `Modules/io/hal` 与 `Modules/io/input` 的实现边界，不定义 UI
> 交互策略。IO 上位规则见 [`../io/io_layering_overview.md`](../io/io_layering_overview.md)。

## 当前链路

```text
platform/host backend
-> hal::RawInputDriver / RawInputSource
-> input::InputService / RawSampler
-> input::RawInputEvent
-> RawSink / Router / consumer adapter
-> input::Intent or domain event
```

| 层 | 负责 | 不负责 |
|---|---|---|
| backend | GPIO、ADC、touch、encoder 或 host event 的读取 | debounce、UI intent |
| `hal_input` | `is_down/read_pointer/read_axis/pop_encoder_ab` 函数表 | 事件队列、时间和策略 |
| `RawSampler` | button debounce/repeat、pointer transition、encoder decode | focus、gesture、控件行为 |
| `InputService` | clock 与 sampler 组合，单次或有预算轮询 | 调度、长期队列所有权 |
| `InputPumpTask` | EDA timer 驱动、budget、sink backpressure | backend 访问细节、UI dispatch |
| `Router` | 固定 16 个 raw subscriber、按类型 mask 分发 | subscriber 生命周期所有权 |
| domain adapter | raw event 到 intent/scene/action 的解释 | 硬件采样 |

HAL 只报告设备事实。click、long-press、navigation、gesture、focus 与控件 action
属于 adapter/domain；不能下沉进 `hal_input`。

## RawInputEvent

当前类型是显式字段结构，不采用 VSF 风格的通用 `id/pre/cur` 位段：

- `Button`：button、pressed、timestamp；
- `Pointer`：down/x/y/id、`Down/Move/Up`、timestamp；
- `Axis`：x/y、timestamp；
- `Encoder`：signed delta、timestamp。

Backend 应直接填充这些字段或实现 `RawInputDriver`，不要把 SDL event、Linux
`input_event`、HID report 或 GPIO handle 穿透给消费者。

## 调度与背压

- `RawSampler::poll()` 每次最多返回一个事件。
- `InputService::poll_raw()` 需要有效 clock；测试可用 `poll_raw_at()` 注入时间。
- `InputPumpTask` 默认周期 `16 ms`、budget `8`；达到 budget 时可通过 `post_more`
  继续处理，否则安排下一周期。
- sink 返回 `false` 时本轮停止，不进行内部重试或阻塞。
- `input::RingQueue::push()` 在满时返回 `false`；当前 queue 本身没有统一 drop counter
  或 trace，调用方必须决定统计和丢弃策略。

## 当前限制

- `RawSampler` 当前只轮询 encoder、四个 button 和 pointer，未从 `read_axis()`
  产生 `Axis` event。
- button release 只更新 sampler 内部状态，不产生 `pressed=false` event；repeat 产生
  额外的 `pressed=true` event。
- `input.events`、`input.intent` 与 `RawInputEvent` 同时存在，但没有仓库级唯一转换器；
  consumer adapter 仍需显式定义映射。
- `Router::dispatch()` 在第一个返回 `true` 的 subscriber 处停止，订阅顺序会影响消费。
- `RawInputSource` 不拥有 driver context；backend 必须保证其生命周期覆盖所有轮询。

## 验证入口

- [`../../Examples/io/raw_input_win_demo`](../../Examples/io/raw_input_win_demo)：host raw sampling；
- [`../../Examples/io/input_pump_win_demo`](../../Examples/io/input_pump_win_demo)：
  service、pump、sink 与 budget；
- [`../../Examples/init/bringup_minimal_observe_demo`](../../Examples/init/bringup_minimal_observe_demo)：
  `init.graph` 中的 input pump binding。

示例存在不证明所有 backend、并发、队列溢出或 UI 映射均已覆盖。
