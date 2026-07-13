# Canonical Player MD3 source ownership. Callers must define CHARM_ROOT.

set(PLAYER_MD3_APPLICATION_MODULES
    "${CHARM_ROOT}/Examples/project/player/app-common/player.app.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.app_config.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.cover_resource.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.fixed_string.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.font_cache.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.font_resource.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.font_resource_apply.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.fs_utils.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.host_features.cppm" # player.product_policy
    "${CHARM_ROOT}/Examples/project/player/app-common/player.input.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.lyrics.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.media_library.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.media_scan.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.playback.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.playback_session.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.product_config.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.recent_history.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.scene_runtime.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.stats_history.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.storage.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.time_utils.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.track_probe.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.controller.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.cover.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.cover_theme.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.ui.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.ui_builder.cppm"
)

if (CHARM_PLAYER_DEBUG_UI)
    list(APPEND PLAYER_MD3_APPLICATION_MODULES
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.ui_debug.cppm")
endif()

set(PLAYER_MD3_RENDER_RUNTIME_MODULES
    "${CHARM_ROOT}/Examples/project/player/app-common/player.render_runtime.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.md3_runtime.cppm"
)

set(PLAYER_PORT_MODULES
    "${CHARM_ROOT}/Examples/project/player/app-common/player.port.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.port_runtime.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-common/player.raster.cppm"
)

set(PLAYER_MD3_PORT_MATERIALIZER_MODULES
    "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.md3_port.cppm"
)

set(PLAYER_MD3_CANONICAL_MODULES
    ${PLAYER_MD3_APPLICATION_MODULES}
    ${PLAYER_MD3_RENDER_RUNTIME_MODULES}
    ${PLAYER_PORT_MODULES}
    ${PLAYER_MD3_PORT_MATERIALIZER_MODULES}
)

file(GLOB PLAYER_MD3_INCLUDED_IMPLEMENTATION_FILES
    CONFIGURE_DEPENDS
    "${CHARM_ROOT}/Examples/project/player/app-common/*.inc"
    "${CHARM_ROOT}/Examples/project/player/app-common/*.tmp"
    "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/*.inc"
    "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/*.tmp")

set(PLAYER_MD3_CANONICAL_CONTRACT_FILES
    ${PLAYER_MD3_CANONICAL_MODULES}
    ${PLAYER_MD3_INCLUDED_IMPLEMENTATION_FILES}
    "${CHARM_ROOT}/Examples/project/player/app-common/player.product_policy.hpp")

set(PLAYER_MD3_FORBIDDEN_PLATFORM_PATTERN
    "SDL|_WIN32|Win32|win32|H747|h747|QEMU|qemu|STM32|stm32|CMSIS|cmsis|FreeRTOS|Zephyr|zephyr|HAL_|windows[.]h|unistd[.]h|pthread[.]h|sys/|platform[.]win|player[.](board_|mcu_policy|runtime_shell|win)|import[ \t]+player[.](display|platform|runtime)[ \t]*;|CHARM_PLAYER_(MCU|HOST|BOARD|PLATFORM)|host_features::|ClockCaps::TimeSource")

foreach(_player_md3_source IN LISTS PLAYER_MD3_CANONICAL_CONTRACT_FILES)
    if (NOT EXISTS "${_player_md3_source}")
        message(FATAL_ERROR "Player MD3 canonical source missing: ${_player_md3_source}")
    endif()
    if (_player_md3_source MATCHES "/player[.](display|platform|runtime)[.]cppm$")
        message(FATAL_ERROR
            "Player MD3 canonical source includes a legacy platform seam: ${_player_md3_source}")
    endif()
    file(READ "${_player_md3_source}" _player_md3_source_text)
    if (_player_md3_source_text MATCHES "${PLAYER_MD3_FORBIDDEN_PLATFORM_PATTERN}")
        message(FATAL_ERROR
            "Player MD3 canonical source contains a platform leak: ${_player_md3_source}")
    endif()
endforeach()

unset(_player_md3_source)
unset(_player_md3_source_text)
