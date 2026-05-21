set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_EXECUTABLE_SUFFIX_ASM ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX ".elf")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(H747_LAB_TARGET_FLAGS
    -mcpu=cortex-m7
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    -mthumb)
string(REPLACE ";" " " H747_LAB_TARGET_FLAGS_STRING "${H747_LAB_TARGET_FLAGS}")

set(CMAKE_C_FLAGS_INIT "${H747_LAB_TARGET_FLAGS_STRING} -Wall -Wextra -Wpedantic -fdata-sections -ffunction-sections")
set(CMAKE_CXX_FLAGS_INIT "${H747_LAB_TARGET_FLAGS_STRING} -fno-rtti -fno-exceptions -fno-threadsafe-statics -Wall -Wextra -Wpedantic -fdata-sections -ffunction-sections")
set(CMAKE_ASM_FLAGS_INIT "${H747_LAB_TARGET_FLAGS_STRING} -x assembler-with-cpp -MMD -MP")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_ASM_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")
set(CMAKE_ASM_FLAGS_RELEASE "-Os -g0")

set(CMAKE_EXE_LINKER_FLAGS_INIT "")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "")
