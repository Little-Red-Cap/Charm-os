# USB Boardlog Format v1

本文档冻结当前 `usb.boardlog` 导入器支持的一组最小板级日志语法，用于把真实板级 USB 日志导入为 `usb.replay.v1`。

## 目标

- 让板级日志能够稳定导入到 PC 原生 replay
- 让 `boardlog -> replay -> regression` 成为可维护链路
- 固定当前可依赖的最小日志 grammar，避免字段漂移

## 支持的日志行

- `usb: connect on`
- `usb: connect off`
- `usb: reset`
- `usb: stall ep=0x..`
- `usb: out ep=0x.. zlp=0|1 data=<hex|- >`
- `usb: in ep=0x.. zlp=0|1 data=<hex|- >`
- `usb: dev_desc size=<N> <hex bytes...>`
- `usb: cfg_desc size=<N> <hex bytes...>`
- `usb: setup bm=0x.. b=0x.. wValue=0x.... wIndex=0x.... wLen=0x....`

无关行会被忽略，例如启动日志、串口日志或其它外设日志。

## 导入规则

- `usb: connect on/off`
  - 导入为 `connect true/false`

- `usb: reset`
  - 导入为 `reset`

- `usb: stall ep=0x..`
  - 导入为 `stall ep=..`
  - 语义上表示设备在对应 endpoint 上发起 halt / STALL，供 replay 对 recovery 闭环做断言

- `usb: out ep=0x.. zlp=.. data=...`
  - 导入为 `out ep=.. zlp=.. data=...`
  - 语义上表示主机向设备发送 bulk/interrupt/其它非控制数据包
  - 当 `data=-` 且 `zlp=1` 时，表示该事务是一个独立的零长度包
  - 连续、同端点的多条 `usb: out` 当前按包级事件原样保留，不自动合并为更高层事务

- `usb: in ep=0x.. zlp=.. data=...`
  - 导入为 `in ep=.. zlp=.. data=...`
  - 语义上表示设备向主机返回的数据事务期望值
  - 当 `data=-` 且 `zlp=1` 时，表示该事务是一个独立的零长度包
  - 连续、同端点的多条 `usb: in` 会在导入时自动拼接为一个 replay `in` 事务，末条的 `zlp` 作为事务结束标记
  - 单条 replay `in` 表示事务级期望值，不承诺运行时只产生一次设备发包或一次 `in_complete`；例如 MSC 的 `data + trailing CSW` 可能在同一个 `in` 事务内拆成多次 ACK

- `usb: dev_desc`
  - 缓存设备描述符原始字节
  - 本行本身不生成 replay step

- `usb: cfg_desc`
  - 缓存配置描述符原始字节
  - 本行本身不生成 replay step

- `usb: setup ...`
  - `GET_DESCRIPTOR(Device)` 导入为 `control_in`，数据来自缓存的 `dev_desc`
  - `GET_DESCRIPTOR(Config)` 导入为 `control_in`，数据来自缓存的 `cfg_desc`
  - `GET_MAX_LUN` 导入为 `control_in`，当前固定响应 `00`
  - `CLEAR_FEATURE(ENDPOINT_HALT)` 导入为 `clear_stall ep=..`
  - 其它 `wLen=0` 的 OUT setup 导入为 `control_out` + `zlp=1`

## 当前限制

- 还不区分 bulk / interrupt / isoch 的端点语义，只按 `ep` + `data` 导入
- `usb: stall` 目前只记录 endpoint，不区分更细的 stall 原因或时间信息
- 多包分段目前只自动合并 `usb: in`；`usb: out` 仍按逐包事件导入
- 还不导入字符串描述符读取
- 如果 `GET_DESCRIPTOR(Device/Config)` 出现在对应描述符缓存之前，导入会失败

## 推荐日志顺序

- 先记录 `connect/reset`
- 再记录 `dev_desc/cfg_desc`
- 再记录关键 `setup`
- 如果涉及 stall / recovery，记录 `usb: stall ...`
- 如果涉及恢复路径，保留 `CLEAR_FEATURE` setup
- 如果涉及数据阶段，记录 `usb: out ...` / `usb: in ...`

## 推荐用途

- 从真板日志快速还原枚举序列
- 从真板日志还原 clear-stall / 标准请求路径
- 从真板日志还原 stall / clear-stall / CSW 的最小 recovery 闭环
- 从真板日志还原 MSC 最小数据阶段与 recovery 片段
- 给 native replay 构建最小 regression fixture
