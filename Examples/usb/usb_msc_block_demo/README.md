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
