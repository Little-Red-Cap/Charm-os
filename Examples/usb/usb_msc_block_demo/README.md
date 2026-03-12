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

Device mode wiring (board/PCD integration):

- Provide `usb::driver::DcdOps`, `dcd_ctx`, and a `usb::driver::DcdDeviceAdapter`.
- Pass them to `UsbMscBlockInitChain` so MSC can attach and publish callbacks.
- The board-side IRQ handler should forward USB events to the adapter callbacks.
