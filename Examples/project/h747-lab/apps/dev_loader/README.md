# H747 Dev Loader

## 文档状态

- `status`: `supporting`
- `scope`: H747 resident development runtime 的平台绑定、资源与验证入口
- `source`: [`dev_loader.cpp`](dev_loader.cpp)、[`app.cmake`](app.cmake)

`dev_loader` 是开发期常驻 runtime，不是产品 bootloader。所有入口汇入同一主链：

```text
UART/USB packetstream or App Store
-> AppImage
-> staged AppImageSource
-> ELF/ModuleX loader
-> AppRuntime
-> CharmAppApi
```

receive/session 原型位于 [`Examples/dev_loader`](../../../../dev_loader/README.md)，App/Store/loader
语义位于 [`Examples/app_abi`](../../../../app_abi/README.md)。本 monitor 只绑定 H747 transport、arena、
media 和 capability backend，不新增 packet、Store、App entry 或 capability table。

## 资源

| 用途 | 区域 | 容量 | 约束 |
|---|---:|---:|---|
| received packetstream | SDRAM2 `0xD0000000` | 256 KiB | `sdram2_receive_buffer` |
| staged image cache | SDRAM2 `0xD0040000` | 128 KiB | `sdram2_stage_cache` |
| receive/stage fallback | D1 `0x24040000` | 128 KiB | SDRAM2 smoke 失败时使用 |
| ELF probe | private RAM scratch | 64 KiB | 只 probe，不执行 |
| App ELF execute | D1 `0x24070000..0x24080000` | 64 KiB | 16-byte alignment |

ELF execute base 必须与 [`app_elf.ld`](../../../../app_abi/elf_samples/app_elf.ld) 一致。SDRAM 只承担
receive/stage；本 target 不从 SDRAM 执行 ELF text。arena 选择和 SDRAM2 smoke 结果由 `dev status`
与 `dev app status` 输出。

## Monitor

| 范围 | 命令 |
|---|---|
| receive | `dev status`、`dev begin`、`dev fill`、`dev verify`、`dev launch dry-run`、`dev abort` |
| packet | `dev packet status|ingest|reset|reset-session` |
| raw UART | `dev raw begin|status|abort` |
| USB CDC | `dev usb begin|status|abort` |
| Store | `dev store status`、`install`、`list`、`stage`，media 为 `qspi|emmc` |
| App | `dev app stage|probe|prepare|run|status`，source 为 received、`qspi:<name>` 或 `emmc:<name>` |

`dev packet ingest` 接受连续或空格分隔的 hex pair。console line buffer 为 128 bytes，单条命令约束在
48 decoded bytes 左右。`packet reset` 只清 partial bytes；失败后从 sequence 0 重放时使用
`packet reset-session`。

## Transport

Packet v0 保持 `begin -> data* -> verify -> launch_dry_run`。Raw UART 与 USB CDC 只是
`ByteTransportRuntime -> PacketRuntime -> BinaryReceiveRuntime` 的 byte source。

- Raw UART 在 `launch_ready`、packet/transport error 或 abort 前保持 binary-transparent，不解释文本
  escape。UART RX 优先 circular DMA，启动失败才回退到 RDR polling；overrun/fallback counter 增长表示
  frontend loss。
- USB CDC 使用 UART 作为 control console，并独占 CDC packetstream。frontend 参数为
  `packet_buffer=1024`、`max_payload=256`、`read_chunk=512`、`drain_limit=4096`。
- USB mode 以 `launch_ready`、`packet_error`、`transport_error` 或 `abort` 结束。`dev usb abort` 停止 USB
  device，但不恢复此前 USB function；后续 App 自行初始化所需 USB 状态。
- Host 安全默认是 256-byte write 和 1 ms inter-chunk delay。更大 chunk 或 zero delay 只用于 throughput
  test；失败必须保留 exit、packet/transport 和 dropped/overflow counters。

## Store 与 Image

QSPI 与 eMMC 共享 Store v1 byte-range layout。QSPI 从 offset 0 安装；eMMC 使用 exposed block window
尾部固定 16 MiB raw development slot，并对非 512-byte aligned range 执行 read-modify-write。它不是
filesystem、产品 partition、slot manager 或 manifest service。

安装固定走 `received_image_read -> app_store_install_image` 并 readback verify；list/stage/run 固定走
`AppStoreReader -> app_store_stage_named_image -> staged AppImageSource`。monitor 不复制 erase/write/
verify 或 lookup 语义。

