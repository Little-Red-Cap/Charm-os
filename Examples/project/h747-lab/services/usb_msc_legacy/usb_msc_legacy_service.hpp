#pragma once

#include <cstdint>

namespace h747::usb_msc_legacy {

struct State {
    std::uint8_t initialized;
    std::uint8_t started;
    std::uint8_t ready;
    std::uint8_t write_enabled;
    std::uint8_t pcd_ready;
    std::uint8_t device_init_called;
    std::uint8_t last_setup_valid;
    std::uint8_t vbus_detector_enabled;
    std::uint8_t soft_disconnect_before_start;
    std::uint8_t soft_disconnect_after_start;
    std::uint8_t gpio_dm;
    std::uint8_t gpio_dp;
    std::uint32_t block_count;
    std::uint32_t block_size;
    std::uint32_t partition_lba;
    std::uint32_t init_calls;
    std::uint32_t ready_calls;
    std::uint32_t capacity_calls;
    std::uint32_t read_calls;
    std::uint32_t write_calls;
    std::uint32_t last_error;
    std::uint32_t cache_hits;
    std::uint32_t cache_misses;
    std::uint32_t last_read_lba;
    std::uint32_t last_read_len;
    std::uint32_t last_write_lba;
    std::uint32_t last_write_len;
    std::uint32_t read_blocks;
    std::uint32_t write_blocks;
    std::uint32_t max_read_len;
    std::uint32_t max_write_len;
    std::uint32_t cache_stores;
    std::uint32_t cache_invalidations;
    std::uint32_t packet_bytes;
    std::uint32_t read_ahead_blocks;
    std::uint32_t cache_window_lba;
    std::uint32_t cache_window_blocks;
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
    std::uint32_t gint_masked;
    std::uint32_t gotgint;
    std::uint32_t dctl;
    std::uint32_t dsts;
    std::uint32_t gotgctl;
    std::uint32_t gccfg;
    std::uint32_t daint;
    std::uint32_t daintmsk;
    std::uint32_t doepmsk;
    std::uint32_t diepmsk;
    std::uint32_t diepctl0;
    std::uint32_t diepint0;
    std::uint32_t doepctl0;
    std::uint32_t doepint0;
    std::uint32_t gpioa_moder;
    std::uint32_t gpioa_pupdr;
    std::uint32_t gpioa_idr;
    std::uint32_t gpioa_afrh;
    std::uint32_t rcc_usbsel;
    std::uint32_t rcc_ahb1enr;
    std::uint32_t nvic_enable_otg_fs;
    std::uint32_t nvic_pending_otg_fs;
    std::uint32_t nvic_active_otg_fs;
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
    std::uint8_t dev_desc_prefix[18];
    std::uint8_t cfg_desc_prefix[32];
};

void init() noexcept;
void poll() noexcept;
void detach() noexcept;
std::int32_t attach() noexcept;
void set_write_enabled(bool enabled) noexcept;
State state() noexcept;

} // namespace h747::usb_msc_legacy
