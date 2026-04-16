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
set(CHARM_RK3506_SYSTEM_SRAM_BASE "0xfff80000" CACHE STRING
    "RK3506 system SRAM base confirmed by the vendor HAL AMP image metadata")
set(CHARM_RK3506_SYSTEM_SRAM_SIZE "0x0000c000" CACHE STRING
    "RK3506 system SRAM span confirmed by the vendor HAL AMP image metadata")
set(CHARM_RK3506_SDRAM_SIZE "0x04000000" CACHE STRING
    "RK3506 provisional SDRAM span reserved for early bare-metal bring-up")
set(CHARM_RK3506_IMAGE_TEXT_BASE "0x00200000" CACHE STRING
    "RK3506 provisional bare-metal image text base")
set(CHARM_RK3506_IMAGE_RAM_LENGTH "0x00400000" CACHE STRING
    "RK3506 provisional bare-metal image span")
set(CHARM_RK3506_STACK_UND_SIZE "0x0400" CACHE STRING
    "RK3506 undefined-mode stack size")
set(CHARM_RK3506_STACK_ABT_SIZE "0x0400" CACHE STRING
    "RK3506 abort-mode stack size")
set(CHARM_RK3506_STACK_IRQ_SIZE "0x0400" CACHE STRING
    "RK3506 IRQ-mode stack size")
set(CHARM_RK3506_STACK_FIQ_SIZE "0x0400" CACHE STRING
    "RK3506 FIQ-mode stack size")
set(CHARM_RK3506_STACK_SVC_SIZE "0x0800" CACHE STRING
    "RK3506 supervisor-mode stack size")
set(CHARM_RK3506_STACK_SYS_SIZE "0x1000" CACHE STRING
    "RK3506 system-mode stack size")
unset(CHARM_RK3506_IMAGE_STACK_TOP CACHE)
set(CHARM_RK3506_UART0_BASE "0xff0a0000" CACHE STRING
    "RK3506 UART0 base address used by the bare-metal skeleton")
set(CHARM_RK3506_UART4_BASE "0xff0e0000" CACHE STRING
    "RK3506 UART4 base address used by the vendor AP HAL demo")
set(CHARM_RK3506_GPIO0_IOC_BASE "0xff950000" CACHE STRING
    "RK3506 GPIO0 IOC base used by the default UART0 pinmux path")
set(CHARM_RK3506_EARLY_UART_BASE "${CHARM_RK3506_UART0_BASE}" CACHE STRING
    "RK3506 early console UART base; defaults to UART0 but can be overridden per board")
set(CHARM_RK3506_EARLY_UART_BAUD_RATE "115200" CACHE STRING
    "RK3506 early console baud rate used by the bare-metal skeleton")
set(CHARM_RK3506_UART_REG_SHIFT "2" CACHE STRING
    "RK3506 UART register shift derived from DTS")
set(CHARM_RK3506_XIN_OSC_HZ "24000000" CACHE STRING
    "RK3506 external oscillator frequency used by the default UART0 clock path")
set(CHARM_RK3506_GICD_BASE "0xff581000" CACHE STRING
    "RK3506 GIC distributor base from public DTS/U-Boot sources")
set(CHARM_RK3506_GICC_BASE "0xff582000" CACHE STRING
    "RK3506 GIC CPU interface base from public DTS/U-Boot sources")
set(CHARM_RK3506_GRF_BASE "0xff288000" CACHE STRING
    "RK3506 main GRF base confirmed by public headers and vendor HAL")
set(CHARM_RK3506_GRF_PMU_BASE "0xff910000" CACHE STRING
    "RK3506 PMU GRF base confirmed by public headers and vendor HAL")
set(CHARM_RK3506_CRU_BASE "0xff9a0000" CACHE STRING
    "RK3506 CRU base confirmed by public headers and vendor HAL")
set(CHARM_RK3506_SCRU_BASE "0xff9a8000" CACHE STRING
    "RK3506 SCRU base confirmed by vendor HAL")
set(CHARM_RK3506_GENERIC_TIMER_FREQUENCY_HZ "24000000" CACHE STRING
    "RK3506 generic timer frequency in Hz from DTS/U-Boot sources")
set(CHARM_RK3506_GENERIC_TIMER_EXPECTED_INTID "30" CACHE STRING
    "RK3506 generic timer IRQ contract; default assumes post-DDR normal-world PL1 handoff uses the non-secure physical timer PPI")

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
