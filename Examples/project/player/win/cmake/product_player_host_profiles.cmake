if (PLAYER_HOST_PROFILE STREQUAL "preview_full")
    set(PLAYER_HOST_PROFILE_STORAGE_DEFAULT ON)
    set(PLAYER_HOST_PROFILE_COVER_DECODE_DEFAULT ON)
    set(PLAYER_HOST_PROFILE_FILE_FONTS_DEFAULT ON)
    set(PLAYER_HOST_PROFILE_PLAYBACK_LOG_DEFAULT ON)
elseif (PLAYER_HOST_PROFILE STREQUAL "portability_probe")
    set(PLAYER_HOST_PROFILE_STORAGE_DEFAULT ON)
    set(PLAYER_HOST_PROFILE_COVER_DECODE_DEFAULT OFF)
    set(PLAYER_HOST_PROFILE_FILE_FONTS_DEFAULT OFF)
    set(PLAYER_HOST_PROFILE_PLAYBACK_LOG_DEFAULT ON)
else()
    message(FATAL_ERROR "Unsupported PLAYER_HOST_PROFILE: ${PLAYER_HOST_PROFILE}")
endif()

macro(player_host_feature_default name default_value doc)
    if (DEFINED CACHE{${name}})
        set(_player_host_feature_value "${${name}}")
        set(${name} "${_player_host_feature_value}" CACHE BOOL "${doc}" FORCE)
    elseif (NOT DEFINED ${name})
        set(${name} "${default_value}")
    endif()
endmacro()

player_host_feature_default(
    CHARM_PLAYER_HOST_STORAGE
    ${PLAYER_HOST_PROFILE_STORAGE_DEFAULT}
    "Enable host VHD storage defaults")
player_host_feature_default(
    CHARM_PLAYER_HOST_COVER_DECODE
    ${PLAYER_HOST_PROFILE_COVER_DECODE_DEFAULT}
    "Enable host-side embedded/file cover decoding")
player_host_feature_default(
    CHARM_PLAYER_HOST_FILE_FONTS
    ${PLAYER_HOST_PROFILE_FILE_FONTS_DEFAULT}
    "Enable host-side FreeType/VFS file fonts")
player_host_feature_default(
    CHARM_PLAYER_PLAYBACK_LOG
    ${PLAYER_HOST_PROFILE_PLAYBACK_LOG_DEFAULT}
    "Enable host playback diagnostics")
