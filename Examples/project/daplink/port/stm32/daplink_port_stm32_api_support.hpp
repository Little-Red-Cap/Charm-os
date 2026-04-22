#ifndef DAPLINK_PORT_STM32_API_SUPPORT_HPP
#define DAPLINK_PORT_STM32_API_SUPPORT_HPP

#include <cstdint>
#include <cstring>

extern "C" void SystemClock_Config(void);
extern "C" void Error_Handler(void);

namespace daplink::port {
    using UartHandle = UART_HandleTypeDef;
    using UsbPcdHandle = PCD_HandleTypeDef;
    using GpioPort = GPIO_TypeDef;

    enum class PinState : std::uint8_t {
        low = 0,
        high = 1,
    };

    enum class UsbEndpointType : std::uint8_t {
        control = 0,
        interrupt = 1,
        bulk = 2,
    };

    struct GpioConfig {
        std::uint32_t mode = 0;
        std::uint32_t pull = 0;
        std::uint32_t speed = 0;
    };

    inline constexpr std::uint32_t kGpioModeOutputPushPull = GPIO_MODE_OUTPUT_PP;
    inline constexpr std::uint32_t kGpioModeOutputOpenDrain = GPIO_MODE_OUTPUT_OD;
    inline constexpr std::uint32_t kGpioModeInput = GPIO_MODE_INPUT;
    inline constexpr std::uint32_t kGpioModeAnalog = GPIO_MODE_ANALOG;

    inline constexpr std::uint32_t kGpioPullNone = GPIO_NOPULL;
    inline constexpr std::uint32_t kGpioPullUp = GPIO_PULLUP;

    inline constexpr std::uint32_t kGpioSpeedLow = GPIO_SPEED_FREQ_LOW;
    inline constexpr std::uint32_t kGpioSpeedHigh = GPIO_SPEED_FREQ_HIGH;

    inline auto to_hal_pin_state(const PinState state) noexcept -> GPIO_PinState {
        return (state == PinState::high) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    }

    inline void gpio_init(GpioPort* port,
                          const std::uint32_t pin,
                          const GpioConfig& cfg) noexcept {
        GPIO_InitTypeDef gpio = {};
        gpio.Pin = pin;
        gpio.Mode = cfg.mode;
        gpio.Pull = cfg.pull;
        gpio.Speed = cfg.speed;
        HAL_GPIO_Init(port, &gpio);
    }

    inline void gpio_write(GpioPort* port, const std::uint32_t pin, const PinState state) noexcept {
        HAL_GPIO_WritePin(port, pin, to_hal_pin_state(state));
    }

    inline auto gpio_read(GpioPort* port, const std::uint32_t pin) noexcept -> bool {
        return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET;
    }

    inline void delay_ms(const std::uint32_t ms) noexcept {
        HAL_Delay(ms);
    }

    inline void nop() noexcept {
        __NOP();
    }

    inline auto system_core_clock_hz() noexcept -> std::uint32_t {
        return SystemCoreClock;
    }

    inline void runtime_init() noexcept {
        HAL_Init();
        SystemClock_Config();
    }

    inline auto tick_ms() noexcept -> std::uint32_t {
        return HAL_GetTick();
    }

    inline void fail_fast() noexcept {
        Error_Handler();
    }

    inline void uart_post_init_default(UartHandle* uart) noexcept {
#if defined(USART_CR1_FIFOEN)
        if (uart == nullptr) {
            return;
        }
        (void)HAL_UARTEx_SetTxFifoThreshold(uart, UART_TXFIFO_THRESHOLD_1_8);
        (void)HAL_UARTEx_SetRxFifoThreshold(uart, UART_RXFIFO_THRESHOLD_1_8);
        (void)HAL_UARTEx_DisableFifoMode(uart);
#else
        (void)uart;
#endif
    }

    inline void uart_apply_line(UartHandle* uart,
                                const std::uint32_t baud,
                                const std::uint8_t stop_bits,
                                const std::uint8_t parity,
                                const std::uint8_t data_bits) noexcept {
        if (uart == nullptr) {
            return;
        }

        uart->Init.BaudRate = baud;
        uart->Init.StopBits = (stop_bits == 2U) ? UART_STOPBITS_2 : UART_STOPBITS_1;
        uart->Init.Parity = UART_PARITY_NONE;
        if (parity == 1U) {
            uart->Init.Parity = UART_PARITY_ODD;
        } else if (parity == 2U) {
            uart->Init.Parity = UART_PARITY_EVEN;
        }
        uart->Init.WordLength = UART_WORDLENGTH_8B;
        if (data_bits == 9U) {
            uart->Init.WordLength = UART_WORDLENGTH_9B;
        } else if (data_bits == 8U && uart->Init.Parity != UART_PARITY_NONE) {
            uart->Init.WordLength = UART_WORDLENGTH_9B;
        }
        (void)HAL_UART_Init(uart);
    }

