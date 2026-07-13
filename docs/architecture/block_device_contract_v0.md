# Block Device Interface v0

> status: `supporting`
>
> 本文是当前 block storage implementation interface 的状态卡，不定义 Charm Core、文件系统
> 或公共 ABI。

## 文档角色

本文是 block storage implementation interface 的当前状态卡，不是文件系统契约、Charm Core 或公共 ABI。

完整的早期提案已保留在 [`../archive/device-interface-drafts-v0/block_device_contract_v0.md`](../archive/device-interface-drafts-v0/block_device_contract_v0.md)。准入规则见 [`interface_admission_policy.md`](interface_admission_policy.md)。

## 代码事实

当前 `Modules/io/block/block.device.cppm` 只是对 `fs::BlockDevice` 的别名和能力位辅助：

- `Device` 实际来自 `fs::BlockDevice`；
- `Caps` 表达 read/write/erase/flush/cached；
- `caps_from_ops` 从函数指针推导能力，`has_caps`/`is_read_only` 做检查；
- `Modules/io/block/block.spi_flash.cppm` 另有 SPI NOR binding 胚胎。

这能证明 block view 已被若干 filesystem/storage 代码消费，但不等于统一的介质契约已冻结。扇区大小、对齐、容量、寿命、同步、断电一致性和错误恢复仍由具体实现决定。

## 当前边界

- block device 与 filesystem、raw flash、App Store、eMMC/QSPI backend 的边界必须分开；
- `erase` 不是所有 block media 的通用操作，不能由能力位存在推断语义细节；
- 读写的粒度、短读写、缓存、flush 和 power-loss 行为没有统一公共承诺；
- 当前 host storage smoke 证明适配和错误路径，不证明任意真实介质。

## 状态与下一证据

状态：`proposed`（历史本地标签，不是 Constitution 裁决）。

若继续推进，先选一个小型 raw media adapter，明确 range/alignment/erase/flush 语义并做 fault-injection smoke，再分别接入 filesystem 或 App Store。不要把 filesystem API 或具体 QSPI/eMMC 控制器细节反向写入本契约。
