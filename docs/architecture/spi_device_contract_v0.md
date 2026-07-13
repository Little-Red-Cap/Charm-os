# SPI Device Interface v0

> status: `exploration`
>
> scope: 尚未统一的 SPI implementation interface

本文不定义 Charm Core、Stable Boundary 或公共 ABI。

早期未决 transaction/CS 语义见
[`Device Interface v0`](../archive/device-interface-drafts-v0/README.md#spi)。
准入规则见 [`interface_admission_policy.md`](interface_admission_policy.md)。

## 代码事实

`hal_spi` 提供配置、non-owning ops handle 和静态 driver concept，但没有证明统一装配、transaction
语义或真实硬件行为。当前也没有统一 transaction mock、准真实 device consumer 或 real-board
记录，因此不能将 HAL 形状解释为 device contract。

## 当前边界

- 传输是同步调用，事务边界、片选策略、并发/锁、DMA、超时和 ISR 安全均未被公共接口定义。
- `transfer` 的 TX/RX 长度关系、半双工语义、全双工语义和错误分类没有统一约定。
- 平台配置不应泄漏到上层，但当前 `SpiConfig` 仍是 HAL 配置类型，不应被误写成应用级 device ABI。

## 重新推进条件

若继续推进，先实现一个独立 transaction mock 和一个小型 SPI consumer，再决定是否需要 device-facing contract；证据应明确区分 Host、QEMU、准真实和 real board。未完成前，不将 `SpiDevice`、bus lock、CS 或 error taxonomy 加入 Charm Core。
