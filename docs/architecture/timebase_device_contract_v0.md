# Timebase Device Interface v0

## 文档角色

本文是 time source implementation interface 的当前状态卡，不是 scheduler/timer contract、Charm Core 或公共 ABI。

完整的早期讨论已保留在 [`../archive/device-interface-drafts-v0/timebase_device_contract_v0.md`](../archive/device-interface-drafts-v0/timebase_device_contract_v0.md)。准入规则见 [`interface_admission_policy.md`](interface_admission_policy.md)。

## 代码事实

当前可核对的来源包括：

- `Modules/system/clock/system_clock.cppm` 的 `Clock/ClockRef/ClockOps` 提供 `now_ms/now_us` 和 `ClockBinding`；
- `Modules/platform/win` 有 steady/manual time source；
- `Modules/io/hal/hal_timer.cppm` 和 `Modules/system/kernel` timer 代码分别属于 HAL/controller 与 runtime queue 层。

这些是不同层次的时间胚胎，不能据此宣称已有统一 driver-facing Timebase contract。

## 当前边界

- monotonic、单位、分辨率、宽度和 wrap 行为没有被一个公共 interface 统一表达；
- `Clock` 未绑定时返回零，调用方不能仅凭数值区分 missing 与真实零时刻；
- 读取、sleep、timeout dispatch、timer queue 和 managed/replay time 必须分层；
- `try_sleep_ms` 等 busy-wait helper 不应被提升为基础 timebase 语义。

## 状态与下一证据

状态：`proposed`（历史本地标签，不是 Constitution 裁决）。

若继续推进，先定义一个只读 monotonic source 的最小事实和 missing/bound 行为，再以 timeout-aware consumer 做 Host/QEMU 证据；scheduler、sleep、replay 和 periodic callback 应留在更高层契约。
