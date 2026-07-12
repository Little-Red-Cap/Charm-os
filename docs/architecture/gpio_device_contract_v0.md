# GPIO Device Interface v0

## 文档角色

本文是 GPIO implementation interface 的当前状态卡，不是 Charm Core、Stable Boundary 或公共 ABI。

完整的早期讨论已保留在 [`../archive/device-interface-drafts-v0/gpio_device_contract_v0.md`](../archive/device-interface-drafts-v0/gpio_device_contract_v0.md)。准入规则见 [`interface_admission_policy.md`](interface_admission_policy.md)。

## 代码事实

当前可核对的接口是 `Modules/io/hal/hal_gpio.cppm`：

- `GpioPin` 用 port/pin 标识引脚；
- `GpioConfig` 描述 input/output、pull 和初始电平；
- `GpioIoHandle` 通过 `GpioOps` 提供 `init/write/read`；
- 缺失操作返回 `hal::Status::unsupported`；
- `GpioDriver` concept 的 `read` 返回电平，和 `GpioOps::read` 的 result/out 形状并不完全一致。

因此当前代码能证明 HAL 适配胚胎存在，但不能证明一个统一的 GPIO device contract 已成立。没有把 pin ownership、复用、边沿、IRQ、去抖和电气状态冻结为公共语义。

## 当前边界

- pin 的生命周期、独占/共享、初始化失败后的状态没有公共定义；
- input sampling、edge event 和 ISR/task 转发没有统一接口；
- active-high/low、pull、电气 drive strength 和安全默认值仍属平台或产品选择；
- `GpioPin`/`GpioConfig` 是 HAL 类型，不应直接成为跨平台应用 ABI。

## 状态与下一证据

状态：`proposed`（历史本地标签，不是 Constitution 裁决）。

若继续推进，先用一个 LED output 或 button input 证明最小 owner/consumer 语义，再单独验证 edge/IRQ；在此之前不把 GPIO provider、pin graph 或 edge taxonomy 提升为 Core 原语。
