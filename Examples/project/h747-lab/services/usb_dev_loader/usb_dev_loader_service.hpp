#pragma once

#include "usb_dev_loader.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace h747::usb_dev_loader {

using Status = h747_usb_dev_loader_status_t;

inline void init() noexcept {
    h747_usb_dev_loader_init();
}

inline void poll_irq() noexcept {
    h747_usb_dev_loader_poll_irq();
}

inline void stop() noexcept {
    h747_usb_dev_loader_stop();
}

inline std::size_t read(std::span<std::uint8_t> output) noexcept {
    return h747_usb_dev_loader_read(output.data(), output.size());
}

inline Status status() noexcept {
    return h747_usb_dev_loader_status();
}

} // namespace h747::usb_dev_loader
