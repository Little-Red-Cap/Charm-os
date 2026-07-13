set(H747_LAB_APP_NAME capability_mvp)
set(H747_LAB_APP_PLATFORM_TIER foundation)
set(H747_LAB_APP_SOURCES
    "${H747_LAB_ROOT}/apps/capability_mvp/capability_mvp.cpp")
set(H747_LAB_APP_INCLUDE_DIRS
    "${H747_LAB_ROOT}/apps/capability_mvp"
    "${CHARM_ROOT}/Examples/system/charm_capability_mvp")
set(H747_LAB_APP_COMPILE_DEFINITIONS
    H747_LAB_FOUNDATION_PLATFORM=1
    H747_CONSOLE_TX_DMA_ENABLED=0)
