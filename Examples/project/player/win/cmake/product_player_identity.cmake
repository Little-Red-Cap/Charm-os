set(PLAYER_PRODUCT "player" CACHE STRING
    "Charm product name for this build")
set(PLAYER_PLATFORM "windows-sdl3" CACHE STRING
    "Charm platform name for this build")
set(PLAYER_BOARD "win_stub" CACHE STRING
    "Charm board name for this build")
set(PLAYER_SCENARIO "ui_preview_ink" CACHE STRING
    "Charm scenario name for this build")

set_property(CACHE PLAYER_PRODUCT PROPERTY STRINGS
    player)
set_property(CACHE PLAYER_PLATFORM PROPERTY STRINGS
    windows-sdl3)
set_property(CACHE PLAYER_BOARD PROPERTY STRINGS
    win_stub)
set_property(CACHE PLAYER_SCENARIO PROPERTY STRINGS
    ui_preview_ink
    ui_preview_vivid
    ui_preview_vivid_md3)
