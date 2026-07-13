# USB MSC block.device demo

> status: `supporting`
>
> scope: Host SCSI/BOT exchange over a block-device fixture

The demo binds `block.sd0` to the MSC BOT stack and runs a Host-side SCSI/BOT
exchange. It checks the adapter and protocol path; it does not prove USB device
enumeration or a real controller backend.

## Build And Run

```bash
cmake -S Examples/usb/usb_msc_block_demo -B <cmake-build-dir> -G Ninja
cmake --build <cmake-build-dir> -- -j1
usb-msc-block-demo <disk.img|vhd>
```

## Evidence Export

```bash
usb-msc-block-demo --export-only --dot out.dot --json out.json --image observe-usb-block.img
usb-msc-block-demo --runtime-observe runtime_observe.json --image observe-usb-block.img
```

The runtime sidecar is a final-state observation: it reports the published
`block.sd0` capability and no attach/detach transition history. It is supporting
input for an artifact report, not proof that the media or USB bus is healthy.

The repository export helper registers the same case:

```powershell
./scripts/export_materialized_graph.ps1 -Case usb-msc-block-demo
./scripts/export_materialized_graph.ps1 -AllCases -OutputRoot out/materialized-graph-bundle
```

## Board Adapter Boundary

A device-mode integration must provide `usb::driver::DcdOps`, controller
context and `usb::driver::DcdDeviceAdapter`, then forward real IRQ events to the
adapter callbacks. Host demo success cannot substitute for that board evidence.

Current protocol and media boundaries are documented in
[`USB`](../../../docs/usb/README.md) and
[`storage`](../../../docs/storage/README.md).
