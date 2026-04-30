cmake_minimum_required(VERSION 3.22)

project(usb_device_audio_st C)

add_library(usb_device_audio_st STATIC
    App/usb_device.c
    App/usbd_desc.c
    App/usbd_audio_if.c
    Target/usbd_conf.c
    Class/AUDIO/Src/usbd_audio.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c
)

target_include_directories(usb_device_audio_st PUBLIC
    App
    Target
    ${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    Class/AUDIO/Inc
)

target_compile_definitions(usb_device_audio_st PUBLIC
    USE_USB_FS=1
)

target_link_libraries(usb_device_audio_st PUBLIC stm32cubemx)
