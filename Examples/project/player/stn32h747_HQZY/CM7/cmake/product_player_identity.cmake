set(PLAYER_PRODUCT "player" CACHE STRING
    "Charm product name for this build")
set(PLAYER_PLATFORM "stm32h747-hal" CACHE STRING
    "Charm platform name for this build")
set(PLAYER_BOARD "hqzy_cm7" CACHE STRING
    "Charm board name for this build")
set(PLAYER_SCENARIO "usb_self_msc" CACHE STRING
    "Charm scenario name for this build")

set_property(CACHE PLAYER_PRODUCT PROPERTY STRINGS
    player)
set_property(CACHE PLAYER_PLATFORM PROPERTY STRINGS
    stm32h747-hal)
set_property(CACHE PLAYER_BOARD PROPERTY STRINGS
    hqzy_cm7)
set_property(CACHE PLAYER_SCENARIO PROPERTY STRINGS
    legacy_main
    usb_as
    usb_audio
    usb_self_cdc
    usb_self_msc
    usb_storage)

set(PLAYER_PROFILE "USB_SELF_MSC" CACHE STRING
    "Player startup profile for stm32h747 HQZY CM7")
set_property(CACHE PLAYER_PROFILE PROPERTY STRINGS
    LEGACY_MAIN
    USB_AS
    USB_AUDIO
    USB_SELF_CDC
    USB_SELF_MSC
    USB_STORAGE)

if (DEFINED PLAYER_PROFILE AND NOT PLAYER_PROFILE STREQUAL "USB_SELF_MSC")
    string(TOLOWER "${PLAYER_PROFILE}" _player_profile_lower)
    set(PLAYER_SCENARIO "${_player_profile_lower}" CACHE STRING
        "Charm scenario name for this build" FORCE)
endif()

if (PLAYER_SCENARIO STREQUAL "legacy_main")
    set(PLAYER_PROFILE "LEGACY_MAIN" CACHE STRING "Player startup profile for stm32h747 HQZY CM7" FORCE)
elseif (PLAYER_SCENARIO STREQUAL "usb_as")
    set(PLAYER_PROFILE "USB_AS" CACHE STRING "Player startup profile for stm32h747 HQZY CM7" FORCE)
elseif (PLAYER_SCENARIO STREQUAL "usb_audio")
    set(PLAYER_PROFILE "USB_AUDIO" CACHE STRING "Player startup profile for stm32h747 HQZY CM7" FORCE)
elseif (PLAYER_SCENARIO STREQUAL "usb_self_cdc")
    set(PLAYER_PROFILE "USB_SELF_CDC" CACHE STRING "Player startup profile for stm32h747 HQZY CM7" FORCE)
elseif (PLAYER_SCENARIO STREQUAL "usb_self_msc")
    set(PLAYER_PROFILE "USB_SELF_MSC" CACHE STRING "Player startup profile for stm32h747 HQZY CM7" FORCE)
elseif (PLAYER_SCENARIO STREQUAL "usb_storage")
    set(PLAYER_PROFILE "USB_STORAGE" CACHE STRING "Player startup profile for stm32h747 HQZY CM7" FORCE)
else()
    message(FATAL_ERROR "Unsupported PLAYER_SCENARIO: ${PLAYER_SCENARIO}")
endif()
