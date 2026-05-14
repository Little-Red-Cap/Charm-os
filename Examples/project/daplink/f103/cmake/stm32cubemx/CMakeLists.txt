cmake_minimum_required(VERSION 3.22)

project(stm32cubemx)
add_library(stm32cubemx INTERFACE)

daplink_require_stm32cube_root("f1" DAPLINK_STM32CUBE_F1_ROOT_RESOLVED)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

target_compile_definitions(stm32cubemx INTERFACE 
	USE_HAL_DRIVER 
	STM32F103xB
    $<$<CONFIG:Debug>:DEBUG>
)

target_include_directories(stm32cubemx INTERFACE
    ../../Core/Inc
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Inc
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/CMSIS/Device/ST/STM32F1xx/Include
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/CMSIS/Include
)

target_sources(stm32cubemx INTERFACE
    ../../Core/Src/main.c
    ../../Core/Src/gpio.c
    ../../Core/Src/usart.c
    ../../Core/Src/usb.c
    ../../Core/Src/stm32f1xx_it.c
    ../../Core/Src/stm32f1xx_hal_msp.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd_ex.c
    ${DAPLINK_STM32CUBE_F1_ROOT_RESOLVED}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_usb.c
    ../../Core/Src/system_stm32f1xx.c
    ../../Core/Src/sysmem.c
    ../../Core/Src/syscalls.c
    ../../startup_stm32f103xb.s
)

target_link_directories(stm32cubemx INTERFACE
)

target_link_libraries(stm32cubemx INTERFACE
)

# Validate that STM32CubeMX code is compatible with C standard
if(CMAKE_C_STANDARD LESS 11)
    message(ERROR "Generated code requires C11 or higher")
endif()

