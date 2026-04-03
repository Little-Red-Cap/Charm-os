set(MODULE_GROUP_BASE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_active.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_hqzy.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_caps.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_config.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_console.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_keys.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_sdmmc.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_usb.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_sdram.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/st7305.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/st7305.panels.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/st7305.transform.cppm"
)

set(MODULE_GROUP_USB_STORAGE_RUNTIME
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/usb_glue.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/usb_storage_bridge.cppm"
)

set(MODULE_GROUP_STORAGE_BOARD_MIN
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_active.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_hqzy.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_config.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_sdmmc.cppm"
)

set(MODULE_GROUP_HQZY_CM7_RUNTIME
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/boot_log.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/board_platform.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/foundation.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/runtime_bringup.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/sdmmc_glue.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/usb_glue.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/usb_storage_bridge.cppm"
)
