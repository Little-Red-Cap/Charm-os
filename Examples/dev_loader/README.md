# Dev Loader 原型

## 文档状态

- `status`: `supporting`
- `scope`: board-free resident download/session 原型
- `authority`: 本目录 headers 与 [`Examples/system`](../system/README.md) fixture

## 边界

本目录提供 board-free resident download 原型。UART、USB、console 或 host frontend 只负责提供
bytes/commands；receive、verify、packet sequence 与 App handoff 共享同一状态机。

它不是产品 bootloader，不定义 reset/jump policy、slot、签名、rollback 或 USB class ownership。
H747 平台行为见 [`h747-lab dev_loader`](../project/h747-lab/apps/dev_loader/README.md)；App image/runtime 见
[`Examples/app_abi/README.md`](../app_abi/README.md)。

## 组成

`charm_dev_loader.hpp` 拥有 Session/storage 状态；commands、packets 与 byte transport 分别负责文本命令、
packet v0 和任意 byte chunk；received/store handoff 只把 `launch_ready` bytes 交给 App/Store 层。具体
helper 与文件拆分以本目录 headers 为准，不在 README 复制清单。

storage 是 caller-owned callback backend，可映射 RAM 或 media。Session 不拥有 storage，也不决定
receive/stage/execute region 的平台地址。

## Receive 与 packetstream

```text
begin(size, crc) -> write/fill* -> verify -> launch_dry_run
```

`launch_dry_run` 只把 verified session 标记为 `launch_ready`，不调用 App entry。abort 清理当前 receive
流程；实际 App stage/probe/run 由上层显式触发。

packetstream 是连续 little-endian `PacketHeader + payload`：

```text
begin -> data* -> verify -> optional launch_dry_run
```

packet v0 不定义 USB framing、retry window 或 product launch policy。`ByteTransportRuntime` 可以接收任意
chunk size，只在完整 frame 可用时 dispatch 到 `PacketRuntime`。`reset()` 只清 byte buffer；新 stream
从 sequence 0 开始时使用 `reset_session()`。

## Command surface

- `dev status`
- `dev begin <size> [crc_hex]`
- `dev fill <hex_byte> <count>`
- `dev verify`
- `dev launch dry-run`
- `dev abort`

平台 monitor 可以增加 frontend/store/app 命令，但不得复制 begin/data/verify state machine。

## Store 与 App handoff

```text
packetstream -> launch_ready bytes
-> optional AppStoreWritableMedia install/readback
-> received image or AppStoreReader named image
-> AppImage -> staged AppImageSource -> AppRuntime
```

`store_install_received_image()` 复用 App ABI Store installer；`store_stage_named_app_image()` 复用 Store
reader/staging。QSPI、eMMC、memory NOR 或其它 media 不改变 Store v1、AppImage 或 CharmAppApi。

ELF/ModuleX probe、relocation、entry ABI 与 execute region 由 App ABI loader/runtime 和平台 backend
负责，不属于 transport/session contract。

## Host 工具

[`dev-loader-packet-stream`](../system/dev_loader_packet_stream_tool/main.cpp) 将 payload 编码为 packet v0；
[`dev-loader-packet-console`](../system/dev_loader_packet_console_tool/main.cpp) 再将 packetstream 转成
`dev packet ingest <hex>` 文本命令。

CLI 参数与默认值由各自 `main.cpp` 定义；工具不扩展 packet/session 语义。

## 验证

Host fixture 位于 [`Examples/system`](../system/README.md) 的 `dev_loader_*` 目录，覆盖 session、packet、
byte transport、console adapter、Store handoff 与 received image。target、负例和断言以各目录
CMake/source 为准；Host smoke 不证明真实 USB/UART、Flash、SDRAM、cache 或目标代码执行。
