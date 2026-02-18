option(CHARM_USE_SYSTEM_SDL3 "Prefer system-installed SDL3 via find_package" ON)
option(CHARM_FETCHCONTENT_SDL3 "Fetch SDL3 via FetchContent when not found" ON)

set(CHARM_SDL3_GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git" CACHE STRING "SDL3 git repository")
set(CHARM_SDL3_GIT_TAG "main" CACHE STRING "SDL3 git tag")

if (NOT DEFINED CHARM_SDL3_SOURCE_DIR)
    set(CHARM_SDL3_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Examples/ThirdParty/SDL3")
endif()

function(charm_find_sdl3)
    if (TARGET SDL3::SDL3)
        return()
    endif()

    if (CHARM_USE_SYSTEM_SDL3)
        find_package(SDL3 QUIET CONFIG)
    endif()

    if (SDL3_FOUND)
        message(STATUS "Using system SDL3")
        return()
    endif()

    if (EXISTS "${CHARM_SDL3_SOURCE_DIR}/CMakeLists.txt")
        message(STATUS "Using local SDL3 source: ${CHARM_SDL3_SOURCE_DIR}")
        add_subdirectory("${CHARM_SDL3_SOURCE_DIR}" EXCLUDE_FROM_ALL)
        return()
    endif()

    if (CHARM_FETCHCONTENT_SDL3)
        include(FetchContent)
        message(STATUS "Fetching SDL3 via FetchContent")
        FetchContent_Declare(
            SDL3
            GIT_REPOSITORY ${CHARM_SDL3_GIT_REPOSITORY}
            GIT_TAG ${CHARM_SDL3_GIT_TAG}
        )
        FetchContent_MakeAvailable(SDL3)
        return()
    endif()

    message(FATAL_ERROR
        "SDL3 not found. Set SDL3_DIR for find_package, provide source at "
        "${CHARM_SDL3_SOURCE_DIR} (or override CHARM_SDL3_SOURCE_DIR), "
        "or enable CHARM_FETCHCONTENT_SDL3."
    )
endfunction()

function(charm_link_sdl3 target)
    charm_find_sdl3()
    target_link_libraries(${target} PRIVATE SDL3::SDL3)
    if (TARGET Charm-os)
        target_link_libraries(Charm-os PRIVATE SDL3::SDL3)
    endif()
endfunction()
