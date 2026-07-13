# Device Interface Drafts v0 归档

## 归档原因

本目录保留 SPI、GPIO、Block、Stream IO 和 Timebase 早期提案中仍有独立价值的职责、错误和
执行语义。五份重复的 Contract Shape/Facts/Evidence/Next Steps 模板已收敛到
[`device_interface_retained_notes.md`](device_interface_retained_notes.md)。

## 当前入口

默认阅读应从 [`../../architecture/README.md`](../../architecture/README.md) 进入，再按主题查看现行短状态卡：

- [`../../architecture/spi_device_contract_v0.md`](../../architecture/spi_device_contract_v0.md)
- [`../../architecture/gpio_device_contract_v0.md`](../../architecture/gpio_device_contract_v0.md)
- [`../../storage/block_device_contract.md`](../../storage/block_device_contract.md)
- [`../../architecture/stream_io_device_contract_v0.md`](../../architecture/stream_io_device_contract_v0.md)
- [`../../architecture/timebase_device_contract_v0.md`](../../architecture/timebase_device_contract_v0.md)

归档不等于否定设计方向，也不等于实现已经成立。若未来重新推进，应先以现行源码、CMake 接线和可重复 smoke 重新核对原文中的每项主张，再申请提升状态。
