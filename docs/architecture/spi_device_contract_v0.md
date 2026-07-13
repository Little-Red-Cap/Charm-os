# SPI Device Interface v0

> status: `supporting`
>
> 本文是当前 SPI implementation interface 的状态卡，不定义 Charm Core、Stable Boundary
> 或公共 ABI。

## 文档角色

本文是 SPI implementation interface 的当前状态卡。它约束讨论范围，但不是 Charm Core、Stable Boundary 或公共 ABI。

完整的早期提案已保留在 [`../archive/device-interface-drafts-v0/spi_device_contract_v0.md`](../archive/device-interface-drafts-v0/spi_device_contract_v0.md)。准入规则见 [`interface_admission_policy.md`](interface_admission_policy.md)。

## 代码事实

当前可核对的接口是 `Modules/io/hal/hal_spi.cppm`：

- `SpiConfig` 描述频率、模式、位序和 bits；
- `SpiIoHandle` 通过 `SpiOps` 提供 `init/enable/disable/transfer`；
- 缺失操作返回 `hal::Status::unsupported`；
- `SpiDriver` concept 描述另一个静态 driver 形状，但没有证明已接入统一装配或真实硬件。

这属于 HAL/implementation 代码，不等于一个面向 device consumer 的 SPI 契约。当前仓库没有可作为本契约证据的统一 SPI transaction mock、准真实 SPI device consumer 或 real-board 记录。

## 当前边界

- 传输是同步调用，事务边界、片选策略、并发/锁、DMA、超时和 ISR 安全均未被公共接口定义。
- `transfer` 的 TX/RX 长度关系、半双工语义、全双工语义和错误分类没有统一约定。
- 平台配置不应泄漏到上层，但当前 `SpiConfig` 仍是 HAL 配置类型，不应被误写成应用级 device ABI。

## 状态与下一证据

状态：`proposed`（历史本地标签，不是 Constitution 裁决）。

若继续推进，先实现一个独立 transaction mock 和一个小型 SPI consumer，再决定是否需要 device-facing contract；证据应明确区分 Host、QEMU、准真实和 real board。未完成前，不将 `SpiDevice`、bus lock、CS 或 error taxonomy 加入 Charm Core。
