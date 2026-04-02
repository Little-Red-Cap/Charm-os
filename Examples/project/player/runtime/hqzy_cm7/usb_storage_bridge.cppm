export module player.runtime.hqzy_cm7.usb_storage_bridge;

import block.device;
import usb.msc_storage_bridge;

extern "C" void MX_USB_DEVICE_Init(void);

export void usb_system_init(block::Device* dev, bool read_only) noexcept {
    usb::msc::bridge::set_block_device(dev, read_only);
    MX_USB_DEVICE_Init();
}
