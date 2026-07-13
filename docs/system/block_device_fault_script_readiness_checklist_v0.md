# Block Device Fault Script Readiness

> **文档状态：`exploration`（尚无统一 fault script）**

现行接口状态见 [`../storage/block_device_contract.md`](../storage/block_device_contract.md)。

## 当前事实

- 仓库有 block device、file backend、cache、registry、stable slot、SDMMC/SPI flash 和 USB block adapter；
- provider/slot 示例验证装配与 detach，不验证介质故障；
- 没有统一的 short read/write、erase、flush、power-loss 或 bad-range fault script。

## 最小推进条件

先实现 memory/file-backed fault injector，至少覆盖：

- 越界和对齐错误；
- read/write/erase/flush failure；
- short operation 或明确拒绝 short operation；
- detach 后旧引用安全失败；
- fault 后介质状态与下一次操作行为。

filesystem、App Store 和 raw flash 应分别验证，不能共享一个模糊的“storage ok”结论。
