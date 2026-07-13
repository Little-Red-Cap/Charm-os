# USB Boardlog 覆盖矩阵

## 文档状态

- `status`: `supporting`
- `scope`: `usb_msc_boardlog_import_smoke` 的场景索引
- `source`: [`main.cpp`](../../Examples/usb/usb_msc_boardlog_import_smoke/main.cpp) 与
  [`fixtures/`](../../Examples/usb/usb_msc_boardlog_import_smoke/fixtures/)

本矩阵只帮助定位 fixture。case、断言和 trace token 以 smoke source 为准；通过 replay 不证明真实
USB 时序或板级执行。

## Grammar 场景

| 场景 | 输入 | 主要检查 |
|---|---|---|
| ZLP | `main.cpp` 内嵌 | 独立 IN/OUT ZLP 的导入与文本 roundtrip |
| segmented IN | `main.cpp` 内嵌 | 同端点连续 IN 合并，末包 ZLP 结束事务 |

## MSC/BOT 场景

| 场景 | 输入 | 主要检查 |
|---|---|---|
| recovery baseline | `msc.boardlog` | stall、clear-stall、phase-error CSW、REQUEST SENSE |
| segmented OUT short | `main.cpp` 动态生成 | OUT 保持包级、short WRITE(10) 与落盘 |
| segmented OUT overrun | `main.cpp` 动态生成 | OUT overrun、stall recovery 与 phase error |
| invalid CBW recovery | `fixtures/invalid_cbw_recovery.boardlog` | invalid CBW 后恢复并继续 READ CAPACITY(10) |
| READ(10) short | `fixtures/read10_short.boardlog` | short data、residue 与 trailing CSW |
| read-only WRITE(10) | `fixtures/request_sense.boardlog` | write-protect failure 与后续 sense |
| READ CAPACITY residue | `fixtures/read_capacity_residue.boardlog` | host length 大于响应时的 residue |
| READ(10) zero length | `fixtures/read10_zero_len_recovery.boardlog` | zero-length error、stall recovery 与 sense |
| READ(10) overrun | `fixtures/read10_overrun_recovery.boardlog` | IN overrun、clear-stall、residue 与 phase error |

## 边界

- 文件 fixture 与 `main.cpp` 内嵌/动态输入并存；没有独立 fixture manifest。
- importer 覆盖 connect/reset、descriptor cache、setup、IN/OUT、STALL、clear-stall 与 ZLP；精确
  grammar 见 [`usb_boardlog_format.md`](usb_boardlog_format.md)。
- 当前没有独立 BOT reset 或 string descriptor boardlog 场景。
- boardlog 是语义事件输入，不包含时间戳、电气信号、IRQ、DMA 或 cache 行为。
- 具体分支由 smoke 断言定义，不能用本表替代测试源码或当次运行结果。
