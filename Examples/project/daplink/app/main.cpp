#include "main.h"

#include <cstdint>

import daplink.board;
import daplink.usb_minimal;
import daplink.cmsis_dap;
import daplink.app_config;
import daplink.ring_buffer;

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

    UartRing uart_tx{};
    UartRing uart_rx{};
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
                    for (std::size_t i = 0; i < payload.size(); ++i) {
                        if (!uart_tx.push(payload[i])) {
                            break;
                        }
                    }
                }
                daplink::usb_minimal::cdc_consume_out();
            }

                if (daplink::board::cdc_uart_rx_ready()) {
                (void)uart_rx.push(daplink::board::cdc_uart_read());
            }

            while (!uart_tx.empty() && daplink::board::cdc_uart_tx_ready()) {
                std::uint8_t byte = 0;
                (void)uart_tx.pop(byte);
                daplink::board::cdc_uart_write(byte);
            }

            if (!uart_rx.empty()) {
                std::uint8_t temp[64] = {};
                const auto len = uart_rx.peek(temp, static_cast<std::uint16_t>(sizeof(temp)));
                if (len != 0U) {
                    const bool sent = daplink::usb_minimal::cdc_send_in(temp, len);
                    if (sent) {
                        uart_rx.drop(len);
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
