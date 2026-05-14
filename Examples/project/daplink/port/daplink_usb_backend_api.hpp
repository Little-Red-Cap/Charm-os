#ifndef DAPLINK_USB_BACKEND_API_HPP
#define DAPLINK_USB_BACKEND_API_HPP

#include "daplink_port_api.hpp"
#include "port/daplink_port_usb_config.hpp"

#include <cstdint>

namespace daplink::usb_backend {
    using PcdHandle = daplink::port::UsbPcdHandle;
    using EndpointType = daplink::port::UsbEndpointType;
    using Layout = daplink::port::usb_config::Layout;

    inline auto start(PcdHandle& hpcd) noexcept -> bool {
        return daplink::port::usb_start(hpcd);
    }

    inline auto config_single_buffer(PcdHandle& hpcd,
                                     const std::uint8_t ep_addr,
                                     const std::uint16_t pma_addr) noexcept -> bool {
        return daplink::port::usb_pma_config_single_buffer(hpcd, ep_addr, pma_addr);
    }

    inline auto set_address(PcdHandle& hpcd, const std::uint8_t address) noexcept -> bool {
        return daplink::port::usb_set_address(hpcd, address);
    }

    inline auto ep_open(PcdHandle& hpcd,
                        const std::uint8_t ep_addr,
                        const std::uint16_t mps,
                        const EndpointType type) noexcept -> bool {
        return daplink::port::usb_ep_open(hpcd, ep_addr, mps, type);
    }

    inline auto ep_receive(PcdHandle& hpcd,
                           const std::uint8_t ep_addr,
                           std::uint8_t* data,
                           const std::uint16_t len) noexcept -> bool {
        return daplink::port::usb_ep_receive(hpcd, ep_addr, data, len);
    }

    inline auto ep_transmit(PcdHandle& hpcd,
                            const std::uint8_t ep_addr,
                            std::uint8_t* data,
                            const std::uint16_t len) noexcept -> bool {
        return daplink::port::usb_ep_transmit(hpcd, ep_addr, data, len);
    }

    inline auto ep_set_stall(PcdHandle& hpcd, const std::uint8_t ep_addr) noexcept -> bool {
        return daplink::port::usb_ep_set_stall(hpcd, ep_addr);
    }

    inline auto ep_rx_count(PcdHandle& hpcd, const std::uint8_t ep_addr) noexcept -> std::uint16_t {
        return daplink::port::usb_ep_rx_count(hpcd, ep_addr);
    }

    inline void copy_setup_packet(const PcdHandle& hpcd, std::uint8_t (&setup)[8]) noexcept {
        daplink::port::usb_copy_setup_packet(hpcd, setup);
    }
}

#endif
