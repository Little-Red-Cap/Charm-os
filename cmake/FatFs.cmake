option(CHARM_FETCHCONTENT_FATFS "Fetch FatFs via FetchContent when not found" OFF)

set(CHARM_FATFS_GIT_REPOSITORY "https://github.com/abbrev/fatfs.git" CACHE STRING "FatFs git repository")
set(CHARM_FATFS_GIT_TAG "master" CACHE STRING "FatFs git tag")

if (NOT DEFINED CHARM_FATFS_ROOT)
    set(CHARM_FATFS_ROOT "${CMAKE_SOURCE_DIR}/Modules/thirdparty/fatfs")
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

    if (CHARM_FETCHCONTENT_FATFS)
        include(FetchContent)
        message(STATUS "Fetching FatFs via FetchContent")
        FetchContent_Declare(
            fatfs
            GIT_REPOSITORY ${CHARM_FATFS_GIT_REPOSITORY}
            GIT_TAG ${CHARM_FATFS_GIT_TAG}
        )
        FetchContent_MakeAvailable(fatfs)
        if (EXISTS "${fatfs_SOURCE_DIR}/ff.c" AND EXISTS "${fatfs_SOURCE_DIR}/ff.h")
            set(${out_include} "${fatfs_SOURCE_DIR}" PARENT_SCOPE)
            set(${out_sources} "${fatfs_SOURCE_DIR}/ff.c" PARENT_SCOPE)
            message(STATUS "Using fetched FatFs: ${fatfs_SOURCE_DIR}")
            return()
        endif()
        if (EXISTS "${fatfs_SOURCE_DIR}/source/ff.c" AND EXISTS "${fatfs_SOURCE_DIR}/source/ff.h")
            set(${out_include} "${fatfs_SOURCE_DIR}/source" PARENT_SCOPE)
            set(${out_sources} "${fatfs_SOURCE_DIR}/source/ff.c" PARENT_SCOPE)
            message(STATUS "Using fetched FatFs: ${fatfs_SOURCE_DIR}/source")
            return()
        endif()
        if (EXISTS "${fatfs_SOURCE_DIR}/src/ff.c" AND EXISTS "${fatfs_SOURCE_DIR}/src/ff.h")
            set(${out_include} "${fatfs_SOURCE_DIR}/src" PARENT_SCOPE)
            set(${out_sources} "${fatfs_SOURCE_DIR}/src/ff.c" PARENT_SCOPE)
            message(STATUS "Using fetched FatFs: ${fatfs_SOURCE_DIR}/src")
            return()
        endif()
        if (EXISTS "${fatfs_SOURCE_DIR}/fatfs/ff.c" AND EXISTS "${fatfs_SOURCE_DIR}/fatfs/ff.h")
            set(${out_include} "${fatfs_SOURCE_DIR}/fatfs" PARENT_SCOPE)
            set(${out_sources} "${fatfs_SOURCE_DIR}/fatfs/ff.c" PARENT_SCOPE)
            message(STATUS "Using fetched FatFs: ${fatfs_SOURCE_DIR}/fatfs")
            return()
        endif()
        if (EXISTS "${fatfs_SOURCE_DIR}/fatfs/source/ff.c" AND EXISTS "${fatfs_SOURCE_DIR}/fatfs/source/ff.h")
            set(${out_include} "${fatfs_SOURCE_DIR}/fatfs/source" PARENT_SCOPE)
            set(${out_sources} "${fatfs_SOURCE_DIR}/fatfs/source/ff.c" PARENT_SCOPE)
            message(STATUS "Using fetched FatFs: ${fatfs_SOURCE_DIR}/fatfs/source")
            return()
        endif()
        if (EXISTS "${fatfs_SOURCE_DIR}/fatfs/src/ff.c" AND EXISTS "${fatfs_SOURCE_DIR}/fatfs/src/ff.h")
            set(${out_include} "${fatfs_SOURCE_DIR}/fatfs/src" PARENT_SCOPE)
            set(${out_sources} "${fatfs_SOURCE_DIR}/fatfs/src/ff.c" PARENT_SCOPE)
            message(STATUS "Using fetched FatFs: ${fatfs_SOURCE_DIR}/fatfs/src")
            return()
        endif()
        message(FATAL_ERROR "FatFs sources not found in FetchContent source at ${fatfs_SOURCE_DIR}")
    endif()

    message(FATAL_ERROR
        "FatFs not found. Provide ff.c/ff.h under ${CHARM_FATFS_ROOT} "
        "(or override CHARM_FATFS_ROOT), or enable CHARM_FETCHCONTENT_FATFS."
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
    target_sources(${target} PRIVATE ${_fatfs_sources})
    target_include_directories(${target} PRIVATE "${CHARM_FATFS_INCLUDE}")
    target_compile_definitions(${target} PRIVATE CHARM_USE_FATFS=1)
endfunction()
