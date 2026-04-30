#TODO 修复这个有问题的CMake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CHARM_ARM_TRIPLE "arm-none-eabi" CACHE STRING "ARM bare-metal toolchain triple")
set(CHARM_ARM_CPU "cortex-m7" CACHE STRING "ARM Cortex-M CPU for bare-metal builds")
set(CHARM_ARM_FLOAT_ABI "hard" CACHE STRING "ARM floating-point ABI")
set(CHARM_ARM_FPU "fpv5-d16" CACHE STRING "ARM FPU")

set(CMAKE_C_COMPILER   "${CHARM_ARM_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${CHARM_ARM_TRIPLE}-g++")
set(CMAKE_ASM_COMPILER "${CHARM_ARM_TRIPLE}-gcc")
set(CMAKE_AR           "${CHARM_ARM_TRIPLE}-ar")
set(CMAKE_OBJCOPY      "${CHARM_ARM_TRIPLE}-objcopy")
set(CMAKE_OBJDUMP      "${CHARM_ARM_TRIPLE}-objdump")
set(CMAKE_RANLIB       "${CHARM_ARM_TRIPLE}-ranlib")
set(CMAKE_STRIP        "${CHARM_ARM_TRIPLE}-strip")

set(CHARM_ARM_ARCH_FLAGS
    "-mcpu=${CHARM_ARM_CPU} -mthumb -mfloat-abi=${CHARM_ARM_FLOAT_ABI} -mfpu=${CHARM_ARM_FPU}")
set(CHARM_ARM_COMMON_FLAGS
    "${CHARM_ARM_ARCH_FLAGS} -ffreestanding -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit")

set(CMAKE_C_FLAGS_INIT "${CHARM_ARM_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CHARM_ARM_COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${CHARM_ARM_ARCH_FLAGS}")

set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib -nostartfiles")
