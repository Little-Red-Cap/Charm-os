module;

#include <cstddef>
#include <cstdint>

export module daplink.port_facade;

import daplink.app_config;
import daplink.base.types;
import daplink.cmsis_dap;
import daplink.dap_init;
import daplink.dap_ops;
import daplink.dap_strategy;
import daplink.dap_transport;
import daplink.board;
import daplink.port_runtime;
import daplink.usb_minimal;
import daplink.io.channel;

namespace daplink::port_facade::detail {
    constexpr std::size_t kUartTxBurstLimit = 8;
    static_assert(daplink::usb_minimal::hid_packet_size == daplink::cmsis_dap::kPacketSize);

    inline daplink::io::result uart_read(void*, daplink::io::MutByteView buf) noexcept {
        std::size_t count = 0;
        while (count < buf.size() && daplink::board::cdc_uart_rx_ready()) {
            buf[count] = static_cast<daplink::base::u8>(daplink::board::cdc_uart_read());
            ++count;
        }
        if (count == 0) {
            return daplink::io::fail(daplink::io::errc::would_block);
        }
        return daplink::io::ok(count);
    }

    inline daplink::io::result uart_write(void*, daplink::io::ByteView buf) noexcept {
        const std::size_t limit = (buf.size() < kUartTxBurstLimit) ? buf.size() : kUartTxBurstLimit;
        if (limit == 0) {
            return daplink::io::fail(daplink::io::errc::would_block);
        }
        std::size_t count = 0;
        while (count < limit && daplink::board::cdc_uart_tx_ready()) {
            daplink::board::cdc_uart_write(static_cast<std::uint8_t>(buf[count]));
            ++count;
            if (daplink::board::cdc_uart_rx_pending()) {
                break;
            }
        }
        if (count == 0) {
            return daplink::io::fail(daplink::io::errc::would_block);
        }
        return daplink::io::ok(count);
    }

    inline daplink::io::result usb_cdc_read(void*, daplink::io::MutByteView buf) noexcept {
        if (!daplink::usb_minimal::cdc_out_ready()) {
            return daplink::io::fail(daplink::io::errc::would_block);
        }
        const auto payload = daplink::usb_minimal::cdc_out_packet();
        if (buf.size() < payload.size()) {
            return daplink::io::fail(daplink::io::errc::would_block);
        }
        const std::size_t len = payload.size();
        for (std::size_t i = 0; i < len; ++i) {
            buf[i] = static_cast<daplink::base::u8>(payload[i]);
        }
        daplink::usb_minimal::cdc_consume_out();
        return daplink::io::ok(len);
    }

    inline daplink::io::result usb_cdc_write(void*, daplink::io::ByteView buf) noexcept {
        if (!daplink::usb_minimal::cdc_send_in(
                reinterpret_cast<const std::uint8_t*>(buf.data()),
                static_cast<std::uint16_t>(buf.size()))) {
            return daplink::io::fail(daplink::io::errc::would_block);
        }
        return daplink::io::ok(buf.size());
    }
}

export namespace daplink::port_facade {
    using SwdBackend = daplink::board::SwdBackend;
    using DapOps = daplink::cmsis_dap::DefaultOps<SwdBackend>;
    using DapPolicy = daplink::dap_strategy::DefaultTransferPolicy<daplink::cmsis_dap::State>;
    using DapTransport = daplink::dap_transport::HidTransport<SwdBackend, DapOps, DapPolicy>;

    inline void init_or_fail() noexcept {
        daplink::port_runtime::init();
        if (!daplink::board::init_peripherals()) {
            daplink::port_runtime::fail_fast();
        }
        daplink::board::configure_debug_pins_hi_z();
    }

    inline auto usb_cdc_channel() noexcept -> daplink::io::Channel {
        return daplink::io::Channel{
            nullptr,
            daplink::io::ChannelOps{detail::usb_cdc_read, detail::usb_cdc_write, nullptr}
        };
    }

    inline auto cdc_uart_channel() noexcept -> daplink::io::Channel {
        return daplink::io::Channel{
            nullptr,
            daplink::io::ChannelOps{detail::uart_read, detail::uart_write, nullptr}
        };
    }

    inline auto make_dap_state() noexcept -> daplink::cmsis_dap::State {
        return daplink::dap_init::make_dap_state();
    }

    inline auto make_device_info() noexcept -> daplink::cmsis_dap::DeviceInfo {
        return {
            daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbManufacturer),
            daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbProduct),
            daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbSerial),
            daplink::cmsis_dap::make_info_field(daplink::app_config::kCmsisDapProtocolVersion),
            daplink::cmsis_dap::make_info_field(daplink::app_config::kProductFwVersion)
        };
    }

    inline auto make_dap_transport(daplink::cmsis_dap::State& state,
                                   const daplink::cmsis_dap::DeviceInfo& info) noexcept -> DapTransport {
        return DapTransport{state, info};
    }

    inline void apply_cdc_line(const std::uint32_t baud,
                               const std::uint8_t stop_bits,
                               const std::uint8_t parity,
                               const std::uint8_t data_bits) noexcept {
        daplink::board::cdc_uart_apply_line(baud, stop_bits, parity, data_bits);
    }
}
