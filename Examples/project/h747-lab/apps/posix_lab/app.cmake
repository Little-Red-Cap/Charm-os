set(H747_LAB_APP_NAME posix_lab)
set(H747_LAB_APP_SOURCES
    "${H747_LAB_ROOT}/apps/posix_lab/posix_lab.cpp"
    "${H747_LAB_ROOT}/apps/posix_lab/elf_load_region.cpp")
set(H747_LAB_APP_INCLUDE_DIRS
    "${CMAKE_CURRENT_BINARY_DIR}/generated/posix_lab_elf_samples"
    "${H747_LAB_ROOT}/apps/posix_lab")
