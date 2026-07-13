option(CHARM_USE_SYSTEM_SDL3 "Prefer system-installed SDL3 via find_package" ON)
option(CHARM_FETCHCONTENT_SDL3 "Fetch SDL3 via FetchContent when not found" ON)

set(CHARM_SDL3_GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git" CACHE STRING "SDL3 git repository")
set(CHARM_SDL3_GIT_TAG "release-3.2.8" CACHE STRING "Pinned SDL3 git tag")
set(CHARM_SDL3_SOURCE_DIR "" CACHE PATH "Explicit SDL3 source checkout")
get_filename_component(_CHARM_SDL3_BUNDLED_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../Examples/ThirdParty/SDL3" ABSOLUTE)

if (CHARM_SDL3_SOURCE_DIR STREQUAL ""
    AND DEFINED ENV{CHARM_SDL3_SOURCE_DIR}
    AND NOT "$ENV{CHARM_SDL3_SOURCE_DIR}" STREQUAL "")
    set(CHARM_SDL3_SOURCE_DIR "$ENV{CHARM_SDL3_SOURCE_DIR}" CACHE PATH
        "Explicit SDL3 source checkout" FORCE)
endif()

function(_charm_record_sdl3_resolution origin)
    if (DEFINED SDL3_VERSION AND NOT SDL3_VERSION STREQUAL "")
        set(_charm_sdl3_version "${SDL3_VERSION}")
    elseif (DEFINED SDL_VERSION AND NOT SDL_VERSION STREQUAL "")
        set(_charm_sdl3_version "${SDL_VERSION}")
    else()
        set(_charm_sdl3_version "unknown")
    endif()
    set(CHARM_SDL3_RESOLVED_ORIGIN "${origin}" CACHE INTERNAL
        "Resolved SDL3 dependency origin" FORCE)
    set(CHARM_SDL3_RESOLVED_VERSION "${_charm_sdl3_version}" CACHE INTERNAL
        "Resolved SDL3 dependency version" FORCE)
    set_property(GLOBAL PROPERTY CHARM_SDL3_RESOLUTION_RECORDED TRUE)
    message(STATUS
        "Charm SDL3: origin=${origin} version=${_charm_sdl3_version}")
endfunction()

function(charm_find_sdl3)
    if (TARGET SDL3::SDL3)
        get_property(_charm_sdl3_recorded GLOBAL
            PROPERTY CHARM_SDL3_RESOLUTION_RECORDED SET)
        if (NOT _charm_sdl3_recorded)
            _charm_record_sdl3_resolution("preconfigured-target")
        else()
            message(STATUS
                "Charm SDL3: origin=${CHARM_SDL3_RESOLVED_ORIGIN} "
                "version=${CHARM_SDL3_RESOLVED_VERSION}")
        endif()
        return()
    endif()

    if (NOT CHARM_SDL3_SOURCE_DIR STREQUAL "")
        get_filename_component(_charm_sdl3_source
            "${CHARM_SDL3_SOURCE_DIR}" ABSOLUTE)
        if (NOT EXISTS "${_charm_sdl3_source}/CMakeLists.txt")
            message(FATAL_ERROR
                "Explicit CHARM_SDL3_SOURCE_DIR is not an SDL3 source checkout: "
                "${_charm_sdl3_source}")
        endif()
        add_subdirectory("${_charm_sdl3_source}"
            "${CMAKE_BINARY_DIR}/_deps/charm-sdl3-source" EXCLUDE_FROM_ALL)
        if (NOT TARGET SDL3::SDL3)
            message(FATAL_ERROR
                "Explicit SDL3 source did not define SDL3::SDL3: ${_charm_sdl3_source}")
        endif()
        _charm_record_sdl3_resolution("source:${_charm_sdl3_source}")
        return()
    endif()

    if (CHARM_USE_SYSTEM_SDL3)
        find_package(SDL3 QUIET CONFIG)
    endif()

    if (SDL3_FOUND)
        _charm_record_sdl3_resolution("system-package")
        return()
    endif()

    set(_charm_sdl3_bundled_source "${_CHARM_SDL3_BUNDLED_SOURCE_DIR}")
    if (EXISTS "${_charm_sdl3_bundled_source}/CMakeLists.txt")
        add_subdirectory("${_charm_sdl3_bundled_source}"
            "${CMAKE_BINARY_DIR}/_deps/charm-sdl3-bundled" EXCLUDE_FROM_ALL)
        _charm_record_sdl3_resolution("bundled-source:${_charm_sdl3_bundled_source}")
        return()
    endif()

    if (CHARM_FETCHCONTENT_SDL3)
        if (DEFINED FETCHCONTENT_SOURCE_DIR_SDL3
            AND NOT FETCHCONTENT_SOURCE_DIR_SDL3 STREQUAL "")
            message(FATAL_ERROR
                "FETCHCONTENT_SOURCE_DIR_SDL3 cannot prove the pinned SDL3 revision. "
                "Use CHARM_SDL3_SOURCE_DIR for an explicit source checkout.")
        endif()
        if (FETCHCONTENT_FULLY_DISCONNECTED
            OR FETCHCONTENT_UPDATES_DISCONNECTED
            OR FETCHCONTENT_UPDATES_DISCONNECTED_SDL3)
            message(FATAL_ERROR
                "Disconnected FetchContent cannot prove the pinned SDL3 revision. "
                "Use CHARM_SDL3_SOURCE_DIR or a system package for offline builds.")
        endif()
        include(FetchContent)
        message(STATUS "Fetching SDL3 via FetchContent")
        FetchContent_Declare(
            SDL3
            GIT_REPOSITORY ${CHARM_SDL3_GIT_REPOSITORY}
            GIT_TAG ${CHARM_SDL3_GIT_TAG}
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(SDL3)
        FetchContent_GetProperties(SDL3 SOURCE_DIR _charm_sdl3_fetched_source)
        find_package(Git REQUIRED)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${_charm_sdl3_fetched_source}" rev-parse HEAD
            RESULT_VARIABLE _charm_sdl3_head_result
            OUTPUT_VARIABLE _charm_sdl3_head
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${_charm_sdl3_fetched_source}"
                rev-parse "${CHARM_SDL3_GIT_TAG}^{commit}"
            RESULT_VARIABLE _charm_sdl3_tag_result
            OUTPUT_VARIABLE _charm_sdl3_tag_commit
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if (NOT _charm_sdl3_head_result EQUAL 0
            OR NOT _charm_sdl3_tag_result EQUAL 0
            OR NOT _charm_sdl3_head STREQUAL _charm_sdl3_tag_commit)
            message(FATAL_ERROR
                "Fetched SDL3 revision does not match ${CHARM_SDL3_GIT_TAG}: "
                "head=${_charm_sdl3_head} tag=${_charm_sdl3_tag_commit}")
        endif()
        _charm_record_sdl3_resolution(
            "fetch:${CHARM_SDL3_GIT_TAG}@${_charm_sdl3_head}")
        return()
    endif()

    message(FATAL_ERROR
        "SDL3 not found. Set SDL3_DIR for find_package, set "
        "CHARM_SDL3_SOURCE_DIR to an SDL3 source checkout, or enable "
        "CHARM_FETCHCONTENT_SDL3."
    )
endfunction()

function(charm_link_sdl3 target)
    charm_find_sdl3()
    target_link_libraries(${target} PRIVATE SDL3::SDL3)
    if (TARGET Charm-media)
        get_target_property(_charm_sdl3_target_type Charm-media TYPE)
        if (_charm_sdl3_target_type STREQUAL "INTERFACE_LIBRARY")
            target_link_libraries(Charm-media INTERFACE SDL3::SDL3)
        else()
            target_link_libraries(Charm-media PRIVATE SDL3::SDL3)
        endif()
    elseif (TARGET Charm-os)
        get_target_property(_charm_sdl3_target_type Charm-os TYPE)
        if (_charm_sdl3_target_type STREQUAL "INTERFACE_LIBRARY")
            target_link_libraries(Charm-os INTERFACE SDL3::SDL3)
        else()
            target_link_libraries(Charm-os PRIVATE SDL3::SDL3)
        endif()
    endif()
endfunction()
