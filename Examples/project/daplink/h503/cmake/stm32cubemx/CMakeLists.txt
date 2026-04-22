cmake_minimum_required(VERSION 3.22)

project(stm32cubemx)
add_library(stm32cubemx INTERFACE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

target_compile_definitions(stm32cubemx INTERFACE 
	USE_HAL_DRIVER 
	STM32H503xx
    $<$<CONFIG:Debug>:DEBUG>
)

target_include_directories(stm32cubemx INTERFACE
    ../../Core/Inc
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Inc
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Inc/Legacy
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/CMSIS/Device/ST/STM32H5xx/Include
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/CMSIS/Include
)

target_sources(stm32cubemx INTERFACE
    ../../Core/Src/main.c
    ../../Core/Src/gpio.c
    ../../Core/Src/gpdma.c
    ../../Core/Src/icache.c
    ../../Core/Src/memorymap.c
    ../../Core/Src/usart.c
    ../../Core/Src/usb.c
    ../../Core/Src/stm32h5xx_it.c
    ../../Core/Src/stm32h5xx_hal_msp.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_cortex.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc_ex.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash_ex.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_gpio.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma_ex.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr_ex.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_exti.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_icache.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_uart.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_uart_ex.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pcd.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pcd_ex.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H5_V1.4.0/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_usb.c
    ../../Core/Src/system_stm32h5xx.c
    ../../Core/Src/sysmem.c
    ../../Core/Src/syscalls.c
    ../../startup_stm32h503xx.s
)

target_link_directories(stm32cubemx INTERFACE
)

target_link_libraries(stm32cubemx INTERFACE
)

# Validate that STM32CubeMX code is compatible with C standard
if(CMAKE_C_STANDARD LESS 11)
    message(ERROR "Generated code requires C11 or higher")
endif()


