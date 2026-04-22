#ifndef DAPLINK_BACKEND_SUPPORT_HPP
#define DAPLINK_BACKEND_SUPPORT_HPP

#include "daplink_port_contract.hpp"
#include "daplink_port_api.hpp"

#include <cstdint>

namespace daplink::backend_support {
    struct DefaultTraits {
        static constexpr bool kInitDmaBeforeUart2 = false;

        static void init_dma() noexcept {}
    };

    inline void default_uart_post_init(daplink::port::UartHandle* uart) noexcept {
        daplink::port::uart_post_init_default(uart);
    }

    inline auto default_uart_data_read(const daplink::port::UartHandle* uart) noexcept -> std::uint8_t {
        return daplink::port::uart_data_read(uart);
    }

    inline void default_uart_data_write(daplink::port::UartHandle* uart, const std::uint8_t byte) noexcept {
        daplink::port::uart_data_write(uart, byte);
    }

    template <typename Traits>
    struct BasicBackendOps {
        static_assert(daplink::port_contract::BackendTraits<Traits>,
                      "daplink backend traits do not satisfy the required contract.");

        static void init_cdc_uart(const std::uint8_t uart_index) noexcept {
            if (uart_index == 2U) {
                if constexpr (Traits::kInitDmaBeforeUart2) {
                    Traits::init_dma();
                }
                Traits::init_uart2();
            } else {
                Traits::init_uart1();
            }
        }

        static auto cdc_uart_handle(const std::uint8_t uart_index) noexcept -> daplink::port::UartHandle* {
            return (uart_index == 2U) ? Traits::uart2_handle() : Traits::uart1_handle();
        }

        static void init_usb_pcd() noexcept {
            daplink::port::usb_init_pcd();
        }

        static auto usb_pcd_handle() noexcept -> daplink::port::UsbPcdHandle& {
            return Traits::usb_pcd_handle();
        }

        static void cdc_uart_post_init(daplink::port::UartHandle* uart) noexcept {
            default_uart_post_init(uart);
        }

        static auto cdc_uart_data_read(const daplink::port::UartHandle* uart) noexcept -> std::uint8_t {
            return default_uart_data_read(uart);
        }

        static void cdc_uart_data_write(daplink::port::UartHandle* uart, const std::uint8_t byte) noexcept {
            default_uart_data_write(uart, byte);
        }
    };
}

#endif
