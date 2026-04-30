cmake_minimum_required(VERSION 3.22)

project(stm32cubemx)
add_library(stm32cubemx INTERFACE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

target_compile_definitions(stm32cubemx INTERFACE 
	USE_HAL_DRIVER 
	STM32G431xx
    $<$<CONFIG:Debug>:DEBUG>
)

target_include_directories(stm32cubemx INTERFACE
    ../../Core/Inc
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Inc
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/CMSIS/Include
)

target_sources(stm32cubemx INTERFACE
    ../../Core/Src/main.c
    ../../Core/Src/gpio.c
    ../../Core/Src/dma.c
    ../../Core/Src/i2c.c
    ../../Core/Src/i2s.c
    ../../Core/Src/opamp.c
    ../../Core/Src/spi.c
    ../../Core/Src/tim.c
    ../../Core/Src/usb.c
    ../../Core/Src/stm32g4xx_it.c
    ../../Core/Src/stm32g4xx_hal_msp.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_i2c.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_i2c_ex.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc_ex.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ex.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ramfunc.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_gpio.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_exti.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma_ex.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr_ex.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_cortex.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_i2s.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_opamp.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_opamp_ex.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_spi.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_spi_ex.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim_ex.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pcd.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pcd_ex.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_ll_usb.c
    ../../Core/Src/system_stm32g4xx.c
    ../../Core/Src/sysmem.c
    ../../Core/Src/syscalls.c
    ../../startup_stm32g431xx.s
)

target_link_directories(stm32cubemx INTERFACE
)

target_link_libraries(stm32cubemx INTERFACE
)

# Validate that STM32CubeMX code is compatible with C standard
if(CMAKE_C_STANDARD LESS 11)
    message(ERROR "Generated code requires C11 or higher")
endif()

