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
- `usb: dev_desc size=<N> <hex bytes...>`
- `usb: cfg_desc size=<N> <hex bytes...>`
- `usb: setup bm=0x.. b=0x.. wValue=0x.... wIndex=0x.... wLen=0x....`

无关行会被忽略，例如启动日志、串口日志或其它外设日志。

## 导入规则

- `usb: connect on/off`
  - 导入为 `connect true/false`

- `usb: reset`
  - 导入为 `reset`

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

- 还不导入普通 bulk `IN/OUT` 数据日志
- 还不导入中断端点或等时端点日志
- 还不导入字符串描述符读取
- 如果 `GET_DESCRIPTOR(Device/Config)` 出现在对应描述符缓存之前，导入会失败

## 推荐日志顺序

- 先记录 `connect/reset`
- 再记录 `dev_desc/cfg_desc`
- 再记录关键 `setup`
- 如果涉及恢复路径，保留 `CLEAR_FEATURE` setup

## 推荐用途

- 从真板日志快速还原枚举序列
- 从真板日志还原 clear-stall / 标准请求路径
- 给 native replay 构建最小 regression fixture
