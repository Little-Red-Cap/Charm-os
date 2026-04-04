# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    CORE_CM7
    STM32H747xx
    CHARM_ENTRY_ALLOWED=1
    CHARM_KERNEL_REQUIRE_SSU_META=1
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Class/CompositeBuilder/Inc
)

# Add sources to executable
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/product_player_scenarios.cmake")

if (PLAYER_PROFILE_COMPILE_DEFINITIONS)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        ${PLAYER_PROFILE_COMPILE_DEFINITIONS})
endif()