`AppStoreEntry.flags & 0xF` 表示 image format：`0=ELF`、`1=ModuleX`。两者进入同一 AppRuntime 与
`charm_app_main(api, argc, argv)`；ModuleX 的 relocation、BSS/XIP 边界由
[`Examples/app_abi`](../../../../app_abi/README.md) 定义，本 monitor 不复制。

## App 执行

- `stage` 从 verified payload 或 Store 创建 `AppImage`。
- `probe` 验证 ELF 并 dry-load metadata。
- `prepare` 继续验证 App ABI 与 argv，但不调用 entry。
- `run` 装载到固定 D1 region、准备 cache，并由 `AppRuntime` 调用 `charm_app_main`。

`dev app status` 的 run record 包含 source、format、name、command、argc、load/entry、span、segments、
run stage/code、exit 和 capability counters。ELF capacity 诊断包含 region base/size、linked base、needed、
free、fits 与 probe code。超过 64 KiB region 的 ELF 必须在 probe/load 返回
`load_buffer_too_small`，不得跳转、partial start 或隐式切换到 SDRAM execute。

当前 capability backend 以诊断为目的：console/time 是真实实现；display/input 使用内存计数；storage
与 AFE 不支持。Player UI 与真实 display/touch policy 不属于本 monitor。

## 验证入口

从 `Examples/project/h747-lab` 使用以下现有入口，不复制 token 判断：

| 目标 | 入口 |
|---|---|
| off-board artifact/inspect/host/H747 build evidence | [`capture-resident-platform-evidence-bundle.ps1`](../../tools/capture-resident-platform-evidence-bundle.ps1) |
| 增加 QEMU ELF evidence | 上述脚本加 `-QemuElf -SkipH747Build` |
| H747 build | preset `build-h747-lab-dev-loader-debug`，复用同一 `cmake-build-*` |
| received/QSPI/eMMC ELF | [`capture-dev-loader-usb-cdc-elf-platform-smoke.ps1`](../../tools/capture-dev-loader-usb-cdc-elf-platform-smoke.ps1) |
| QSPI/eMMC mixed ELF/ModuleX | [`capture-dev-loader-usb-cdc-appstore-platform-matrix-smoke.ps1`](../../tools/capture-dev-loader-usb-cdc-appstore-platform-matrix-smoke.ps1) |
| 已安装 Store 复测 | [`capture-dev-loader-installed-store-matrix-smoke.ps1`](../../tools/capture-dev-loader-installed-store-matrix-smoke.ps1) |

Board matrix 只能显式启用；默认 evidence bundle 不烧录、不打开串口或 USB。Focused raw/USB/throughput
helper 仍位于 `tools/`，不得另写一套 token parser。

## 保留的板级发现

以下是历史实板调试结论，不代表当前 commit 已复测；当前状态必须重新运行上述 evidence：

- raw UART 与 USB CDC packetstream 曾在安全默认下达到 `launch_ready`、CRC 相等且无 drop/overflow；
- USB host chunk 256/512/1024 bytes 曾在 zero delay 下通过；
- SDRAM2 receive/stage 的早期 fallback 原因是 memory smoke 前未应用 `storage_stage_a`，不是局部 bank
  故障；
- mixed Store 曾在 QSPI/eMMC 完成 install/list/run；
- received、QSPI 与 eMMC ELF 曾进入同一 AppRuntime 并 exit 0；
- received/stored `modulex_hello_app` 曾完成 relocation 并 exit 0；
- eMMC wide mode 使用 `bus=8`、`clkcr=0x00008006`、`sta=0x00000000`；保留 1-bit/div16 fallback。

## 故障分类

| 观察 | 归属 |
|---|---|
| CDC 与 comparison firmware 均 `setup=0 reset=0 connect=0` | physical host/device path，常见原因是 device cable 缺失 |
| `transport_error/buffer_too_small` 或 drop/overflow 增长 | USB byte-source pacing/buffering |
| UART overrun/fallback 增长 | UART RX frontend |
| `launch_ready` 后 Store 失败 | install/media validation |
| `lookup/load/abi/argv/start/exit` | 对应 AppRuntime stage |
| pyOCD AP warning 但 process exit 0 | warning 本身不证明 firmware 失败 |

## 不提供

- 产品 boot policy、signature、rollback、slot selection 或 crash recovery；
- raw jump、第二套 App ABI 或第二套 Store/packet format；
- composite USB 或自动恢复旧 USB state；
- SDRAM ELF execute、MPU sandbox、process isolation 或 scheduler contract；
- Player UI、display、touch 或产品 storage policy。

H747 动态 image 角色见
[`h747_lab_dynamic_boundary_roadmap.md`](../../docs/h747_lab_dynamic_boundary_roadmap.md)。
