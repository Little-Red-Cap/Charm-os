# USB Boardlog 导入格式

## 文档状态

- `status`: `supporting`
- `scope`: `usb.boardlog` 文本到 `usb.replay.v1` 的当前映射
- `source`: [`usb.boardlog.cppm`](../../Modules/io/usb/mock/usb.boardlog.cppm)

Boardlog 是宽松的开发日志导入格式，没有显式 schema/version header。它不是 USB trace 标准、
时间序列格式或电气层记录。

## 输入行

| 输入 | 必需字段 | replay 结果 |
|---|---|---|
| `usb: connect [state]` | 可选 `on/off/true/false/connected/disconnected/1/0` | `connect`，省略 state 时为 true |
| `usb: reset` | 无 | `reset` |
| `usb: stall ep=<hex>` | `ep` | `stall` |
| `usb: out ...` | `ep`、`zlp`、`data` | 单个 `out` step |
| `usb: in ...` | `ep`、`zlp`、`data` | 同 endpoint 的连续未结束 IN 合并为一个事务 |
| `usb: dev_desc size=<N> <bytes...>` | `size` token 与至少一个 byte | 缓存 descriptor，不生成 step |
| `usb: cfg_desc size=<N> <bytes...>` | 同上 | 缓存 descriptor，不生成 step |
| `usb: setup ...` | `bm/bmRequestType`、`b/bRequest`、`wv/wValue`、`wi/wIndex`、`wl/wLen` | 按下表翻译 control step |

`data=-` 表示空 payload。Hex payload 可含 `,`、`:`、`_`、`-` 分隔符；清理分隔符后必须是
偶数个 hex digit。Descriptor 的 `size=<N>` 当前只作为 byte 列表起点，不校验 N 与实际长度。

## Setup 映射

| request | 结果 |
|---|---|
| IN `GET_DESCRIPTOR(Device)` | 从此前 `dev_desc` 缓存裁剪到 `wLength` 后生成 `control_in` |
| IN `GET_DESCRIPTOR(Config)` | 从此前 `cfg_desc` 缓存裁剪到 `wLength` 后生成 `control_in` |
| `GET_MAX_LUN` (`A1/FE`, length 1) | 固定 payload `00` 的 `control_in` |
| OUT `CLEAR_FEATURE(ENDPOINT_HALT)` | `clear_stall`，endpoint 取 `wIndex` 低字节 |
| 其它 `wLength=0` OUT request | 带 ZLP 期望的 `control_out` |
| 其它 setup | 不生成 step，增加 `skipped_steps` |

Descriptor GET 出现在对应缓存之前返回 `missing_descriptor`；不支持的 descriptor type 被跳过。

## 结果与失败

`LoadResult` 返回 replay trace、错误行、`imported_steps` 和 `skipped_steps`。错误分类为：

- `syntax`：必需字段缺失或值越界；
- `invalid_hex`：IN/OUT payload 不是可解析 byte sequence；
- `missing_descriptor`：descriptor GET 没有前置缓存；
- `file_io`：`load_file()` 打开或读取失败。

空行和大多数无关日志被忽略，但不会统一增加 `skipped_steps`。`to_text()` 序列化的是
规范化 `usb.replay.v1`，不能还原原始 boardlog 文本、注释、分包或被忽略行。

## 限制

- endpoint 没有 bulk/interrupt/isochronous 类型信息；
- IN 可按 endpoint 合并，OUT 始终保留逐包 step；
- 没有 timestamp、frame number、IRQ、DMA、cache 或 stall cause；
- 不导入 string descriptor 内容；
- 导入成功只证明语法可投影，不证明 replay 通过或真实设备行为正确。
