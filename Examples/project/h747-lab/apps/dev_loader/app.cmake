set(H747_LAB_APP_NAME dev_loader)
set(H747_LAB_APP_SOURCES
    "${H747_LAB_ROOT}/apps/dev_loader/dev_loader.cpp")
set(H747_LAB_APP_INCLUDE_DIRS
    "${H747_LAB_ROOT}/apps/dev_loader"
    "${CHARM_ROOT}/Examples/dev_loader"
    "${CHARM_ROOT}/Examples/app_abi")
set(H747_LAB_APP_COMPILE_DEFINITIONS
    H747_CONSOLE_TX_DMA_ENABLED=0)