    inline void uart_clear_overrun(UartHandle* uart) noexcept {
        if (uart == nullptr) {
            return;
        }
        if (__HAL_UART_GET_FLAG(uart, UART_FLAG_ORE) != RESET) {
            __HAL_UART_CLEAR_OREFLAG(uart);
        }
    }

    inline auto uart_rx_ready(UartHandle* uart) noexcept -> bool {
        uart_clear_overrun(uart);
        if (uart == nullptr) {
            return false;
        }
        return __HAL_UART_GET_FLAG(uart, UART_FLAG_RXNE) != RESET;
    }

    inline auto uart_rx_pending(UartHandle* uart) noexcept -> bool {
        if (uart == nullptr) {
            return false;
        }
        return (__HAL_UART_GET_FLAG(uart, UART_FLAG_RXNE) != RESET) ||
            (__HAL_UART_GET_FLAG(uart, UART_FLAG_ORE) != RESET);
    }

    inline auto uart_tx_ready(UartHandle* uart) noexcept -> bool {
        if (uart == nullptr) {
            return false;
        }
        return __HAL_UART_GET_FLAG(uart, UART_FLAG_TXE) != RESET;
    }

    inline auto uart_data_read(const UartHandle* uart) noexcept -> std::uint8_t {
#if defined(USART_RDR_RDR)
        return static_cast<std::uint8_t>(uart->Instance->RDR & 0xFFU);
#else
        return static_cast<std::uint8_t>(uart->Instance->DR & 0xFFU);
#endif
    }

    inline void uart_data_write(UartHandle* uart, const std::uint8_t byte) noexcept {
#if defined(USART_TDR_TDR)
        uart->Instance->TDR = byte;
#else
        uart->Instance->DR = byte;
#endif
    }

    inline void usb_init_pcd() noexcept {
        MX_USB_PCD_Init();
    }

    inline auto usb_start(UsbPcdHandle& hpcd) noexcept -> bool {
        return HAL_OK == HAL_PCD_Start(&hpcd);
    }

    inline auto usb_pma_config_single_buffer(UsbPcdHandle& hpcd,
                                             const std::uint8_t ep_addr,
                                             const std::uint16_t pma_addr) noexcept -> bool {
        return HAL_OK == HAL_PCDEx_PMAConfig(&hpcd, ep_addr, PCD_SNG_BUF, pma_addr);
    }

    inline auto to_hal_ep_type(const UsbEndpointType type) noexcept -> std::uint8_t {
        switch (type) {
            case UsbEndpointType::control:
                return EP_TYPE_CTRL;
            case UsbEndpointType::interrupt:
                return EP_TYPE_INTR;
            case UsbEndpointType::bulk:
                return EP_TYPE_BULK;
        }
        return EP_TYPE_CTRL;
    }

    inline auto usb_set_address(UsbPcdHandle& hpcd, const std::uint8_t address) noexcept -> bool {
        return HAL_OK == HAL_PCD_SetAddress(&hpcd, address);
    }

    inline auto usb_ep_open(UsbPcdHandle& hpcd,
                            const std::uint8_t ep_addr,
                            const std::uint16_t mps,
                            const UsbEndpointType type) noexcept -> bool {
        return HAL_OK == HAL_PCD_EP_Open(&hpcd, ep_addr, mps, to_hal_ep_type(type));
    }

    inline auto usb_ep_receive(UsbPcdHandle& hpcd,
                               const std::uint8_t ep_addr,
                               std::uint8_t* data,
                               const std::uint16_t len) noexcept -> bool {
        return HAL_OK == HAL_PCD_EP_Receive(&hpcd, ep_addr, data, len);
    }

    inline auto usb_ep_transmit(UsbPcdHandle& hpcd,
                                const std::uint8_t ep_addr,
                                std::uint8_t* data,
                                const std::uint16_t len) noexcept -> bool {
        return HAL_OK == HAL_PCD_EP_Transmit(&hpcd, ep_addr, data, len);
    }

    inline auto usb_ep_set_stall(UsbPcdHandle& hpcd, const std::uint8_t ep_addr) noexcept -> bool {
        return HAL_OK == HAL_PCD_EP_SetStall(&hpcd, ep_addr);
    }

    inline auto usb_ep_rx_count(UsbPcdHandle& hpcd, const std::uint8_t ep_addr) noexcept -> std::uint16_t {
        return static_cast<std::uint16_t>(HAL_PCD_EP_GetRxCount(&hpcd, ep_addr));
    }

    inline void usb_copy_setup_packet(const UsbPcdHandle& hpcd, std::uint8_t (&setup)[8]) noexcept {
        std::memcpy(setup, hpcd.Setup, sizeof(setup));
    }
}

#endif
