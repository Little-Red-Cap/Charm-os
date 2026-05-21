#include "port.h"

#include "drivers.h"

extern "C" void Error_Handler(void);

namespace h747::port {

void runtime_init() {
    HAL_Init();
    SystemClock_Config();
}

void init_default_peripherals() {
    h747::board::init_default_peripherals();
}

[[noreturn]] void fail_fast() {
    Error_Handler();
    while (true) {
    }
}

std::uint32_t tick_ms() {
    return HAL_GetTick();
}

void delay_ms(const std::uint32_t ms) {
    HAL_Delay(ms);
}

UART_HandleTypeDef* uart1_handle() {
    return &huart1;
}

UART_HandleTypeDef* uart2_handle() {
    return &huart2;
}

I2C_HandleTypeDef* i2c1_handle() {
    return &hi2c1;
}

GPIO_TypeDef* gpio_port(const GpioPortId port) {
    switch (port) {
    case GpioPortId::A: return GPIOA;
    case GpioPortId::B: return GPIOB;
    case GpioPortId::C: return GPIOC;
    case GpioPortId::D: return GPIOD;
    case GpioPortId::E: return GPIOE;
    case GpioPortId::F: return GPIOF;
    case GpioPortId::G: return GPIOG;
    case GpioPortId::H: return GPIOH;
    case GpioPortId::I: return GPIOI;
    case GpioPortId::J: return GPIOJ;
    case GpioPortId::K: return GPIOK;
    default: return nullptr;
    }
}

std::uint16_t gpio_mask(const Pin pin) {
    return static_cast<std::uint16_t>(1U << pin.number);
}

GPIO_PinState gpio_read(const Pin pin) {
    return HAL_GPIO_ReadPin(gpio_port(pin.port), gpio_mask(pin));
}

void gpio_write(const Pin pin, const GPIO_PinState state) {
    HAL_GPIO_WritePin(gpio_port(pin.port), gpio_mask(pin), state);
}

void gpio_toggle(const Pin pin) {
    HAL_GPIO_TogglePin(gpio_port(pin.port), gpio_mask(pin));
}

} // namespace h747::port
