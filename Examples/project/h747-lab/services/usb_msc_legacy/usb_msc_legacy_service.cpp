#include "usb_msc_legacy_service.hpp"

#include "storage.h"
#include "stm32h7xx_hal.h"
#include "usb_device.h"
#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_msc_scsi.h"
#include "usbd_storage_if.h"

#include <cstring>

extern "C" PCD_HandleTypeDef hpcd_USB_OTG_FS;
extern "C" USBD_HandleTypeDef hUsbDeviceFS;

namespace h747::usb_msc_legacy {
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

std::uint32_t nvic_word_for_irq(const IRQn_Type irq) noexcept {
    const auto index = static_cast<std::uint32_t>(irq) >> 5U;
    return index;
}

std::uint32_t nvic_mask_for_irq(const IRQn_Type irq) noexcept {
    return 1UL << (static_cast<std::uint32_t>(irq) & 0x1FU);
}

std::uint32_t nvic_enabled(const IRQn_Type irq) noexcept {
    return ((NVIC->ISER[nvic_word_for_irq(irq)] & nvic_mask_for_irq(irq)) != 0U) ? 1U : 0U;
}

std::uint32_t nvic_pending(const IRQn_Type irq) noexcept {
    return ((NVIC->ISPR[nvic_word_for_irq(irq)] & nvic_mask_for_irq(irq)) != 0U) ? 1U : 0U;
}

std::uint32_t nvic_active(const IRQn_Type irq) noexcept {
    return ((NVIC->IABR[nvic_word_for_irq(irq)] & nvic_mask_for_irq(irq)) != 0U) ? 1U : 0U;
}

std::uint32_t known_scsi_command_count() noexcept {
    return usb_legacy_scsi_opcode_count(SCSI_TEST_UNIT_READY)
         + usb_legacy_scsi_opcode_count(SCSI_REQUEST_SENSE)
         + usb_legacy_scsi_opcode_count(SCSI_INQUIRY)
         + usb_legacy_scsi_opcode_count(SCSI_START_STOP_UNIT)
         + usb_legacy_scsi_opcode_count(SCSI_ALLOW_MEDIUM_REMOVAL)
         + usb_legacy_scsi_opcode_count(SCSI_MODE_SENSE6)
         + usb_legacy_scsi_opcode_count(SCSI_MODE_SENSE10)
         + usb_legacy_scsi_opcode_count(SCSI_READ_FORMAT_CAPACITIES)
         + usb_legacy_scsi_opcode_count(SCSI_READ_CAPACITY10)
         + usb_legacy_scsi_opcode_count(SCSI_READ_CAPACITY16)
         + usb_legacy_scsi_opcode_count(SCSI_READ10)
         + usb_legacy_scsi_opcode_count(SCSI_READ12)
         + usb_legacy_scsi_opcode_count(SCSI_WRITE10)
         + usb_legacy_scsi_opcode_count(SCSI_WRITE12)
         + usb_legacy_scsi_opcode_count(SCSI_VERIFY10);
}

} // namespace

void init() noexcept {
    MX_USB_DEVICE_LEGACY_STORAGE_Init();
}

void poll() noexcept {
    // USB is driven by OTG_FS_IRQHandler. Poll remains as an app-facing seam
    // for future deferred work, but must not re-enter the HAL IRQ handler.
}

void detach() noexcept {
    usb_legacy_device_detach();
}

std::int32_t attach() noexcept {
    return usb_legacy_device_attach();
}

void set_write_enabled(bool enabled) noexcept {
    usb_legacy_msc_set_write_enabled(enabled ? 1U : 0U);
}

