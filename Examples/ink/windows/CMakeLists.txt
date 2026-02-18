cmake_minimum_required(VERSION 4.0)
project(ink-example-win)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)



option(UI_SEM_TEST "Enable ui semantics tests" OFF)
if (UI_SEM_TEST)
    add_compile_definitions(UI_SEM_TEST=1)
endif()



add_executable(${target_name}
        main.cpp
#        ../demo/main.cpp
)

# Usage 1:  build source
#file(GLOB_RECURSE MODULE_INTERFACE_UNITS "../../Modules/*.cppm" "../../Examples/app/*.cppm" "./*.cppm")
#target_sources(${target_name}
#        PRIVATE
#        FILE_SET modules TYPE CXX_MODULES
#        BASE_DIRS
#            "${CMAKE_CURRENT_SOURCE_DIR}/../../"
#        FILES
#            ${MODULE_INTERFACE_UNITS}
#)


# Usage 2:  link lib
file(GLOB_RECURSE MODULE_INTERFACE_UNITS
        "../app/*.cppm"
        "./*.cppm"
)
set(FLIP_DEMO_CPPM "${CMAKE_CURRENT_SOURCE_DIR}/flip.demo.cppm")
list(REMOVE_ITEM MODULE_INTERFACE_UNITS "${FLIP_DEMO_CPPM}")
target_sources(${target_name}
        PRIVATE
        FILE_SET modules TYPE CXX_MODULES
        BASE_DIRS
            "${CMAKE_CURRENT_SOURCE_DIR}/../"
        FILES
            ${MODULE_INTERFACE_UNITS}
)
set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
target_link_libraries(${target_name} PRIVATE Charm-os)
target_compile_definitions(Charm-os PRIVATE OUT_ENABLE_FLOAT)




# ---- SDL3 options (MUST be set before add_subdirectory/FetchContent)
set(SDL_TEST OFF)
set(SDL_VIDEO ON)
set(SDL_AUDIO OFF)
set(SDL_GPU ON)
set(SDL_RENDER ON)
set(SDL_CAMERA OFF)
set(SDL_JOYSTICK OFF)
set(SDL_HAPTIC OFF)
set(SDL_POWER OFF)
set(SDL_SENSOR OFF)
set(SDL_DIALOG OFF)
set(SDL_TRAY OFF)

set(SDL_SHARED OFF)
set(SDL_STATIC ON)
set(SDL3_DISABLE_WINDOWS_MAIN ON)

include("${CHARM_ROOT}/cmake/SDL3.cmake")
charm_link_sdl3(${target_name})
