# Dev Loader 原型

## 边界

本目录提供 board-free resident download 原型。UART、USB、console 或 host frontend 只负责提供
bytes/commands；receive、verify、packet sequence 与 App handoff 共享同一状态机。

它不是产品 bootloader，不定义 reset/jump policy、slot、签名、rollback 或 USB class ownership。
H747 平台行为见 [`h747-lab dev_loader`](../project/h747-lab/apps/dev_loader/README.md)；App image/runtime 见
[`Examples/app_abi/README.md`](../app_abi/README.md)。

## 组成

| 文件 | 职责 |
|---|---|
| [`charm_dev_loader.hpp`](charm_dev_loader.hpp) | `Session`、storage callback 与 receive state |
| [`charm_dev_loader_commands.hpp`](charm_dev_loader_commands.hpp) | 文本 command 到 Session transition |
| [`charm_dev_loader_packets.hpp`](charm_dev_loader_packets.hpp) | packet v0 到 binary receive path |
| [`charm_dev_loader_byte_transport.hpp`](charm_dev_loader_byte_transport.hpp) | 任意 byte chunk 的 buffering/frame dispatch |
| [`charm_dev_loader_packet_stream.hpp`](charm_dev_loader_packet_stream.hpp) | packetstream build/replay helper |
| [`charm_dev_loader_packet_console.hpp`](charm_dev_loader_packet_console.hpp) | packetstream 到 `dev packet ingest <hex>` adapter |
| [`charm_dev_loader_hex.hpp`](charm_dev_loader_hex.hpp) | console hex decode |
| [`charm_dev_loader_received_image.hpp`](charm_dev_loader_received_image.hpp) | `launch_ready` image 的 read-only handoff |
| [`charm_dev_loader_store_handoff.hpp`](charm_dev_loader_store_handoff.hpp) | received Store install 与 named App staging |

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

## 验证

Host fixture 位于 [`Examples/system`](../system/README.md) 的 `dev_loader_*` 目录，覆盖 session、packet、
byte transport、console adapter、Store handoff 与 received image。target、负例和断言以各目录
CMake/source 为准；Host smoke 不证明真实 USB/UART、Flash、SDRAM、cache 或目标代码执行。
