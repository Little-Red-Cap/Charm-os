#pragma once

#include <cstdint>

namespace h747::usb_msc {

struct UsbMscState {
    std::uint8_t initialized;
    std::uint8_t started;
    std::uint8_t ready;
    std::uint8_t write_enabled;
    std::uint8_t last_setup_valid;
    std::uint8_t pcd_ready;
    std::uint8_t device_init_called;
    std::uint8_t vbus_detector_enabled;
    std::uint32_t block_count;
    std::uint32_t block_size;
    std::uint32_t partition_lba;
    std::uint32_t init_calls;
    std::uint32_t ready_calls;
    std::uint32_t capacity_calls;
    std::uint32_t read_calls;
    std::uint32_t write_calls;
    std::uint32_t last_error;
    std::uint32_t setup_count;
    std::uint32_t reset_count;
    std::uint32_t suspend_count;
    std::uint32_t resume_count;
    std::uint32_t connect_count;
    std::uint32_t disconnect_count;
    std::uint32_t out_ep0_hits;
    std::uint32_t in_ep0_hits;
    std::uint32_t gusbcfg;
    std::uint32_t gahbcfg;
    std::uint32_t gintsts;
    std::uint32_t gintmsk;
    std::uint32_t dctl;
    std::uint32_t dsts;
    std::uint32_t gotgctl;
    std::uint32_t gccfg;
    std::uint32_t diepctl0;
    std::uint32_t diepint0;
    std::uint32_t doepctl0;
    std::uint32_t doepint0;
    std::int32_t pcd_init_status;
    std::int32_t usbd_init_status;
    std::int32_t register_class_status;
    std::int32_t register_storage_status;
    std::int32_t usbd_start_status;
    std::uint8_t last_setup[8];
    std::uint8_t dev_desc_len;
    std::uint8_t cfg_desc_len;
    std::uint8_t dev_desc_prefix_len;
    std::uint8_t cfg_desc_prefix_len;
    std::uint8_t usbd_init_ok;
    std::uint8_t class_ok;
    std::uint8_t storage_ok;
    std::uint8_t usbd_start_ok;
    std::uint8_t dev_desc_prefix[18];
    std::uint8_t cfg_desc_prefix[32];
};

void init() noexcept;
void poll() noexcept;
UsbMscState state() noexcept;

} // namespace h747::usb_msc
