set(MODULE_GROUP_PRODUCT_BASE
    "${CMAKE_CURRENT_SOURCE_DIR}/app/debug_hooks.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_config.cppm"
)

set(MODULE_GROUP_PRODUCT_FS
    "${CMAKE_CURRENT_SOURCE_DIR}/app/fs_demo.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/fs_demo_mmc.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/fs_demo_sd.cppm"
)

set(MODULE_GROUP_PRODUCT_AUDIO
    "${CMAKE_CURRENT_SOURCE_DIR}/app/audio_mp3_demo.cppm"
)

set(MODULE_GROUP_PRODUCT_DISPLAY
    "${CMAKE_CURRENT_SOURCE_DIR}/app/display_st7305.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/ink_demo.cppm"
)

set(MODULE_GROUP_PRODUCT_PLAYER_UI
    "${CMAKE_CURRENT_SOURCE_DIR}/../../app-ink/hqzy/player_app_state.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../app-ink/hqzy/player_controller.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../app-ink/hqzy/player_fs_utils.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../app-ink/hqzy/player_ui_ink.cppm"
)

set(MODULE_GROUP_PRODUCT_LEGACY_RUNTIME
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_boot_debug.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_boot_fs.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_init_graph.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_post_bringup.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_pre_bringup.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_run_modes.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/usb_system.cppm"
)

set(MODULE_GROUP_PRODUCT_BUNDLES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bundles/hqzy_cm7_usb_storage_bundle.cppm"
)

set(MODULE_GROUP_PRODUCT_PROFILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../profiles/hqzy_cm7_usb_self_msc.cppm"
)

set(MODULE_GROUP_PRODUCT_PROFILE_SYSTEM
    "${CMAKE_CURRENT_SOURCE_DIR}/../../profiles/hqzy_cm7_usb_self_msc.system.cppm"
)
