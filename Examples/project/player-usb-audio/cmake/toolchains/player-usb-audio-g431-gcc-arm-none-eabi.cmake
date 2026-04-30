set(PLAYER_USB_AUDIO_TOOLCHAIN_TARGET_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard ")
set(PLAYER_USB_AUDIO_TOOLCHAIN_LINKER_SCRIPT "../../g431/stm32g431cbtx_flash.ld")

include("${CMAKE_CURRENT_LIST_DIR}/player_usb_audio_stm32_toolchain_common.cmake")
