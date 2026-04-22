set(DAPLINK_TOOLCHAIN_TARGET_FLAGS "-mcpu=cortex-m33 -mfpu=fpv4-sp-d16 -mfloat-abi=hard ")
set(DAPLINK_TOOLCHAIN_LINKER_SCRIPT "../../h503/STM32H503xx_FLASH.ld")

include("${CMAKE_CURRENT_LIST_DIR}/daplink_stm32_toolchain_common.cmake")
