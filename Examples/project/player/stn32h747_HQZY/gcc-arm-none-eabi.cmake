set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

# Some default GCC settings
# Prefer an explicit toolchain root so board builds do not depend on PATH
# accidentally resolving to an incomplete "latest" snapshot.
set(CHARM_ARM_NONE_EABI_ROOT "" CACHE PATH "Root path of the ARM bare-metal arm-none-eabi toolchain")

if(NOT CHARM_ARM_NONE_EABI_ROOT AND DEFINED ENV{CHARM_ARM_NONE_EABI_ROOT})
    set(CHARM_ARM_NONE_EABI_ROOT "$ENV{CHARM_ARM_NONE_EABI_ROOT}" CACHE PATH
        "Root path of the ARM bare-metal arm-none-eabi toolchain" FORCE)
endif()

if(NOT CHARM_ARM_NONE_EABI_ROOT AND DEFINED ENV{ARM_NONE_EABI_ROOT})
    set(CHARM_ARM_NONE_EABI_ROOT "$ENV{ARM_NONE_EABI_ROOT}" CACHE PATH
        "Root path of the ARM bare-metal arm-none-eabi toolchain" FORCE)
endif()

if(NOT CHARM_ARM_NONE_EABI_ROOT)
    foreach(_candidate
            "D:/Toolchains/Arm GNU Toolchain arm-none-eabi/15.2 rel1"
            "D:/Toolchains/Arm GNU Toolchain arm-none-eabi/14.2 rel1"
            "D:/Toolchains/Arm GNU Toolchain arm-none-eabi/13.2 Rel1")
        if(EXISTS "${_candidate}/bin/arm-none-eabi-gcc.exe")
            file(GLOB_RECURSE _candidate_crtbegin
                "${_candidate}/lib/gcc/arm-none-eabi/*/crtbegin.o")
            if(_candidate_crtbegin)
                set(CHARM_ARM_NONE_EABI_ROOT "${_candidate}" CACHE PATH
                    "Root path of the ARM bare-metal arm-none-eabi toolchain" FORCE)
                break()
            endif()
        endif()
    endforeach()
endif()

if(CHARM_ARM_NONE_EABI_ROOT)
    set(TOOLCHAIN_PREFIX            "${CHARM_ARM_NONE_EABI_ROOT}/bin/arm-none-eabi-")
else()
    # Fallback for portable environments that intentionally manage PATH.
    set(TOOLCHAIN_PREFIX            arm-none-eabi-)
endif()

set(_TOOLCHAIN_EXE_SUFFIX "")
if(CMAKE_HOST_WIN32)
    set(_TOOLCHAIN_EXE_SUFFIX ".exe")
endif()

set(CMAKE_C_COMPILER                "${TOOLCHAIN_PREFIX}gcc${_TOOLCHAIN_EXE_SUFFIX}")
set(CMAKE_ASM_COMPILER              "${CMAKE_C_COMPILER}")
set(CMAKE_CXX_COMPILER              "${TOOLCHAIN_PREFIX}g++${_TOOLCHAIN_EXE_SUFFIX}")
set(CMAKE_LINKER                    "${TOOLCHAIN_PREFIX}g++${_TOOLCHAIN_EXE_SUFFIX}")
set(CMAKE_AR                        "${TOOLCHAIN_PREFIX}ar${_TOOLCHAIN_EXE_SUFFIX}")
set(CMAKE_RANLIB                    "${TOOLCHAIN_PREFIX}ranlib${_TOOLCHAIN_EXE_SUFFIX}")
set(CMAKE_NM                        "${TOOLCHAIN_PREFIX}nm${_TOOLCHAIN_EXE_SUFFIX}")
set(CMAKE_OBJDUMP                   "${TOOLCHAIN_PREFIX}objdump${_TOOLCHAIN_EXE_SUFFIX}")
set(CMAKE_OBJCOPY                   "${TOOLCHAIN_PREFIX}objcopy${_TOOLCHAIN_EXE_SUFFIX}")
set(CMAKE_SIZE                      "${TOOLCHAIN_PREFIX}size${_TOOLCHAIN_EXE_SUFFIX}")

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX     ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_FLAGS "${STM32_MCU_FLAGS}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -fdata-sections -ffunction-sections")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")

set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")

set(CMAKE_C_LINK_FLAGS "${TARGET_FLAGS}")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -T \"${CMAKE_SOURCE_DIR}/${STM32_LINKER_SCRIPT}\"")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} --specs=nano.specs")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--start-group -lc -lm -Wl,--end-group")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--print-memory-usage")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} ${STM32_LINKER_OPTION}")
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--start-group -lstdc++ -lsupc++ -Wl,--end-group")
