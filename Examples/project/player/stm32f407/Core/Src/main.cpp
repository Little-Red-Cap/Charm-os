#include "main.h"

#include <array>
#include <cstddef>
#include <cstring>

#include "gpio.h"
#include "i2c.h"
#include "usart.h"

import player.stm32.fs_demo;
import lcd_driver;
import charm.core.config;
import charm.core.geometry;
import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.pixel_format;
import charm.gfx.render;

extern "C" void SystemClock_Config(void);

int main()
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();

    fs_demo_run();
    LCD_Init();

    constexpr int kTileHeight = 32;
    constexpr std::size_t kTileStride = static_cast<std::size_t>(screen_width) * 2;
    constexpr std::size_t kTileBytes = kTileStride * static_cast<std::size_t>(kTileHeight);
    static std::array<std::byte, kTileBytes> g_tile_buf{};
    RuntimeCanvas canvas(g_tile_buf.data(),
                         screen_width,
                         kTileHeight,
                         PixelFormat::RGB565,
                         kTileStride);

    while (true) {
        HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
        for (int ty = 0; ty < screen_height; ty += kTileHeight) {
            const int tile_h = (ty + kTileHeight <= screen_height)
                ? kTileHeight
                : (screen_height - ty);
            Rect clip{0, ty, screen_width, tile_h};
            canvas.set_origin(0, -ty);
            canvas.set_clip(clip);
            canvas.clear(rgba{18, 22, 30, 255});
            ui::render::draw_rect(canvas, 24, 24, 200, 80, rgba{60, 120, 200, 255}, true);
            ui::render::draw_rect(canvas, 30, 30, 188, 68, rgba{18, 22, 30, 255}, true);
            ui::render::draw_rect(canvas, 260, 40, 180, 60, rgba{30, 180, 120, 255}, true);
            canvas.clear_origin();
            canvas.clear_clip();
            LCD_BlitRect565(0, static_cast<uint16_t>(ty), screen_width, static_cast<uint16_t>(tile_h),
                            reinterpret_cast<const uint16_t*>(g_tile_buf.data()));
        }
        HAL_Delay(16);
    }
}
