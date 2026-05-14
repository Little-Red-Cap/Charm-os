include_guard(GLOBAL)

set(DAPLINK_PORT_NAME "f103")
set(DAPLINK_PORT_LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/stm32f103c8tx_flash.ld")
set(DAPLINK_PORT_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../cmake/toolchains/daplink-f103-gcc-arm-none-eabi.cmake")
set(DAPLINK_PORT_TARGET_FLAGS
    -mcpu=cortex-m3
)

set(DAPLINK_PORT_EXTRA_CPP_SOURCES "")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/daplink_port_usb_callbacks.cpp")
    list(APPEND DAPLINK_PORT_EXTRA_CPP_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/daplink_port_usb_callbacks.cpp"
    )
endif()
