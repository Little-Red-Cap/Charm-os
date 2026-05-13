include_guard(GLOBAL)

set(DAPLINK_PORT_NAME "h503")
set(DAPLINK_PORT_LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/STM32H503xx_FLASH.ld")
set(DAPLINK_PORT_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../cmake/toolchains/daplink-h503-gcc-arm-none-eabi.cmake")
set(DAPLINK_PORT_TARGET_FLAGS
    -mcpu=cortex-m33
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

set(DAPLINK_PORT_EXTRA_CPP_SOURCES "")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/daplink_port_usb_callbacks.cpp")
    list(APPEND DAPLINK_PORT_EXTRA_CPP_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/daplink_port_usb_callbacks.cpp"
    )
endif()
