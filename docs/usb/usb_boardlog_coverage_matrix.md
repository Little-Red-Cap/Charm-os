# USB Boardlog Coverage Matrix

本文档冻结当前 `usb_msc_boardlog_import_smoke` 已覆盖的 `boardlog -> replay -> runtime` 场景，用来说明：

- 哪些 `boardlog` 语义已经被独立夹具固定
- 每条夹具主要压住哪些 MSC/BOT 语义
- 后续继续补夹具时，哪里还存在明显缺口

## 适用范围

当前矩阵主要对应：

- `Examples/usb/usb_msc_boardlog_import_smoke`
- `Examples/usb/usb_msc_boardlog_import_smoke/fixtures`
- `docs/usb/usb_boardlog_format.md`
- `scripts/usb_native_smoke.ps1`

这里记录的是**独立板级输入夹具**，而不是所有 replay fixture。

当前夹具已开始采用混合形态维护：

- 纯静态场景逐步外置到 `fixtures/*.boardlog`
- 仍需动态拼接 payload 的场景暂时保留在 `main.cpp`

## 当前覆盖

### Parser / grammar 级

- `ZLP boardlog`
  - 验证 `usb: out/in ... zlp=1 data=-`
  - 固定独立零长度包的导入与 roundtrip

- `segmented IN boardlog`
  - 验证连续 `usb: in` 的自动合并
  - 固定事务级 `in` 聚合与末包 `zlp` 结束语义

## MSC 集成夹具

- `msc.boardlog`
  - 覆盖 `stall_out -> clear_stall -> phase-error CSW -> REQUEST SENSE`
  - 固定文件型板级恢复片段导入链路

- `segmented-out short`
  - 覆盖连续 `usb: out` 按包级保留、不自动合并
  - 固定 `write10 short` 的 `data_out_started`、`csw_sent` 与落盘结果

- `segmented-out overrun recovery`
  - 覆盖 `OUT segmentation + stall_out + clear_stall + phase_error`
  - 固定 `wait_csw`、`clear_stall_seen`、`csw_sent(flag=true)`

- `invalid-cbw recovery`
  - 覆盖 `cbw_invalid + stall_in + clear_stall + phase-error CSW`
  - 固定后续 `READ CAPACITY(10)` 恢复命令可继续执行

- `read10-short`
  - 覆盖 `data + trailing CSW` 处于同一 `in` 事务
  - 固定 `data_in_started(residue=512)`、`csw_ready`、`csw_sent(flag=false)`

- `write10 read-only + request-sense`
  - 覆盖失败 `CSW` 后的 `REQUEST SENSE`
  - 固定 `sense_set(0x07/0x27/0x00)` 与恢复后的 sense 返回

- `read-capacity residue`
  - 覆盖 `READ CAPACITY(10)` 主机长度大于设备返回长度
  - 固定 `read_capacity`、`csw_ready(residue=2)`、`csw_sent(residue=2)`

- `read10-zero-len recovery`
  - 覆盖 `transfer_length=0` 的 `read10` 错误恢复
  - 固定 `stall_in`、`wait_csw`、`phase_error`、`sense_set(0x05/0x20/0x00)` 与后续 `REQUEST SENSE`

- `read10-overrun recovery`
  - 覆盖 `host_len > expected` 的 `IN` 方向 overrun
  - 固定 `data_in_started(residue=512)`、`stall_in`、`clear_stall_seen`、`phase_error`、`csw_sent(flag=true)`

## 当前已固定的语义维度

### Boardlog grammar

- `connect/reset`
- `dev_desc/cfg_desc`
- `setup`
- `out/in`
- `stall`
- `clear_stall`
- `zlp`
- `segmented in`
- `segmented out`

### Host / device 观测面

- `HostEventKind::connect`
- `HostEventKind::reset`
- `HostEventKind::out_packet`
- `HostEventKind::in_complete`
- `HostEventKind::clear_stall`
- `DeviceActionKind::stall_ep`
- endpoint halted / cleared 状态

### MSC trace vocabulary

- `cbw_invalid`
- `read_capacity`
- `read10_started`
- `write10_started`
- `data_in_started`
- `data_out_started`
- `stall_in_requested`
- `stall_out_requested`
- `wait_csw`
- `phase_error`
- `clear_stall_seen`
- `sense_set`
- `csw_ready`
- `csw_sent`

## 当前仍然明显的缺口

- 夹具目前仍是“外置文件 + 内嵌字符串”混合状态，`main.cpp` 仍然偏大
- 还没有把所有最小板级夹具系统性外置成独立 `.boardlog` 文件
- 还没有单独覆盖 `BOT reset` 风格板级片段
- 还没有覆盖字符串描述符读取类 `boardlog` 输入
- `boardlog` 仍然是语义级输入，不追求时间戳或电气级拟真

## 下一步建议

- 把高价值夹具继续外置成 `Examples/usb/usb_msc_boardlog_import_smoke/fixtures/*.boardlog`
- 给外置夹具建立一个很小的清单，减少 `main.cpp` 继续膨胀
- 优先补 `BOT reset` 或其它仍未独立固定的恢复片段
