# USB MSC block.device demo

This demo wires `block.sd0` into the MSC BOT stack and runs a
host-side SCSI/BOT exchange to validate the wiring.

Build (Windows / Ninja):

```bash
cmake -S Examples/usb/usb_msc_block_demo -B Examples/usb/usb_msc_block_demo/build -G Ninja
cmake --build Examples/usb/usb_msc_block_demo/build
```

Run:

```bash
usb-msc-block-demo <disk.img|vhd>
```

Export-only observe path:

```bash
usb-msc-block-demo --export-only --dot out.dot --json out.json --image observe-usb-block.img
```

Repo helper script also registers this demo as an export case:

```powershell
./scripts/export_materialized_graph.ps1 -Case usb-msc-block-demo
./scripts/export_materialized_graph.ps1 -AllCases -OutputRoot out/materialized-graph-bundle
```

Device mode wiring (board/PCD integration):

- Provide `usb::driver::DcdOps`, `dcd_ctx`, and a `usb::driver::DcdDeviceAdapter`.
- Pass them to `UsbMscBlockInitChain` so MSC can attach and publish callbacks.
- The board-side IRQ handler should forward USB events to the adapter callbacks.
