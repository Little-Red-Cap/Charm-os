#pragma once

#include <cstdint>

#include "stm32h7xx_hal.h"

extern "C" void SystemClock_Config(void);

namespace h747::port {

enum class GpioPortId : std::uint8_t {
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
};

struct Pin {
    GpioPortId port;
    std::uint8_t number;
};

inline constexpr Pin kLed{GpioPortId::G, 9};
inline constexpr Pin kPmicIrq{GpioPortId::B, 5};
inline constexpr Pin kWifiRegOn{GpioPortId::K, 2};
inline constexpr Pin kWifiHostWake{GpioPortId::G, 7};
inline constexpr Pin kBtReset{GpioPortId::J, 12};
inline constexpr Pin kBtWake{GpioPortId::B, 13};
inline constexpr Pin kBtHostWake{GpioPortId::B, 12};
inline constexpr Pin kPanelReset{GpioPortId::A, 8};

void runtime_init();
void init_default_peripherals();
[[noreturn]] void fail_fast();
std::uint32_t tick_ms();
void delay_ms(std::uint32_t ms);
UART_HandleTypeDef* uart1_handle();
UART_HandleTypeDef* uart2_handle();
#if !defined(H747_LAB_FOUNDATION_PLATFORM)
I2C_HandleTypeDef* i2c1_handle();
#endif
GPIO_TypeDef* gpio_port(GpioPortId port);
std::uint16_t gpio_mask(Pin pin);
GPIO_PinState gpio_read(Pin pin);
void gpio_write(Pin pin, GPIO_PinState state);
void gpio_toggle(Pin pin);

} // namespace h747::port
