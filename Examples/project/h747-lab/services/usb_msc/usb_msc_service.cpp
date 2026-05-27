#include "usb_msc_service.hpp"

#include "storage.h"
#include "stm32h7xx_hal.h"
#include "usb_device.h"
#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_storage_if.h"

#include <cstring>

extern "C" PCD_HandleTypeDef hpcd_USB_OTG_FS;
extern "C" USBD_HandleTypeDef hUsbDeviceFS;

namespace h747::usb_msc {

namespace {

template <std::size_t N>
void copy_prefix(std::uint8_t (&dst)[N], std::uint8_t& dst_len, const uint8_t* src, const uint16_t src_len) noexcept {
    if (src == nullptr || src_len == 0U) {
        dst_len = 0U;
        return;
    }
    const auto copy_len = static_cast<std::uint8_t>((src_len < N) ? src_len : N);
    std::memcpy(dst, src, copy_len);
    dst_len = copy_len;
}

} // namespace

void init() noexcept {
    MX_USB_DEVICE_STORAGE_Init();
}

void poll() noexcept {
    if (usb_otg_fs_pcd_ready() == 0U || hpcd_USB_OTG_FS.Instance == nullptr) {
        return;
    }
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}

UsbMscState state() noexcept {
    const auto storage = h747_storage_state();
    const auto device = usb_device_storage_status();
    const auto* usb = USB_OTG_FS;
    const auto* usb_dev = reinterpret_cast<USB_OTG_DeviceTypeDef*>(USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE);
    const auto* in0 = reinterpret_cast<USB_OTG_INEndpointTypeDef*>(USB_OTG_FS_PERIPH_BASE + USB_OTG_IN_ENDPOINT_BASE);
    const auto* out0 = reinterpret_cast<USB_OTG_OUTEndpointTypeDef*>(USB_OTG_FS_PERIPH_BASE + USB_OTG_OUT_ENDPOINT_BASE);

    UsbMscState result{
        .initialized = storage.initialized,
        .started = device.usbd_start_ok,
        .ready = storage.block_device_ready,
        .write_enabled = 1U,
        .last_setup_valid = usb_last_setup_valid(),
        .pcd_ready = usb_otg_fs_pcd_ready(),
        .device_init_called = device.init_called,
        .vbus_detector_enabled = device.vbus_detector_enabled,
        .block_count = h747_storage_block_count(),
        .block_size = h747_storage_block_size(),
        .partition_lba = h747_storage_partition_lba(),
        .init_calls = usb_msc_init_calls(),
        .ready_calls = usb_msc_ready_calls(),
        .capacity_calls = usb_msc_capacity_calls(),
        .read_calls = usb_msc_read_calls(),
        .write_calls = usb_msc_write_calls(),
        .last_error = usb_msc_last_error(),
        .setup_count = usb_setup_count(),
        .reset_count = usb_reset_count(),
        .suspend_count = usb_suspend_count(),
        .resume_count = usb_resume_count(),
        .connect_count = usb_connect_count(),
        .disconnect_count = usb_disconnect_count(),
        .out_ep0_hits = usb_out_ep_hits(0U),
        .in_ep0_hits = usb_in_ep_hits(0U),
        .gusbcfg = static_cast<std::uint32_t>(usb->GUSBCFG),
        .gahbcfg = static_cast<std::uint32_t>(usb->GAHBCFG),
        .gintsts = static_cast<std::uint32_t>(usb->GINTSTS),
        .gintmsk = static_cast<std::uint32_t>(usb->GINTMSK),
        .dctl = static_cast<std::uint32_t>(usb_dev->DCTL),
        .dsts = static_cast<std::uint32_t>(usb_dev->DSTS),
        .gotgctl = static_cast<std::uint32_t>(usb->GOTGCTL),
        .gccfg = static_cast<std::uint32_t>(usb->GCCFG),
        .diepctl0 = static_cast<std::uint32_t>(in0->DIEPCTL),
        .diepint0 = static_cast<std::uint32_t>(in0->DIEPINT),
        .doepctl0 = static_cast<std::uint32_t>(out0->DOEPCTL),
        .doepint0 = static_cast<std::uint32_t>(out0->DOEPINT),
        .pcd_init_status = usb_otg_fs_pcd_init_status(),
        .usbd_init_status = device.usbd_init_status,
        .register_class_status = device.register_class_status,
        .register_storage_status = device.register_storage_status,
        .usbd_start_status = device.usbd_start_status,
        .last_setup = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
        .dev_desc_len = 0U,
        .cfg_desc_len = 0U,
        .dev_desc_prefix_len = 0U,
        .cfg_desc_prefix_len = 0U,
        .usbd_init_ok = device.usbd_init_ok,
        .class_ok = device.class_ok,
        .storage_ok = device.storage_ok,
        .usbd_start_ok = device.usbd_start_ok,
        .dev_desc_prefix = {0U},
        .cfg_desc_prefix = {0U},
    };

    usb_copy_last_setup(result.last_setup);

    if ((hUsbDeviceFS.pDesc != nullptr) && (hUsbDeviceFS.pDesc->GetDeviceDescriptor != nullptr)) {
        uint16_t len = 0U;
        const uint8_t* desc = hUsbDeviceFS.pDesc->GetDeviceDescriptor(hUsbDeviceFS.dev_speed, &len);
        result.dev_desc_len = static_cast<std::uint8_t>((len > 255U) ? 255U : len);
        copy_prefix(result.dev_desc_prefix, result.dev_desc_prefix_len, desc, len);
    }

    if ((hUsbDeviceFS.pClass[0] != nullptr) && (hUsbDeviceFS.pClass[0]->GetFSConfigDescriptor != nullptr)) {
        uint16_t len = 0U;
        const uint8_t* desc = hUsbDeviceFS.pClass[0]->GetFSConfigDescriptor(&len);
        result.cfg_desc_len = static_cast<std::uint8_t>((len > 255U) ? 255U : len);
        copy_prefix(result.cfg_desc_prefix, result.cfg_desc_prefix_len, desc, len);
    }

    return result;
}

} // namespace h747::usb_msc
