#include "main.h"

#include "gpio.h"
#include <cstdint>

import daplink.board;
import daplink.usb_minimal;

extern "C" void SystemClock_Config(void);
extern "C" void MPU_Config(void);

inline void toggle_led() { HAL_GPIO_TogglePin(CONNECT_LED_GPIO_Port, CONNECT_LED_Pin); }

extern "C" void HAL_PCD_ResetCallback(PCD_HandleTypeDef* hpcd) {
    daplink::usb_minimal::on_reset(*hpcd);
}

extern "C" void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* hpcd) {
    daplink::usb_minimal::on_setup_stage(*hpcd);
}

extern "C" void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* hpcd, std::uint8_t epnum) {
    daplink::usb_minimal::on_data_out_stage(*hpcd, epnum);
}

extern "C" void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* hpcd, std::uint8_t epnum) {
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

    while (true) {
        // TODO(daplink): replace heartbeat loop with CMSIS-DAP request pump.
        toggle_led();
        HAL_Delay(1000);
    }
}