State state() noexcept {
    const auto storage = h747_storage_state();
    const auto device = usb_legacy_device_status();
    const auto* usb = USB_OTG_FS;
    const auto* usb_dev = reinterpret_cast<USB_OTG_DeviceTypeDef*>(USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE);
    const auto* in0 = reinterpret_cast<USB_OTG_INEndpointTypeDef*>(USB_OTG_FS_PERIPH_BASE + USB_OTG_IN_ENDPOINT_BASE);
    const auto* out0 = reinterpret_cast<USB_OTG_OUTEndpointTypeDef*>(USB_OTG_FS_PERIPH_BASE + USB_OTG_OUT_ENDPOINT_BASE);

    State result{
        .initialized = storage.initialized,
        .started = device.usbd_start_ok,
        .ready = storage.block_device_ready,
        .write_enabled = usb_legacy_msc_write_enabled(),
        .pcd_ready = usb_legacy_pcd_ready(),
        .device_init_called = device.init_called,
        .last_setup_valid = usb_legacy_last_setup_valid(),
        .vbus_detector_enabled = device.vbus_detector_enabled,
        .soft_disconnect_before_start = device.soft_disconnect_before_start,
        .soft_disconnect_after_start = device.soft_disconnect_after_start,
        .gpio_dm = static_cast<std::uint8_t>((GPIOA->IDR >> 11U) & 0x1U),
        .gpio_dp = static_cast<std::uint8_t>((GPIOA->IDR >> 12U) & 0x1U),
        .block_count = h747_storage_raw_block_count(),
        .block_size = h747_storage_block_size(),
        .partition_lba = h747_storage_partition_lba(),
        .init_calls = usb_legacy_msc_init_calls(),
        .ready_calls = usb_legacy_msc_ready_calls(),
        .capacity_calls = usb_legacy_msc_capacity_calls(),
        .read_calls = usb_legacy_msc_read_calls(),
        .write_calls = usb_legacy_msc_write_calls(),
        .last_error = usb_legacy_msc_last_error(),
        .cache_hits = usb_legacy_msc_cache_hits(),
        .cache_misses = usb_legacy_msc_cache_misses(),
        .last_read_lba = usb_legacy_msc_last_read_lba(),
        .last_read_len = usb_legacy_msc_last_read_len(),
        .last_write_lba = usb_legacy_msc_last_write_lba(),
        .last_write_len = usb_legacy_msc_last_write_len(),
        .read_blocks = usb_legacy_msc_read_blocks(),
        .write_blocks = usb_legacy_msc_write_blocks(),
        .max_read_len = usb_legacy_msc_max_read_len(),
        .max_write_len = usb_legacy_msc_max_write_len(),
        .cache_stores = usb_legacy_msc_cache_stores(),
        .cache_invalidations = usb_legacy_msc_cache_invalidations(),
        .packet_bytes = usb_legacy_msc_packet_bytes(),
        .read_ahead_blocks = usb_legacy_msc_read_ahead_blocks(),
        .cache_window_lba = usb_legacy_msc_cache_window_lba(),
        .cache_window_blocks = usb_legacy_msc_cache_window_blocks(),
        .setup_count = usb_legacy_setup_count(),
        .reset_count = usb_legacy_reset_count(),
        .suspend_count = usb_legacy_suspend_count(),
        .resume_count = usb_legacy_resume_count(),
        .connect_count = usb_legacy_connect_count(),
        .disconnect_count = usb_legacy_disconnect_count(),
        .out_ep0_hits = usb_legacy_out_ep_hits(0U),
        .in_ep0_hits = usb_legacy_in_ep_hits(0U),
        .out_ep1_hits = usb_legacy_out_ep_hits(1U),
        .in_ep1_hits = usb_legacy_in_ep_hits(1U),
        .stall_ep1_out_hits = usb_legacy_stall_ep_hits(MSC_EPOUT_ADDR),
        .stall_ep1_in_hits = usb_legacy_stall_ep_hits(MSC_EPIN_ADDR),
        .clear_stall_ep1_out_hits = usb_legacy_clear_stall_ep_hits(MSC_EPOUT_ADDR),
        .clear_stall_ep1_in_hits = usb_legacy_clear_stall_ep_hits(MSC_EPIN_ADDR),
        .open_ep_count = usb_legacy_open_ep_count(),
        .close_ep_count = usb_legacy_close_ep_count(),
        .set_address_count = usb_legacy_set_address_count(),
        .setup_std_get_status = usb_legacy_setup_std_get_status_count(),
        .setup_std_clear_feature = usb_legacy_setup_std_clear_feature_count(),
        .setup_std_set_feature = usb_legacy_setup_std_set_feature_count(),
        .setup_std_set_address = usb_legacy_setup_std_set_address_count(),
        .setup_std_get_descriptor_device = usb_legacy_setup_std_get_descriptor_device_count(),
        .setup_std_get_descriptor_config = usb_legacy_setup_std_get_descriptor_config_count(),
        .setup_std_get_descriptor_string = usb_legacy_setup_std_get_descriptor_string_count(),
        .setup_std_get_descriptor_qualifier = usb_legacy_setup_std_get_descriptor_qualifier_count(),
        .setup_std_get_descriptor_other = usb_legacy_setup_std_get_descriptor_other_count(),
        .setup_std_get_configuration = usb_legacy_setup_std_get_configuration_count(),
        .setup_std_set_configuration = usb_legacy_setup_std_set_configuration_count(),
        .setup_std_get_interface = usb_legacy_setup_std_get_interface_count(),
        .setup_std_set_interface = usb_legacy_setup_std_set_interface_count(),
        .setup_std_other = usb_legacy_setup_std_other_count(),
        .setup_class_get_max_lun = usb_legacy_setup_class_get_max_lun_count(),
        .setup_class_bot_reset = usb_legacy_setup_class_bot_reset_count(),
        .setup_class_other = usb_legacy_setup_class_other_count(),
        .setup_vendor = usb_legacy_setup_vendor_count(),
        .setup_type_other = usb_legacy_setup_type_other_count(),
        .scsi_cbw_count = usb_legacy_scsi_cbw_count(),
        .scsi_bad_cbw_count = usb_legacy_scsi_bad_cbw_count(),
        .scsi_test_unit_ready_count = usb_legacy_scsi_opcode_count(SCSI_TEST_UNIT_READY),
        .scsi_request_sense_count = usb_legacy_scsi_opcode_count(SCSI_REQUEST_SENSE),
        .scsi_inquiry_count = usb_legacy_scsi_opcode_count(SCSI_INQUIRY),
        .scsi_start_stop_count = usb_legacy_scsi_opcode_count(SCSI_START_STOP_UNIT),
        .scsi_allow_removal_count = usb_legacy_scsi_opcode_count(SCSI_ALLOW_MEDIUM_REMOVAL),
        .scsi_mode_sense6_count = usb_legacy_scsi_opcode_count(SCSI_MODE_SENSE6),
        .scsi_mode_sense10_count = usb_legacy_scsi_opcode_count(SCSI_MODE_SENSE10),
        .scsi_read_format_capacity_count = usb_legacy_scsi_opcode_count(SCSI_READ_FORMAT_CAPACITIES),
        .scsi_read_capacity10_count = usb_legacy_scsi_opcode_count(SCSI_READ_CAPACITY10),
        .scsi_read_capacity16_count = usb_legacy_scsi_opcode_count(SCSI_READ_CAPACITY16),
        .scsi_read10_count = usb_legacy_scsi_opcode_count(SCSI_READ10),
        .scsi_read12_count = usb_legacy_scsi_opcode_count(SCSI_READ12),
        .scsi_write10_count = usb_legacy_scsi_opcode_count(SCSI_WRITE10),
        .scsi_write12_count = usb_legacy_scsi_opcode_count(SCSI_WRITE12),
        .scsi_verify10_count = usb_legacy_scsi_opcode_count(SCSI_VERIFY10),
        .scsi_other_count = usb_legacy_scsi_cbw_count() - known_scsi_command_count(),
        .scsi_last_tag = usb_legacy_scsi_last_tag(),
        .scsi_last_data_len = usb_legacy_scsi_last_data_len(),
        .scsi_last_lba = usb_legacy_scsi_last_lba(),
        .scsi_last_blocks = usb_legacy_scsi_last_blocks(),
        .scsi_last_opcode = usb_legacy_scsi_last_opcode(),
        .scsi_last_lun = usb_legacy_scsi_last_lun(),
        .scsi_last_cb_len = usb_legacy_scsi_last_cb_len(),
        .scsi_last_flags = usb_legacy_scsi_last_flags(),
        .scsi_last_valid = usb_legacy_scsi_last_valid(),
        .dev_config = hUsbDeviceFS.dev_config,
        .dev_default_config = hUsbDeviceFS.dev_default_config,
        .dev_config_status = hUsbDeviceFS.dev_config_status,
        .ep0_state = hUsbDeviceFS.ep0_state,
        .ep0_data_len = hUsbDeviceFS.ep0_data_len,
        .class_id = hUsbDeviceFS.classId,
        .num_classes = hUsbDeviceFS.NumClasses,
        .dev_state = hUsbDeviceFS.dev_state,
        .dev_old_state = hUsbDeviceFS.dev_old_state,
        .dev_address = hUsbDeviceFS.dev_address,
        .dev_connection_status = hUsbDeviceFS.dev_connection_status,
        .dev_speed = static_cast<std::uint8_t>(hUsbDeviceFS.dev_speed),
        .conf_idx = hUsbDeviceFS.ConfIdx,
        .ep1_out_used = static_cast<std::uint8_t>(hUsbDeviceFS.ep_out[MSC_EPOUT_ADDR & 0x0FU].is_used),
        .ep1_in_used = static_cast<std::uint8_t>(hUsbDeviceFS.ep_in[MSC_EPIN_ADDR & 0x0FU].is_used),
        .gusbcfg = static_cast<std::uint32_t>(usb->GUSBCFG),
        .gahbcfg = static_cast<std::uint32_t>(usb->GAHBCFG),
        .gintsts = static_cast<std::uint32_t>(usb->GINTSTS),
        .gintmsk = static_cast<std::uint32_t>(usb->GINTMSK),
        .gint_masked = static_cast<std::uint32_t>(usb->GINTSTS & usb->GINTMSK),
        .gotgint = static_cast<std::uint32_t>(usb->GOTGINT),
        .dctl = static_cast<std::uint32_t>(usb_dev->DCTL),
        .dsts = static_cast<std::uint32_t>(usb_dev->DSTS),
        .gotgctl = static_cast<std::uint32_t>(usb->GOTGCTL),
        .gccfg = static_cast<std::uint32_t>(usb->GCCFG),
        .daint = static_cast<std::uint32_t>(usb_dev->DAINT),
        .daintmsk = static_cast<std::uint32_t>(usb_dev->DAINTMSK),
        .doepmsk = static_cast<std::uint32_t>(usb_dev->DOEPMSK),
        .diepmsk = static_cast<std::uint32_t>(usb_dev->DIEPMSK),
        .diepctl0 = static_cast<std::uint32_t>(in0->DIEPCTL),
        .diepint0 = static_cast<std::uint32_t>(in0->DIEPINT),
        .doepctl0 = static_cast<std::uint32_t>(out0->DOEPCTL),
        .doepint0 = static_cast<std::uint32_t>(out0->DOEPINT),
        .gpioa_moder = static_cast<std::uint32_t>(GPIOA->MODER),
        .gpioa_pupdr = static_cast<std::uint32_t>(GPIOA->PUPDR),
        .gpioa_idr = static_cast<std::uint32_t>(GPIOA->IDR),
        .gpioa_afrh = static_cast<std::uint32_t>(GPIOA->AFR[1]),
        .rcc_usbsel = static_cast<std::uint32_t>(RCC->D2CCIP2R),
        .rcc_ahb1enr = static_cast<std::uint32_t>(RCC->AHB1ENR),
        .nvic_enable_otg_fs = nvic_enabled(OTG_FS_IRQn),
        .nvic_pending_otg_fs = nvic_pending(OTG_FS_IRQn),
        .nvic_active_otg_fs = nvic_active(OTG_FS_IRQn),
        .pcd_init_status = usb_legacy_pcd_init_status(),
        .usbd_init_status = device.usbd_init_status,
        .register_class_status = device.register_class_status,
        .register_storage_status = device.register_storage_status,
        .usbd_start_status = device.usbd_start_status,
        .last_setup = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
        .setup_history_count = 0U,
        .setup_history = {{0U}},
        .dev_desc_len = 0U,
        .cfg_desc_len = 0U,
        .dev_desc_prefix_len = 0U,
        .cfg_desc_prefix_len = 0U,
        .dev_desc_prefix = {0U},
        .cfg_desc_prefix = {0U},
    };

    usb_legacy_copy_last_setup(result.last_setup);
    result.setup_history_count = usb_legacy_setup_history_count();
    for (std::uint8_t slot = 0U; slot < result.setup_history_count && slot < 4U; ++slot) {
        usb_legacy_copy_setup_history(slot, result.setup_history[slot]);
    }

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

} // namespace h747::usb_msc_legacy
