#include "main.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

#include "gpio.h"
#include "i2c.h"
#include "usart.h"

import player.stm32.fs_demo;
import lcd_driver;
import charm.core.config;
import charm.core.container;
import charm.core.factory;
import charm.core.gui;
import charm.core.geometry;
import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.pixel_format;
import charm.gfx.render;
import charm.widgets.label;
import charm.widgets.progress;
import charm.widgets.text;
import out.api;

extern "C" void SystemClock_Config(void);

out::port::console_sink uart_sink;

int main()
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    out::println<"boot: init ok">(uart_sink);
    LCD_Init();
    out::println<"boot: lcd init ok">(uart_sink);
    fs_demo_run();
    out::println<"boot: fs demo done">(uart_sink);

    auto color = false;
    while (true) {
        HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
        color = !color;
        // LCD_Clear(color ? 0xFFFF : 0);
        HAL_Delay(1000);
    }
}
