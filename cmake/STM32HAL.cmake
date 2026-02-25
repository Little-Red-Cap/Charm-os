# STM32 HAL (series-agnostic wrapper for simple builds)

function(charm_add_stm32_hal target)
  set(options)
  set(oneValueArgs ROOT)
  cmake_parse_arguments(CHARM_HAL "${options}" "${oneValueArgs}" "" ${ARGN})

  if (NOT CHARM_HAL_ROOT)
    set(CHARM_HAL_ROOT "${CHARM_ROOT}/Modules/thirdparty/stm32")
  endif()

  set(_hal_inc "${CHARM_HAL_ROOT}/Inc")
  set(_hal_src "${CHARM_HAL_ROOT}/Src")

  if (NOT EXISTS "${_hal_src}")
    message(FATAL_ERROR "STM32 HAL Src not found: ${_hal_src}")
  endif()

  file(GLOB STM32_HAL_SOURCES CONFIGURE_DEPENDS "${_hal_src}/*.c")
  add_library(${target} OBJECT ${STM32_HAL_SOURCES})
  target_include_directories(${target} PUBLIC "${_hal_inc}")
endfunction()
