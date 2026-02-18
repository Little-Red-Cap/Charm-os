option(CHARM_USE_SYSTEM_ETL "Prefer system-installed ETL via find_package" ON)
option(CHARM_FETCHCONTENT_ETL "Fetch ETL via FetchContent when not found" ON)

set(CHARM_ETL_GIT_REPOSITORY "https://github.com/ETLCPP/etl.git" CACHE STRING "ETL git repository")
set(CHARM_ETL_GIT_TAG "master" CACHE STRING "ETL git tag")

if (NOT DEFINED CHARM_ETL_ROOT)
    set(CHARM_ETL_ROOT "${CMAKE_SOURCE_DIR}/Modules/thirdparty/etl")
endif()

function(charm_find_etl out_var)
    if (DEFINED ${out_var})
        return()
    endif()

    if (CHARM_USE_SYSTEM_ETL)
        find_package(ETL QUIET CONFIG)
    endif()

    if (ETL_FOUND)
        message(STATUS "Using system ETL")
        set(${out_var} "${ETL_INCLUDE_DIRS}" PARENT_SCOPE)
        return()
    endif()

    if (EXISTS "${CHARM_ETL_ROOT}/include/etl/etl.h")
        message(STATUS "Using local ETL headers: ${CHARM_ETL_ROOT}")
        set(${out_var} "${CHARM_ETL_ROOT}/include" PARENT_SCOPE)
        return()
    endif()

    if (EXISTS "${CHARM_ETL_ROOT}/etl/etl.h")
        message(STATUS "Using local ETL headers: ${CHARM_ETL_ROOT}")
        set(${out_var} "${CHARM_ETL_ROOT}" PARENT_SCOPE)
        return()
    endif()

    if (CHARM_FETCHCONTENT_ETL)
        include(FetchContent)
        message(STATUS "Fetching ETL via FetchContent")
        FetchContent_Declare(
            etl
            GIT_REPOSITORY ${CHARM_ETL_GIT_REPOSITORY}
            GIT_TAG ${CHARM_ETL_GIT_TAG}
        )
        FetchContent_MakeAvailable(etl)
        if (EXISTS "${etl_SOURCE_DIR}/include/etl/etl.h")
            set(${out_var} "${etl_SOURCE_DIR}/include" PARENT_SCOPE)
            return()
        endif()
        if (EXISTS "${etl_SOURCE_DIR}/etl/etl.h")
            set(${out_var} "${etl_SOURCE_DIR}" PARENT_SCOPE)
            return()
        endif()
        message(FATAL_ERROR "ETL headers not found in FetchContent source at ${etl_SOURCE_DIR}")
    endif()

    message(FATAL_ERROR
        "ETL not found. Set ETL_DIR for find_package, provide headers at "
        "${CHARM_ETL_ROOT} (or override CHARM_ETL_ROOT), "
        "or enable CHARM_FETCHCONTENT_ETL."
    )
endfunction()

function(charm_link_etl target)
    charm_find_etl(CHARM_ETL_INCLUDE)
    target_include_directories(${target} PRIVATE "${CHARM_ETL_INCLUDE}")
    target_compile_definitions(${target} PRIVATE CHARM_USE_ETL=1)
endfunction()
