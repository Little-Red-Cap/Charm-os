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

Export runtime observe sidecar:

```bash
usb-msc-block-demo --runtime-observe runtime_observe.json --image observe-usb-block.img
```

这条 sidecar 当前反映的是 bringup 完成后的最终状态：

- `published_capabilities = ["block.sd0"]`
- `publish_state_summary.published = 1`
- `recent_transitions = []`

也就是说，它现在更适合作为“真实末态摘要”进入 bundle / artifact report，
而不是演示 attach/detach 历史。

Repo helper script also registers this demo as an export case:

```powershell
./scripts/export_materialized_graph.ps1 -Case usb-msc-block-demo
./scripts/export_materialized_graph.ps1 -AllCases -OutputRoot out/materialized-graph-bundle
```

当前 `usb-msc-block-demo` 也已经接入了正式的 `runtime_observe` 导出链。
通过 `export_case_manifest -> export_bundle -> artifact_report` 路径导出时，
bundle `index.json` 会包含同 case 的 `runtime_observe` sidecar。

Device mode wiring (board/PCD integration):

- Provide `usb::driver::DcdOps`, `dcd_ctx`, and a `usb::driver::DcdDeviceAdapter`.
- Pass them to `UsbMscBlockInitChain` so MSC can attach and publish callbacks.
- The board-side IRQ handler should forward USB events to the adapter callbacks.
