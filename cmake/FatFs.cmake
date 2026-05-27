if (NOT DEFINED CHARM_FATFS_ROOT)
    get_filename_component(CHARM_FATFS_ROOT
        "${CMAKE_CURRENT_LIST_DIR}/../Modules/thirdparty/fatfs"
        ABSOLUTE)
endif()

function(charm_find_fatfs out_include out_sources)
    if (DEFINED ${out_include} AND DEFINED ${out_sources})
        return()
    endif()

    set(_roots
        "${CHARM_FATFS_ROOT}"
        "${CHARM_FATFS_ROOT}/source"
        "${CHARM_FATFS_ROOT}/src"
    )

    foreach(root IN LISTS _roots)
        if (EXISTS "${root}/ff.c" AND EXISTS "${root}/ff.h")
            set(${out_include} "${root}" PARENT_SCOPE)
            set(${out_sources} "${root}/ff.c" PARENT_SCOPE)
            message(STATUS "Using local FatFs: ${root}")
            return()
        endif()
    endforeach()

    message(FATAL_ERROR
        "FatFs not found. Provide ff.c/ff.h under ${CHARM_FATFS_ROOT} "
        "(or override CHARM_FATFS_ROOT)."
    )
endfunction()

function(charm_link_fatfs target)
    charm_find_fatfs(CHARM_FATFS_INCLUDE CHARM_FATFS_SOURCES)
    set(_fatfs_sources "${CHARM_FATFS_SOURCES}")
    if (EXISTS "${CHARM_FATFS_INCLUDE}/ffunicode.c")
        list(APPEND _fatfs_sources "${CHARM_FATFS_INCLUDE}/ffunicode.c")
    endif()
    if (EXISTS "${CHARM_FATFS_INCLUDE}/ffsystem.c")
        list(APPEND _fatfs_sources "${CHARM_FATFS_INCLUDE}/ffsystem.c")
    endif()
    if (EXISTS "${CHARM_FATFS_INCLUDE}/option/cc936.c")
        list(APPEND _fatfs_sources "${CHARM_FATFS_INCLUDE}/option/cc936.c")
    elseif (EXISTS "${CHARM_FATFS_INCLUDE}/option/unicode.c")
        list(APPEND _fatfs_sources "${CHARM_FATFS_INCLUDE}/option/unicode.c")
    endif()
    target_sources(${target} PRIVATE ${_fatfs_sources})
    target_include_directories(${target} PRIVATE "${CHARM_FATFS_INCLUDE}")
    target_compile_definitions(${target} PRIVATE CHARM_USE_FATFS=1)
endfunction()
