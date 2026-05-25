set(H747_LAB_APP_NAME player_md3)
include("${H747_LAB_ROOT}/cmake/h747_lab_player_md3_manifest.cmake")
set(H747_LAB_APP_SOURCES
    "${H747_LAB_ROOT}/apps/player_md3/player_md3.cpp"
    "${H747_LAB_ROOT}/apps/player_md3/player_md3_runtime.cpp"
    "${H747_LAB_ROOT}/apps/player_md3/player_md3_input.cpp"
    "${H747_LAB_ROOT}/apps/player_md3/player_md3_memory.cpp"
    "${H747_LAB_ROOT}/apps/player_md3/player_md3_diag.cpp")
set(H747_LAB_APP_INCLUDE_DIRS
    "${H747_LAB_ROOT}/apps/player_md3"
    "${H747_LAB_ROOT}/apps/player"
    "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3")
h747_lab_collect_player_md3_modules(H747_LAB_APP_MODULE_SOURCES H747_LAB_APP_MODULE_BASE_DIRS)
set(H747_LAB_APP_COMPILE_DEFINITIONS
    CHARM_PLAYER_HOST_UI=0
    CHARM_PLAYER_HOST_STORAGE=0
    CHARM_PLAYER_HOST_COVER_DECODE=0
    CHARM_PLAYER_HOST_FILE_FONTS=0
    CHARM_PLAYER_MCU=1
    CHARM_PLAYER_DEBUG_UI=0
    CHARM_VIVID_UNSUPPORTED_WIDGET_DIAG=1
    CHARM_ENABLE_UI_VIVID=1
    CHARM_AUDIO_USE_VFS=1
    CHARM_AUDIO_ENABLE_MP3=0
    CHARM_AUDIO_ENABLE_FLAC=0
    CHARM_PLAYER_RESOURCE_FONT_SMALL_PX=14
    CHARM_PLAYER_RESOURCE_FONT_NORMAL_PX=18
    CHARM_PLAYER_RESOURCE_FONT_LARGE_PX=76)
set(H747_LAB_VIVID_FEATURESET FULL)
