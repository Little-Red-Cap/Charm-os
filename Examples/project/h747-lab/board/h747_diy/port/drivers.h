#pragma once

#include "stm32h7xx_hal.h"

namespace h747::board {

void init_default_peripherals();

} // namespace h747::board

extern "C" {
extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c4;
extern QSPI_HandleTypeDef hqspi;
extern SDRAM_HandleTypeDef hsdram1;
extern SDRAM_HandleTypeDef hsdram2;
extern TIM_HandleTypeDef htim5;
extern TIM_HandleTypeDef htim8;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
}
