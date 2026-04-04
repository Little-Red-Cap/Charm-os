# Compiler options
set(STM32_MCU_FLAGS  "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard " )

# Linker options
set(STM32_LINKER_SCRIPT STM32H747XIHX_FLASH.ld)
set(STM32_LINKER_OPTION  )

# Include toolchain file
include("../gcc-arm-none-eabi.cmake")

set(PLAYER_PLATFORM_COMMON_SOURCES
    app/profile_main.cpp
    app/uart_bridge.c
    app/port_impl.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Class/CompositeBuilder/Src/usbd_composite_builder.c
    USB_DEVICE/Class/usbd_msc.c
    USB_DEVICE/Class/usbd_msc_bot.c
    USB_DEVICE/Class/usbd_msc_data.c
    USB_DEVICE/Class/usbd_msc_scsi.c
    USB_DEVICE/App/usbd_storage_if.c
)

function(platform_stm32h747_hal_attach_target target_name)
    include("mx-generated.cmake")
endfunction()
