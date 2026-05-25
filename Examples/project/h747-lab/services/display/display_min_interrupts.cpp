#include "display_min.h"

#include "stm32h7xx_hal.h"

extern DSI_HandleTypeDef hdsi_display_min;
extern LTDC_HandleTypeDef hltdc_display_min;

extern "C" {

void LTDC_IRQHandler(void) {
    HAL_LTDC_IRQHandler(&hltdc_display_min);
}

void LTDC_ER_IRQHandler(void) {
    HAL_LTDC_IRQHandler(&hltdc_display_min);
}

void DSI_IRQHandler(void) {
    HAL_DSI_IRQHandler(&hdsi_display_min);
}

} // extern "C"
