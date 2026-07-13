# USB MSC BlockDevice Demo

## 文档状态

- `status`: `supporting`
- `scope`: Host SCSI/BOT exchange over a BlockDevice fixture
- `source`: [`main.cpp`](main.cpp)、[`CMakeLists.txt`](CMakeLists.txt)

该 demo 将 `block.sd0` 接入 MSC BOT，并运行 Host-side SCSI exchange。它验证 adapter/protocol path，
不证明 USB enumeration、真实 controller 或 media 健康。

## Build 与运行

```powershell
$build = 'cmake-build-usb-msc-block-demo'
$image = 'path/to/disk.img'
cmake -S Examples/usb/usb_msc_block_demo -B $build -G Ninja
cmake --build $build -- -j1
& "$build/usb-msc-block-demo.exe" $image
```

重复验证应复用同一 `$build`。

## Evidence

Executable 支持 export-only graph/DOT/JSON 与 runtime-observe sidecar；参数和默认路径以 `main.cpp`
为准。sidecar 只记录 final-state `block.sd0` publication，不提供 attach/detach history，也不证明 USB
bus 或 media 可用。

仓库聚合导出通过
[`export_materialized_graph.ps1`](../../../scripts/export_materialized_graph.ps1) 的
`-Case usb-msc-block-demo` 进入，artifact report 只消费该 fixture 已声明的事实。

## Board Boundary

Device-mode board integration 必须提供 `usb::driver::DcdOps`、controller context 与
`DcdDeviceAdapter`，并把真实 IRQ 转发到 adapter callback。Host demo 不能替代板级证据。协议与介质
边界分别见 [`USB`](../../../docs/usb/README.md) 和 [`storage`](../../../docs/storage/README.md)。
