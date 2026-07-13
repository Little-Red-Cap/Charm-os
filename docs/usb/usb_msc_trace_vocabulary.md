# USB MSC Trace Vocabulary

## 文档状态

- `status`: `supporting`
- `scope`: `usb.class_msc` 内部 trace 的字段解释
- `source`: [`usb.msc.cppm`](../../Modules/io/usb/class/usb.msc.cppm)

`MscTraceEvent` 用于 native fixture 和诊断，不是 USB wire format、稳定 ABI 或产品 telemetry。
事件种类、字段和写入位置以源码为准。

## 事件族

| 事件族 | kinds | 主要字段 |
|---|---|---|
| CBW | `cbw_received`、`cbw_invalid` | `command`、host `transfer_length`、方向 `flag`；invalid 还记录 `residue` |
| command | `read_capacity`、`read10_started`、`write10_started` | `command`、`transfer_length`、`lba`、`blocks` |
| data | `data_in_started`、`data_out_started` | 实际 data-stage `transfer_length`、`lba`、`blocks`、预计 `residue` |
| recovery | `stall_in_requested`、`stall_out_requested`、`wait_csw`、`clear_stall_seen`、`phase_error` | host length、residue、stall 方向 |
| sense | `sense_set` | `sense_key`、`sense_asc`、`sense_ascq` 与来源 command |
| CSW | `csw_ready`、`csw_sent` | `command`、`residue`、是否 phase error 的 `flag` |

`read_capacity.lba` 表示最后可访问 LBA，`blocks` 表示总块数。`read10_started` 与
`write10_started` 的 `transfer_length` 来自 CBW；对应 `data_*_started` 记录设备实际准备处理的长度。

## 解释规则

- 结构没有字段 presence mask；未用于当前 kind 的字段保持默认值，不能按统一 schema 解读。
- `flag` 是重载字段：CBW 表示 IN 方向，clear-stall 表示 IN endpoint，CSW 表示 phase error。
- `csw_ready` 表示 CSW 已形成，`csw_sent` 表示它已由 data path 取出；二者不能互换。
- `wait_csw` 只表示 class 已等待 clear-stall，不证明主机一定会完成恢复。
- `sense_set` 记录 sense 来源，不等于 REQUEST SENSE 已被主机读取。

## 记录边界

- `MscBot` 只保留 64 个事件；追加失败被忽略，没有 overflow counter。
- event 没有时间戳、全局 sequence、endpoint identity 或跨 reset correlation。
- `clear_trace()` 只清空记录，不重置 BOT 状态。
- trace 适合单个 fixture 内按 kind 和字段断言，不适合作为长期日志协议。

当前 boardlog fixture 场景见
[`usb_boardlog_coverage_matrix.md`](usb_boardlog_coverage_matrix.md)。通过这些 fixture 不证明真实控制器
时序、并发、DMA、cache 或主机兼容性。
