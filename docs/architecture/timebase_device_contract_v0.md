# Timebase Device Interface v0

> status: `exploration`
>
> scope: 尚未统一的 time source implementation interface

本文不定义 scheduler/timer contract、Charm Core 或公共 ABI。

早期 monotonic/resolution/wrap 未决语义见
[`Device Interface v0`](../archive/device-interface-drafts-v0/README.md#timebase)。
准入规则见 [`interface_admission_policy.md`](interface_admission_policy.md)。

## 代码事实

当前 `system_clock` 提供 `Clock/ClockRef/ClockOps` 与 ms/us 读取，Win platform 有 steady/manual
source；HAL timer 与 kernel timer queue 属于不同层次。这些实现不能证明已有统一 driver-facing
Timebase contract。

## 当前边界

- monotonic、单位、分辨率、宽度和 wrap 行为没有被一个公共 interface 统一表达；
- `Clock` 未绑定时返回零，调用方不能仅凭数值区分 missing 与真实零时刻；
- 读取、sleep、timeout dispatch、timer queue 和 managed/replay time 必须分层；
- `try_sleep_ms` 等 busy-wait helper 不应被提升为基础 timebase 语义。

## 重新推进条件

若继续推进，先定义一个只读 monotonic source 的最小事实和 missing/bound 行为，再以 timeout-aware consumer 做 Host/QEMU 证据；scheduler、sleep、replay 和 periodic callback 应留在更高层契约。
