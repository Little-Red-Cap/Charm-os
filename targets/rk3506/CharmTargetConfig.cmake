include_guard(GLOBAL)

charm_get_source_root(_CHARM_RK3506_SOURCE_ROOT)
charm_get_target_config_name(_CHARM_RK3506_TARGET "rk3506")

set(CHARM_TARGET_HAS_HOSTED_CXX OFF CACHE BOOL "" FORCE)
set(CHARM_TARGET_HAS_CXX_MATH OFF CACHE BOOL "" FORCE)
set(CHARM_TARGET_HAS_WIN32 OFF CACHE BOOL "" FORCE)
set(CHARM_TARGET_HAS_WINSOCK OFF CACHE BOOL "" FORCE)
set(CHARM_TARGET_STRICT_ALIGNMENT ON CACHE BOOL "" FORCE)
set(CHARM_RK3506_SDRAM_BASE "0x00000000" CACHE STRING
    "RK3506 provisional SDRAM base used by the bare-metal skeleton")
set(CHARM_RK3506_SDRAM_SIZE "0x04000000" CACHE STRING
    "RK3506 provisional SDRAM span reserved for early bare-metal bring-up")
set(CHARM_RK3506_IMAGE_TEXT_BASE "0x00200000" CACHE STRING
    "RK3506 provisional bare-metal image text base")
set(CHARM_RK3506_IMAGE_RAM_LENGTH "0x00400000" CACHE STRING
    "RK3506 provisional bare-metal image span")
set(CHARM_RK3506_IMAGE_STACK_TOP "0x00600000" CACHE STRING
    "RK3506 provisional bare-metal image stack top")
set(CHARM_RK3506_UART0_BASE "0xff0a0000" CACHE STRING
    "RK3506 UART0 base address used by the bare-metal skeleton")
set(CHARM_RK3506_UART_REG_SHIFT "2" CACHE STRING
    "RK3506 UART register shift derived from DTS")
set(CHARM_RK3506_GICD_BASE "0xff581000" CACHE STRING
    "RK3506 GIC distributor base from public DTS/U-Boot sources")
set(CHARM_RK3506_GICC_BASE "0xff582000" CACHE STRING
    "RK3506 GIC CPU interface base from public DTS/U-Boot sources")
set(CHARM_RK3506_GENERIC_TIMER_FREQUENCY_HZ "24000000" CACHE STRING
    "RK3506 generic timer frequency in Hz from DTS/U-Boot sources")

if (NOT TARGET ${_CHARM_RK3506_TARGET})
    charm_add_config_interface(${_CHARM_RK3506_TARGET}
        ARCH armv7-a
        PLATFORM rk3506
        BAREMETAL
        COMPILE_DEFINITIONS
            CHARM_TARGET_SOC_RK3506=1
            CHARM_TARGET_CPU_CORTEX_A7=1
            CHARM_TARGET_HAS_GIC=1
            CHARM_TARGET_HAS_ARM_GENERIC_TIMER=1)
endif()

if (NOT TARGET Charm::target::rk3506)
    add_library(Charm::target::rk3506 ALIAS ${_CHARM_RK3506_TARGET})
endif()

charm_register_target_bootstrap(${_CHARM_RK3506_TARGET}
    PREFIX CHARM_RK3506
    SOURCES_FILE "${CMAKE_CURRENT_LIST_DIR}/sources.cmake"
    TARGET_NAME "rk3506-baremetal-skeleton"
    ARCH armv7-a)

set(CHARM_RK3506_TARGET_NAME "rk3506")
set(CHARM_RK3506_TARGET_CONFIG "${_CHARM_RK3506_TARGET}")
set(CHARM_RK3506_TOOLCHAIN_FILE
    "${_CHARM_RK3506_SOURCE_ROOT}/cmake/toolchains/armv7a-none-eabi-gcc.cmake")
set(CHARM_RK3506_SOURCES_FILE "${CMAKE_CURRENT_LIST_DIR}/sources.cmake")
