# GPIO Device Interface v0

> status: `exploration`
>
> scope: 尚未统一的 GPIO implementation interface

本文不定义 Charm Core、Stable Boundary 或公共 ABI。

早期 input/output/edge 未决语义见
[`Device Interface v0`](../archive/device-interface-drafts-v0/README.md#gpio)。
准入规则见 [`interface_admission_policy.md`](interface_admission_policy.md)。

## 代码事实

`hal_gpio` 提供 pin/config、non-owning ops handle 和静态 driver concept；ops 与 concept 的 read 形状
并不完全一致。它只能证明 HAL 适配接口存在，不能证明统一 GPIO device contract 已成立，也未冻结
pin ownership、复用、边沿、IRQ、去抖或电气状态。

## 当前边界

- pin 的生命周期、独占/共享、初始化失败后的状态没有公共定义；
- input sampling、edge event 和 ISR/task 转发没有统一接口；
- active-high/low、pull、电气 drive strength 和安全默认值仍属平台或产品选择；
- `GpioPin`/`GpioConfig` 是 HAL 类型，不应直接成为跨平台应用 ABI。

## 重新推进条件

若继续推进，先用一个 LED output 或 button input 证明最小 owner/consumer 语义，再单独验证 edge/IRQ；在此之前不把 GPIO provider、pin graph 或 edge taxonomy 提升为 Core 原语。
