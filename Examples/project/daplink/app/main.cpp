#include "main.h"

#include <array>
#include <cstdint>

#include "app_config.h"

import daplink.board;
import daplink.usb_minimal;
import daplink.cmsis_dap;

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
    constexpr std::size_t kUartBufMask = kUartBufSize - 1;
    static_assert((kUartBufSize & kUartBufMask) == 0);

    struct ring_buffer {
        std::array<std::uint8_t, kUartBufSize> data{};
        std::uint16_t head = 0;
        std::uint16_t tail = 0;
    };

    inline bool rb_empty(const ring_buffer& rb) noexcept {
        return rb.head == rb.tail;
    }

    inline bool rb_full(const ring_buffer& rb) noexcept {
        return static_cast<std::uint16_t>((rb.head + 1) & kUartBufMask) == rb.tail;
    }

    inline bool rb_push(ring_buffer& rb, const std::uint8_t value) noexcept {
        if (rb_full(rb)) {
            return false;
        }
        rb.data[rb.head] = value;
        rb.head = static_cast<std::uint16_t>((rb.head + 1) & kUartBufMask);
        return true;
    }

    inline bool rb_pop(ring_buffer& rb, std::uint8_t& value) noexcept {
        if (rb_empty(rb)) {
            return false;
        }
        value = rb.data[rb.tail];
        rb.tail = static_cast<std::uint16_t>((rb.tail + 1) & kUartBufMask);
        return true;
    }

    inline std::uint16_t rb_count(const ring_buffer& rb) noexcept {
        return static_cast<std::uint16_t>((rb.head - rb.tail) & kUartBufMask);
    }

    inline std::uint16_t rb_peek(const ring_buffer& rb,
                                 std::uint8_t* dst,
                                 const std::uint16_t max_len) noexcept {
        const std::uint16_t available = rb_count(rb);
        const std::uint16_t len = (available < max_len) ? available : max_len;
        std::uint16_t index = rb.tail;
        for (std::uint16_t i = 0; i < len; ++i) {
            dst[i] = rb.data[index];
            index = static_cast<std::uint16_t>((index + 1) & kUartBufMask);
        }
        return len;
    }

    inline void rb_drop(ring_buffer& rb, const std::uint16_t len) noexcept {
        rb.tail = static_cast<std::uint16_t>((rb.tail + len) & kUartBufMask);
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
    const daplink::cmsis_dap::DeviceInfo kInfo{
        daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbManufacturer),
        daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbProduct),
        daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbSerial),
        daplink::cmsis_dap::make_info_field(daplink::app_config::kFwVersion)
    };

    ring_buffer uart_tx{};
    ring_buffer uart_rx{};
    daplink::usb_minimal::cdc_line_config last_line = daplink::usb_minimal::cdc_line();
    if constexpr (kEnableCdc) {
        daplink::board::cdc_uart_apply_line(
            last_line.baud, last_line.stop_bits, last_line.parity, last_line.data_bits);
    }

    while (true) {
        if (daplink::usb_minimal::take_reset()) {
            dap_state = {};
            if constexpr (kEnableCdc) {
                uart_tx = {};
                uart_rx = {};
            }
        }
        if constexpr (kEnableCdc) {
            const bool hid_busy = daplink::usb_minimal::out_ready();
            if (!hid_busy) {
                const auto line = daplink::usb_minimal::cdc_line();
                if (line.baud != last_line.baud || line.stop_bits != last_line.stop_bits ||
                    line.parity != last_line.parity || line.data_bits != last_line.data_bits) {
                    last_line = line;
                    daplink::board::cdc_uart_apply_line(
                        last_line.baud, last_line.stop_bits, last_line.parity, last_line.data_bits);
                }
                if (daplink::usb_minimal::cdc_out_ready()) {
                    const auto payload = daplink::usb_minimal::cdc_out_packet();
                    if (!payload.empty()) {
                        for (const auto byte : payload) {
                            if (!rb_push(uart_tx, byte)) {
                                break;
                            }
                        }
                        daplink::usb_minimal::cdc_consume_out();
                    } else {
                        daplink::usb_minimal::cdc_consume_out();
                    }
                }

                if (daplink::board::cdc_uart_rx_ready()) {
                    (void)rb_push(uart_rx, daplink::board::cdc_uart_read());
                }

                while (!rb_empty(uart_tx) && daplink::board::cdc_uart_tx_ready()) {
                    std::uint8_t byte = 0;
                    (void)rb_pop(uart_tx, byte);
                    daplink::board::cdc_uart_write(byte);
                }

                if (!rb_empty(uart_rx)) {
                    std::uint8_t temp[64] = {};
                    const auto len = rb_peek(uart_rx, temp, static_cast<std::uint16_t>(sizeof(temp)));
                    if (len != 0U) {
                        const bool sent = daplink::usb_minimal::cdc_send_in(temp, len);
                        if (sent) {
                            rb_drop(uart_rx, len);
                        }
                    }
                }
            }
        }
        if constexpr (kEnableHid) {
            if (daplink::usb_minimal::out_ready()) {
                auto in = daplink::usb_minimal::out_packet();
                auto out = daplink::usb_minimal::in_packet();
                daplink::cmsis_dap::process_packet<daplink::board::SwdBackend>(dap_state, kInfo, in, out);
                daplink::usb_minimal::send_in_packet(static_cast<std::uint16_t>(daplink::cmsis_dap::kPacketSize));
                daplink::usb_minimal::consume_out();
            }
        }
    }
}
