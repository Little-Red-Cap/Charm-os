set(PLAYER_RESOURCE_FONT_PATH "/font/gflex_variable.ttf" CACHE STRING
    "Charm Player default VFS font resource path")
set(PLAYER_RESOURCE_FONT_SMALL_PX "14" CACHE STRING
    "Charm Player default small font size")
set(PLAYER_RESOURCE_FONT_NORMAL_PX "18" CACHE STRING
    "Charm Player default normal font size")
set(PLAYER_RESOURCE_FONT_LARGE_PX "76" CACHE STRING
    "Charm Player default large font size")
if (NOT DEFINED PLAYER_HOST_STORAGE_VHD_PATH)
    set(_player_storage_vhd_default "")
    if (DEFINED ENV{PLAYER_HOST_STORAGE_VHD_PATH})
        file(TO_CMAKE_PATH "$ENV{PLAYER_HOST_STORAGE_VHD_PATH}" _player_storage_vhd_default)
    endif()
    set(PLAYER_HOST_STORAGE_VHD_PATH "${_player_storage_vhd_default}" CACHE FILEPATH
        "Charm Player legacy Windows storage VHD path")
    unset(_player_storage_vhd_default)
endif()
