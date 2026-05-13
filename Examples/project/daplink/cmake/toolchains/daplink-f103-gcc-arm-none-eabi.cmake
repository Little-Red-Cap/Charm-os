set(DAPLINK_TOOLCHAIN_PORT "f103")
set(DAPLINK_TOOLCHAIN_TARGET_FLAGS "-mcpu=cortex-m3 ")
set(DAPLINK_TOOLCHAIN_LINKER_SCRIPT "../../f103/stm32f103c8tx_flash.ld")

include("${CMAKE_CURRENT_LIST_DIR}/daplink_stm32_toolchain_common.cmake")
