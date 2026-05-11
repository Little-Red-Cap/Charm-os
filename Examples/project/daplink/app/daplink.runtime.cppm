module;

#include <array>
#include <cstdint>

export module daplink.runtime;

import daplink.app_config;
import daplink.cmsis_dap;
import daplink.dap_init;
import daplink.dap_ops;
import daplink.dap_policy;
import daplink.dap_strategy;
import daplink.dap_transport;
import daplink.port_facade;
import daplink.ring_buffer;
import daplink.usb_minimal;

namespace daplink::runtime::detail {
    constexpr auto kUsbProfile = daplink::app_config::kConfig.usb.profile;
    constexpr bool kEnableCdc =
        (kUsbProfile == daplink::app_config::UsbProfile::cdc) ||
        (kUsbProfile == daplink::app_config::UsbProfile::composite);
    constexpr bool kEnableHid =
        (kUsbProfile == daplink::app_config::UsbProfile::hid) ||
        (kUsbProfile == daplink::app_config::UsbProfile::composite);

    static_assert(!kEnableHid || (daplink::usb_minimal::hid_packet_size == daplink::cmsis_dap::kPacketSize));

    constexpr std::size_t kUartBufSize = 256;
    using UartRing = daplink::ring_buffer::Buffer<kUartBufSize>;
    constexpr std::size_t kIoChunk = 64;
} // namespace daplink::runtime::detail

export namespace daplink::runtime {
    inline int run() {
        daplink::port_facade::init_or_fail();

        auto dap_state = daplink::dap_init::make_dap_state();
        const daplink::cmsis_dap::DeviceInfo info{
            daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbManufacturer),
            daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbProduct),
            daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbSerial),
            daplink::cmsis_dap::make_info_field(daplink::app_config::kCmsisDapProtocolVersion),
            daplink::cmsis_dap::make_info_field(daplink::app_config::kProductFwVersion)
        };

        detail::UartRing uart_tx{};
        detail::UartRing uart_rx{};
        daplink::dap_policy::UsbScheduler scheduler{};
        scheduler.cdc_policy = static_cast<daplink::dap_policy::CdcPolicy>(
            daplink::app_config::kConfig.cdc.policy);
        using DapOps = daplink::cmsis_dap::DefaultOps<daplink::port_facade::SwdBackend>;
        using DapPolicy = daplink::dap_strategy::DefaultTransferPolicy<daplink::cmsis_dap::State>;
        daplink::dap_transport::HidTransport<daplink::port_facade::SwdBackend, DapOps, DapPolicy> dap_transport{
            dap_state,
            info
        };
        auto usb_cdc = daplink::port_facade::usb_cdc_channel();
        auto uart = daplink::port_facade::cdc_uart_channel();
        auto last_line = daplink::dap_policy::UsbScheduler::to_line(daplink::usb_minimal::cdc_line());
        if constexpr (detail::kEnableCdc) {
            daplink::port_facade::apply_cdc_line(
                last_line.baud,
                last_line.stop_bits,
                last_line.parity,
                last_line.data_bits);
        }

        while (true) {
            scheduler.template tick<detail::kIoChunk>(
                dap_transport,
                [&]() noexcept {
                    dap_state = {};
                    dap_transport.reset_session();
                    if constexpr (detail::kEnableCdc) {
                        uart_tx = {};
                        uart_rx = {};
                    }
                },
                [&](const daplink::dap_policy::CdcLine& line) noexcept {
                    if constexpr (detail::kEnableCdc) {
                        daplink::port_facade::apply_cdc_line(
                            line.baud,
                            line.stop_bits,
                            line.parity,
                            line.data_bits);
                    } else {
                        (void)line;
                    }
                },
                usb_cdc,
                uart,
                uart_tx,
                uart_rx,
                last_line,
                detail::kEnableHid,
                detail::kEnableCdc);
        }
    }
}
