# SPI Transaction Mock Readiness

> **文档状态：`exploration`（尚无对应 mock）**

现行接口状态见 [`../architecture/spi_device_contract_v0.md`](../architecture/spi_device_contract_v0.md)。

## 当前事实

- 仓库有 `hal_spi`、SPI node 和 SPI flash binding；
- 没有独立的 SPI transaction script/mock smoke；
- 现有 HAL 或 flash 代码不能证明统一的 transaction、CS、timeout 或错误语义。

## 最小推进条件

实现一个固定容量 mock，至少覆盖：

- transfer 顺序与 TX/RX bytes；
- 配置或参数不匹配；
- backend busy/timeout/io failure；
- 脚本未消费完与调用超出脚本。

Host mock 必须标为 Host 证据，不得使用 producer/fact 名称冒充 real-board 结果。
