set(H747_LAB_APP_NAME app_lab)
set(H747_LAB_APP_SOURCES
    "${H747_LAB_ROOT}/apps/app_lab/app_lab.cpp"
    "${H747_LAB_ROOT}/apps/app_lab/elf_load_region.cpp")
set(H747_LAB_APP_INCLUDE_DIRS
    "${CMAKE_CURRENT_BINARY_DIR}/generated/app_lab_elf_samples"
    "${H747_LAB_ROOT}/apps/app_lab"
    "${CHARM_ROOT}/Examples/app_abi")
