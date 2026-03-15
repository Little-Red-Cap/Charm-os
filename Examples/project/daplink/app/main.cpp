#include "main.h"

#include <array>
#include <cstdint>

import daplink.board;
import daplink.usb_minimal;
import daplink.cmsis_dap;
import daplink.app_config;
import daplink.ring_buffer;
import daplink.dap_queue;
import io.channel;
import util.core;

namespace {
    constexpr auto kUsbProfile = daplink::app_config::kConfig.usb.profile;
    constexpr bool kEnableCdc =
        (kUsbProfile == daplink::app_config::UsbProfile::cdc) ||
        (kUsbProfile == daplink::app_config::UsbProfile::composite);
    constexpr bool kEnableHid =
        (kUsbProfile == daplink::app_config::UsbProfile::hid) ||
        (kUsbProfile == daplink::app_config::UsbProfile::composite);

    static_assert(!kEnableHid || (daplink::usb_minimal::hid_packet_size == daplink::cmsis_dap::kPacketSize));
}

extern "C" void SystemClock_Config(void);
extern "C" void MPU_Config(void);

namespace {
    constexpr std::size_t kUartBufSize = 256;
    using UartRing = daplink::ring_buffer::Buffer<kUartBufSize>;
    constexpr std::uint8_t kDapBurstLimit = daplink::app_config::kConfig.dap.burst_limit;
    constexpr std::size_t kIoChunk = 64;

    io::result uart_read(void*, io::MutByteView buf) noexcept {
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

    io::result uart_write(void*, io::ByteView buf) noexcept {
        std::size_t count = 0;
        while (count < buf.size() && daplink::board::cdc_uart_tx_ready()) {
            daplink::board::cdc_uart_write(static_cast<std::uint8_t>(buf[count]));
            ++count;
        }
        if (count == 0) {
            return io::fail(io::errc::would_block);
        }
        return io::ok(count);
    }

    io::result usb_cdc_read(void*, io::MutByteView buf) noexcept {
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

    io::result usb_cdc_write(void*, io::ByteView buf) noexcept {
        if (!daplink::usb_minimal::cdc_send_in(
                reinterpret_cast<const std::uint8_t*>(buf.data()),
                static_cast<std::uint16_t>(buf.size()))) {
            return io::fail(io::errc::would_block);
        }
        return io::ok(buf.size());
    }

    void pump_read(io::Channel& ch, UartRing& rb) noexcept {
        const std::uint16_t free = rb.free();
        if (free == 0) {
            return;
        }
        std::array<util::u8, kIoChunk> temp{};
        const std::size_t want = (free < temp.size()) ? free : temp.size();
        auto r = ch.read(io::MutByteView{temp.data(), want});
        if (!r) {
            return;
        }
        const auto count = static_cast<std::uint16_t>(r.value());
        for (std::uint16_t i = 0; i < count; ++i) {
            (void)rb.push(static_cast<std::uint8_t>(temp[i]));
        }
    }

    void pump_write(io::Channel& ch, UartRing& rb) noexcept {
        const std::uint16_t available = rb.count();
        if (available == 0) {
            return;
        }
        std::array<util::u8, kIoChunk> temp{};
        const std::uint16_t want =
            static_cast<std::uint16_t>((available < temp.size()) ? available : temp.size());
        const auto len = rb.peek(reinterpret_cast<std::uint8_t*>(temp.data()), want);
        if (len == 0) {
            return;
        }
        auto r = ch.write(io::ByteView{temp.data(), len});
        if (r) {
            rb.drop(static_cast<std::uint16_t>(r.value()));
        }
    }
} // namespace

int main()
{
    HAL_Init();
    SystemClock_Config();

    if (!daplink::board::init_peripherals()) {
        Error_Handler();
    }
    daplink::board::configure_debug_pins_hi_z();

    daplink::cmsis_dap::State dap_state{};
    dap_state.current_hz = daplink::app_config::kConfig.swd.default_hz;
    dap_state.min_hz = daplink::app_config::kConfig.swd.min_hz;
    const daplink::cmsis_dap::DeviceInfo kInfo{
        daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbManufacturer),
        daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbProduct),
        daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbSerial),
        daplink::cmsis_dap::make_info_field(daplink::app_config::kFwVersion)
    };

    UartRing uart_tx{};
    UartRing uart_rx{};
    daplink::dap_queue::Queue<> dap_queue{};
    io::Channel usb_cdc{
        nullptr,
        io::ChannelOps{usb_cdc_read, usb_cdc_write, nullptr}
    };
    io::Channel uart{
        nullptr,
        io::ChannelOps{uart_read, uart_write, nullptr}
    };
    daplink::usb_minimal::cdc_line_config last_line = daplink::usb_minimal::cdc_line();
    if constexpr (kEnableCdc) {
        daplink::board::cdc_uart_apply_line(
            last_line.baud, last_line.stop_bits, last_line.parity, last_line.data_bits);
    }

    while (true) {
        if (daplink::usb_minimal::take_reset()) {
            dap_state = {};
            dap_queue.reset();
            if constexpr (kEnableCdc) {
                uart_tx = {};
                uart_rx = {};
            }
        }
        daplink::usb_minimal::poll();
        if constexpr (kEnableHid) {
            std::uint8_t processed = 0;
            while (daplink::usb_minimal::out_ready() && processed < kDapBurstLimit) {
                if (!dap_queue.can_accept()) {
                    break;
                }
                auto in = daplink::usb_minimal::out_packet();
                if (!dap_queue.enqueue<daplink::board::SwdBackend>(dap_state, kInfo, in)) {
                    break;
                }
                daplink::usb_minimal::consume_out();
                ++processed;
            }
            if (dap_queue.has_pending()) {
                auto pending = dap_queue.peek();
                auto out = daplink::usb_minimal::in_packet();
                const auto len = dap_queue.peek_len();
                for (std::uint16_t i = 0; i < len; ++i) {
                    out[i] = pending[i];
                }
                if (daplink::usb_minimal::try_send_in_packet(len)) {
                    dap_queue.consume();
                }
            }
        }
        if constexpr (kEnableCdc) {
            const bool dap_busy =
                daplink::usb_minimal::out_ready() ||
                daplink::usb_minimal::hid_in_busy() ||
                dap_queue.has_pending();
            if (!dap_busy) {
                const auto line = daplink::usb_minimal::cdc_line();
                if (line.baud != last_line.baud || line.stop_bits != last_line.stop_bits ||
                    line.parity != last_line.parity || line.data_bits != last_line.data_bits) {
                    last_line = line;
                    daplink::board::cdc_uart_apply_line(
                        last_line.baud, last_line.stop_bits, last_line.parity, last_line.data_bits);
                }
                pump_read(usb_cdc, uart_tx);
                pump_read(uart, uart_rx);
                pump_write(uart, uart_tx);
                pump_write(usb_cdc, uart_rx);
            }
        }
    }
}
