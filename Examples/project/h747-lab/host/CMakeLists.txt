cmake_minimum_required(VERSION 4.0)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source cmake-build-* directory for h747-lab host.")
endif()

project(h747_lab_host LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(H747_LAB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_executable(h747_lab_host_display_raster_demo
    "${H747_LAB_ROOT}/host/display_raster_demo_host.cpp")

target_include_directories(h747_lab_host_display_raster_demo PRIVATE
    "${H747_LAB_ROOT}")

target_compile_options(h747_lab_host_display_raster_demo PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_test(NAME h747_lab_host_display_raster_demo_ci
    COMMAND $<TARGET_FILE:h747_lab_host_display_raster_demo> --ci)
set_tests_properties(h747_lab_host_display_raster_demo_ci PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[display-raster-host-ci\\] ok=1"
    FAIL_REGULAR_EXPRESSION "\\[display-raster-host-ci\\] ok=0")

add_executable(h747_lab_host_player
    "${H747_LAB_ROOT}/host/player_host.cpp")

target_include_directories(h747_lab_host_player PRIVATE
    "${H747_LAB_ROOT}")

target_compile_options(h747_lab_host_player PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_test(NAME h747_lab_host_player_ci
    COMMAND $<TARGET_FILE:h747_lab_host_player> --ci)
set_tests_properties(h747_lab_host_player_ci PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[player-host-ci\\] ok=1 input_ok=1 command_ok=1"
    FAIL_REGULAR_EXPRESSION "\\[player-host-ci\\] ok=0|input_ok=0|command_ok=0")

add_executable(h747_lab_host_profile_evidence
    "${H747_LAB_ROOT}/host/profile_evidence_host.cpp")

target_include_directories(h747_lab_host_profile_evidence PRIVATE
    "${H747_LAB_ROOT}")

target_compile_options(h747_lab_host_profile_evidence PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_test(NAME h747_lab_host_profile_evidence_ci
    COMMAND $<TARGET_FILE:h747_lab_host_profile_evidence>)
set_tests_properties(h747_lab_host_profile_evidence_ci PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[h747-host-profile-evidence-ci\\] ok=1")

add_executable(h747_lab_host_profile_compare
    "${H747_LAB_ROOT}/host/profile_compare_host.cpp")

target_include_directories(h747_lab_host_profile_compare PRIVATE
    "${H747_LAB_ROOT}")

target_compile_options(h747_lab_host_profile_compare PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_test(NAME h747_lab_host_profile_compare_ci
    COMMAND $<TARGET_FILE:h747_lab_host_profile_compare>)
set_tests_properties(h747_lab_host_profile_compare_ci PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[h747-host-profile-compare-ci\\] ok=1")

add_test(NAME h747_lab_host_profile_explain_ci
    COMMAND $<TARGET_FILE:h747_lab_host_profile_compare>)
set_tests_properties(h747_lab_host_profile_explain_ci PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[h747-host-profile-explain-ci\\] ok=1")

add_executable(h747_lab_host_app_profile_alignment
    "${H747_LAB_ROOT}/host/app_profile_alignment_host.cpp")

target_include_directories(h747_lab_host_app_profile_alignment PRIVATE
    "${H747_LAB_ROOT}")

target_compile_options(h747_lab_host_app_profile_alignment PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_test(NAME h747_lab_host_app_profile_alignment_ci
    COMMAND $<TARGET_FILE:h747_lab_host_app_profile_alignment>)
set_tests_properties(h747_lab_host_app_profile_alignment_ci PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[h747-host-app-profile-alignment-ci\\] ok=1")
