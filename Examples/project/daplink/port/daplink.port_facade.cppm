module;

#include <cstddef>
#include <cstdint>

export module daplink.port_facade;

import daplink.board;
import daplink.port_runtime;
import daplink.usb_minimal;
import io.channel;
import util.core;

namespace daplink::port_facade::detail {
    constexpr std::size_t kUartTxBurstLimit = 8;

    inline io::result uart_read(void*, io::MutByteView buf) noexcept {
        std::size_t count = 0;
        while (count < buf.size() && daplink::board::cdc_uart_rx_ready()) {
            buf[count] = static_cast<util::u8>(daplink::board::cdc_uart_read());
            ++count;
        }
        if (count == 0) {
            return io::fail(io::errc::would_block);
        }
        return io::ok(count);
    }

    inline io::result uart_write(void*, io::ByteView buf) noexcept {
        const std::size_t limit = (buf.size() < kUartTxBurstLimit) ? buf.size() : kUartTxBurstLimit;
        if (limit == 0) {
            return io::fail(io::errc::would_block);
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
            return io::fail(io::errc::would_block);
        }
        return io::ok(count);
    }

    inline io::result usb_cdc_read(void*, io::MutByteView buf) noexcept {
        if (!daplink::usb_minimal::cdc_out_ready()) {
            return io::fail(io::errc::would_block);
        }
        const auto payload = daplink::usb_minimal::cdc_out_packet();
        if (buf.size() < payload.size()) {
            return io::fail(io::errc::would_block);
        }
        const std::size_t len = payload.size();
        for (std::size_t i = 0; i < len; ++i) {
            buf[i] = static_cast<util::u8>(payload[i]);
        }
        daplink::usb_minimal::cdc_consume_out();
        return io::ok(len);
    }

    inline io::result usb_cdc_write(void*, io::ByteView buf) noexcept {
        if (!daplink::usb_minimal::cdc_send_in(
                reinterpret_cast<const std::uint8_t*>(buf.data()),
                static_cast<std::uint16_t>(buf.size()))) {
            return io::fail(io::errc::would_block);
        }
        return io::ok(buf.size());
    }
}

export namespace daplink::port_facade {
    using SwdBackend = daplink::board::SwdBackend;

    inline void init_or_fail() noexcept {
        daplink::port_runtime::init();
        if (!daplink::board::init_peripherals()) {
            daplink::port_runtime::fail_fast();
        }
        daplink::board::configure_debug_pins_hi_z();
    }

    inline auto usb_cdc_channel() noexcept -> io::Channel {
        return io::Channel{
            nullptr,
            io::ChannelOps{detail::usb_cdc_read, detail::usb_cdc_write, nullptr}
        };
    }

    inline auto cdc_uart_channel() noexcept -> io::Channel {
        return io::Channel{
            nullptr,
            io::ChannelOps{detail::uart_read, detail::uart_write, nullptr}
        };
    }

    inline void apply_cdc_line(const std::uint32_t baud,
                               const std::uint8_t stop_bits,
                               const std::uint8_t parity,
                               const std::uint8_t data_bits) noexcept {
        daplink::board::cdc_uart_apply_line(baud, stop_bits, parity, data_bits);
    }
}
