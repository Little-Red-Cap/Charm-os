#include "main.h"
#include "usart.h"

#include <array>
#include <cstdint>

import daplink.board;
import daplink.usb_minimal;
import daplink.cmsis_dap;

extern "C" void SystemClock_Config(void);
extern "C" void MPU_Config(void);

#ifndef CHARM_DAP_CDC_UART
#define CHARM_DAP_CDC_UART 1
#endif

#if (CHARM_DAP_USB_PROFILE != 0)
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

    inline UART_HandleTypeDef* cdc_uart_handle() noexcept {
#if (CHARM_DAP_CDC_UART == 2)
        return &huart2;
#else
        return &huart1;
#endif
    }

    inline void apply_line_coding(UART_HandleTypeDef& uart,
                                  const daplink::usb_minimal::cdc_line_config& cfg) noexcept {
        uart.Init.BaudRate = cfg.baud;
        uart.Init.StopBits = UART_STOPBITS_1;
        if (cfg.stop_bits == 2) {
            uart.Init.StopBits = UART_STOPBITS_2;
        }
        uart.Init.Parity = UART_PARITY_NONE;
        if (cfg.parity == 1) {
            uart.Init.Parity = UART_PARITY_ODD;
        } else if (cfg.parity == 2) {
            uart.Init.Parity = UART_PARITY_EVEN;
        }
        uart.Init.WordLength = UART_WORDLENGTH_8B;
        if (cfg.data_bits == 9) {
            uart.Init.WordLength = UART_WORDLENGTH_9B;
        } else if (cfg.data_bits == 8 && uart.Init.Parity != UART_PARITY_NONE) {
            uart.Init.WordLength = UART_WORDLENGTH_9B;
        }
        (void)HAL_UART_Init(&uart);
    }
} // namespace
#endif

extern "C" void HAL_PCD_ResetCallback(PCD_HandleTypeDef* hpcd) {
    daplink::usb_minimal::on_reset(*hpcd);
}

extern "C" void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* hpcd) {
    daplink::usb_minimal::on_setup_stage(*hpcd);
}

extern "C" void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    daplink::usb_minimal::on_data_out_stage(*hpcd, epnum);
}

extern "C" void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    daplink::usb_minimal::on_data_in_stage(*hpcd, epnum);
}

int main()
{
    HAL_Init();
    SystemClock_Config();

    if (!daplink::board::init_peripherals()) {
        Error_Handler();
    }
    daplink::board::configure_debug_pins_hi_z();

    static_assert(daplink::usb_minimal::hid_packet_size == daplink::cmsis_dap::kPacketSize);
    daplink::cmsis_dap::State dap_state{};
    constexpr char kVendor[] = "Charm";
    constexpr char kProduct[] = "Charm CMSIS-DAP";
    constexpr char kSerial[] = "0001";
    constexpr char kFw[] = "0.1.0";
    const daplink::cmsis_dap::DeviceInfo kInfo{
        daplink::cmsis_dap::make_info_field(kVendor),
        daplink::cmsis_dap::make_info_field(kProduct),
        daplink::cmsis_dap::make_info_field(kSerial),
        daplink::cmsis_dap::make_info_field(kFw)
    };

#if (CHARM_DAP_USB_PROFILE != 0)
    auto* cdc_uart = cdc_uart_handle();
    ring_buffer uart_tx{};
    ring_buffer uart_rx{};
    daplink::usb_minimal::cdc_line_config last_line = daplink::usb_minimal::cdc_line();
    apply_line_coding(*cdc_uart, last_line);
#endif

    while (true) {
        if (daplink::usb_minimal::take_reset()) {
            dap_state = {};
#if (CHARM_DAP_USB_PROFILE != 0)
            uart_tx = {};
            uart_rx = {};
#endif
        }
#if (CHARM_DAP_USB_PROFILE != 0)
        const bool hid_busy = daplink::usb_minimal::out_ready();
        if (!hid_busy) {
            const auto line = daplink::usb_minimal::cdc_line();
            if (line.baud != last_line.baud || line.stop_bits != last_line.stop_bits ||
                line.parity != last_line.parity || line.data_bits != last_line.data_bits) {
                last_line = line;
                apply_line_coding(*cdc_uart, last_line);
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

            if (__HAL_UART_GET_FLAG(cdc_uart, UART_FLAG_RXNE) != RESET) {
                const std::uint8_t byte = static_cast<std::uint8_t>(cdc_uart->Instance->DR & 0xFFU);
                (void)rb_push(uart_rx, byte);
            }

            while (!rb_empty(uart_tx) &&
                   (__HAL_UART_GET_FLAG(cdc_uart, UART_FLAG_TXE) != RESET)) {
                std::uint8_t byte = 0;
                (void)rb_pop(uart_tx, byte);
                cdc_uart->Instance->DR = byte;
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
#endif
#if (CHARM_DAP_USB_PROFILE != 1)
        if (daplink::usb_minimal::out_ready()) {
            auto in = daplink::usb_minimal::out_packet();
            auto out = daplink::usb_minimal::in_packet();
            daplink::cmsis_dap::process_packet<daplink::board::SwdBackend>(dap_state, kInfo, in, out);
            daplink::usb_minimal::send_in_packet(static_cast<std::uint16_t>(daplink::cmsis_dap::kPacketSize));
            daplink::usb_minimal::consume_out();
        }
#endif
    }
}
