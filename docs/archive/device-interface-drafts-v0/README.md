# Device Interface Drafts v0 归档

## 归档原因

本目录保存 SPI、GPIO、Block、Stream IO 和 Timebase 五份早期 device interface 提案的完整正文。它们包含接口取舍、候选错误语义、职责边界和未来 evidence 方向，具有讨论和追溯价值；但它们没有被源码、统一装配和独立证据共同证明为稳定公共契约。

## 当前入口

默认阅读应从 [`../../architecture/README.md`](../../architecture/README.md) 进入，再按主题查看现行短状态卡：

- [`../../architecture/spi_device_contract_v0.md`](../../architecture/spi_device_contract_v0.md)
- [`../../architecture/gpio_device_contract_v0.md`](../../architecture/gpio_device_contract_v0.md)
- [`../../architecture/block_device_contract_v0.md`](../../architecture/block_device_contract_v0.md)
- [`../../architecture/stream_io_device_contract_v0.md`](../../architecture/stream_io_device_contract_v0.md)
- [`../../architecture/timebase_device_contract_v0.md`](../../architecture/timebase_device_contract_v0.md)

归档不等于否定设计方向，也不等于实现已经成立。若未来重新推进，应先以现行源码、CMake 接线和可重复 smoke 重新核对原文中的每项主张，再申请提升状态。
