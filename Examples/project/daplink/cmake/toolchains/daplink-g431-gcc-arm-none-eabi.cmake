set(DAPLINK_TOOLCHAIN_TARGET_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard ")
set(DAPLINK_TOOLCHAIN_LINKER_SCRIPT "../../g431/stm32g431cbtx_flash.ld")

include("${CMAKE_CURRENT_LIST_DIR}/daplink_stm32_toolchain_common.cmake")
