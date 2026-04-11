# USB MSC Trace Vocabulary v1

本文档冻结当前 `usb.class_msc` 在 replay / regression 中可依赖的一组 trace 事件与字段语义。

## 目标

- 给 replay suite 提供稳定的强断言词汇
- 明确每个事件哪些字段有效
- 在 helper 抽象前先固定语义字典

## 通用字段

- `command`：当前 CBW 的 SCSI opcode
- `transfer_length`：当前事件关联的主机传输长度或设备数据阶段长度
- `lba`：当前命令涉及的起始逻辑块地址
- `blocks`：当前命令涉及的块数
- `residue`：当前事件对应的 CSW residue 或预计 residue
- `flag`：事件相关的布尔语义，具体含义由事件定义

## 事件语义

- `cbw_received`
  - 有效字段：`command` `transfer_length` `flag`
  - `flag=true` 表示 CBW 声明方向为 IN

- `cbw_invalid`
  - 有效字段：`command` `transfer_length` `residue` `flag`
  - `residue` 等于无效 CBW 进入 phase error 时的 CSW residue
  - `flag=true` 表示无效 CBW 声明方向为 IN

- `read_capacity`
  - 有效字段：`command` `transfer_length` `lba` `blocks`
  - `lba` 表示最后一个可访问 LBA
  - `blocks` 表示设备总块数

- `read10_started`
  - 有效字段：`command` `transfer_length` `lba` `blocks`
  - `transfer_length` 是主机在 CBW 中声明的 data transfer length

- `write10_started`
  - 有效字段：`command` `transfer_length` `lba` `blocks`
  - `transfer_length` 是主机在 CBW 中声明的 data transfer length

- `data_in_started`
  - 有效字段：`command` `transfer_length` `lba` `blocks` `residue`
  - `transfer_length` 是设备实际准备进入的数据阶段长度
  - `residue` 在 overrun / short-transfer 场景下表示当前已知的预计 CSW residue

- `data_out_started`
  - 有效字段：`command` `transfer_length` `lba` `blocks` `residue`
  - `transfer_length` 是设备实际接受的数据阶段长度
  - `residue` 在 overrun / short-transfer 场景下表示当前已知的预计 CSW residue

- `stall_in_requested`
  - 有效字段：`command` `transfer_length` `residue` `flag`
  - `residue` 等于请求 stall 时计划进入 CSW 的 residue
  - `flag` 固定表示 IN 方向

- `stall_out_requested`
  - 有效字段：`command` `transfer_length` `residue` `flag`
  - `residue` 等于请求 stall 时计划进入 CSW 的 residue
  - `flag=false` 表示 OUT 方向

- `wait_csw`
  - 有效字段：`command` `transfer_length`
  - 表示 class 逻辑已经进入“等待 clear-stall 后再放行 CSW”的门控状态

- `clear_stall_seen`
  - 有效字段：`command` `flag`
  - `flag=true` 表示清的是 IN endpoint stall
  - `flag=false` 表示清的是 OUT endpoint stall

- `phase_error`
  - 有效字段：`command` `transfer_length` `residue`
  - 表示当前命令已经被判定为 phase error

- `sense_set`
  - 有效字段：`command` `sense_key` `sense_asc` `sense_ascq` `transfer_length`
  - 用于校验 REQUEST SENSE 前的语义来源是否正确

- `csw_ready`
  - 有效字段：`command` `residue` `flag`
  - `residue` 等于即将发出的 CSW residue
  - `flag=true` 表示即将发出的 CSW 状态是 `phase_error`

- `csw_sent`
  - 有效字段：`command` `residue` `flag`
  - `residue` 等于实际发出的 CSW residue
  - `flag=true` 表示实际发出的 CSW 状态是 `phase_error`

## 强断言建议

- 枚举后标准 MSC happy path：至少断言 `read_capacity` / `csw_sent`
- short-transfer：至少断言 `data_in_started` 或 `data_out_started` 与 `csw_sent.residue`
- overrun / bad-direction / invalid-CBW：至少断言 `stall_*_requested` `wait_csw` `clear_stall_seen` `csw_sent`
- REQUEST SENSE 路径：至少断言 `sense_set`

## 当前回归覆盖

- `READ CAPACITY(10)` residue
- `READ10` short / overrun / zero-length / bad-direction
- `WRITE10` short / overrun
- `invalid CBW` clear-stall recovery
