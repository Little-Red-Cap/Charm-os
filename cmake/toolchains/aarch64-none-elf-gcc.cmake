set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CHARM_AARCH64_TRIPLE "aarch64-none-elf" CACHE STRING "AArch64 bare-metal toolchain triple")
set(CHARM_AARCH64_CPU "cortex-a53" CACHE STRING "AArch64 CPU for bare-metal builds")

set(CMAKE_C_COMPILER   "${CHARM_AARCH64_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${CHARM_AARCH64_TRIPLE}-g++")
set(CMAKE_ASM_COMPILER "${CHARM_AARCH64_TRIPLE}-gcc")
set(CMAKE_AR           "${CHARM_AARCH64_TRIPLE}-ar")
set(CMAKE_OBJCOPY      "${CHARM_AARCH64_TRIPLE}-objcopy")
set(CMAKE_OBJDUMP      "${CHARM_AARCH64_TRIPLE}-objdump")
set(CMAKE_RANLIB       "${CHARM_AARCH64_TRIPLE}-ranlib")
set(CMAKE_STRIP        "${CHARM_AARCH64_TRIPLE}-strip")

set(CHARM_AARCH64_COMMON_FLAGS
    "-mcpu=${CHARM_AARCH64_CPU} -ffreestanding -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit")

set(CMAKE_C_FLAGS_INIT "${CHARM_AARCH64_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CHARM_AARCH64_COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "-mcpu=${CHARM_AARCH64_CPU}")

set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib -nostartfiles")
