set(_charm_freetype_repo_dir "${CMAKE_CURRENT_LIST_DIR}/../Modules/thirdparty/freetype")

function(_charm_is_freetype_source_dir path out_var)
    if (NOT path OR path STREQUAL "")
        set(${out_var} FALSE PARENT_SCOPE)
        return()
    endif()

    file(TO_CMAKE_PATH "${path}" _charm_freetype_path)
    if (EXISTS "${_charm_freetype_path}/CMakeLists.txt"
        AND EXISTS "${_charm_freetype_path}/include/ft2build.h")
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(_charm_detect_freetype_dir out_var)
    set(_candidates)

    if (DEFINED CHARM_FREETYPE_DIR AND NOT CHARM_FREETYPE_DIR STREQUAL "")
        list(APPEND _candidates "${CHARM_FREETYPE_DIR}")
    endif()
    if (DEFINED ENV{CHARM_FREETYPE_DIR} AND NOT "$ENV{CHARM_FREETYPE_DIR}" STREQUAL "")
        list(APPEND _candidates "$ENV{CHARM_FREETYPE_DIR}")
    endif()
    if (DEFINED ENV{FREETYPE_DIR} AND NOT "$ENV{FREETYPE_DIR}" STREQUAL "")
        list(APPEND _candidates "$ENV{FREETYPE_DIR}")
    endif()

    list(APPEND _candidates
        "${_charm_freetype_repo_dir}"
        "${_charm_freetype_repo_dir}/freetype2")

    # Prefer nearby full FreeType checkouts before falling back to Cargo cache copies.
    # Some host builds keep third-party sources outside the repo tree, and those copies
    # are more reliable than the freetype-sys snapshot for our current Windows flow.
    set(_nearby_roots
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/.."
        "${CMAKE_CURRENT_LIST_DIR}/../.."
        "${CMAKE_CURRENT_LIST_DIR}/../../.."
        "${CMAKE_CURRENT_LIST_DIR}/../../../..")
    foreach(_root IN LISTS _nearby_roots)
        file(TO_CMAKE_PATH "${_root}" _root_norm)
        list(APPEND _candidates
            "${_root_norm}/Third_Party/freetype"
            "${_root_norm}/third_party/freetype"
            "${_root_norm}/thirdparty/freetype")
    endforeach()

    set(_cargo_roots)
    if (DEFINED ENV{CARGO_HOME} AND NOT "$ENV{CARGO_HOME}" STREQUAL "")
        list(APPEND _cargo_roots "$ENV{CARGO_HOME}")
    endif()
    if (DEFINED ENV{USERPROFILE} AND NOT "$ENV{USERPROFILE}" STREQUAL "")
        list(APPEND _cargo_roots "$ENV{USERPROFILE}/.cargo")
    endif()
    if (DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
        list(APPEND _cargo_roots "$ENV{HOME}/.cargo")
    endif()
    list(REMOVE_DUPLICATES _cargo_roots)

    foreach(_cargo_root IN LISTS _cargo_roots)
        file(TO_CMAKE_PATH "${_cargo_root}" _cargo_root_norm)
        if (NOT EXISTS "${_cargo_root_norm}/registry/src")
            continue()
        endif()
        file(GLOB _cargo_candidates LIST_DIRECTORIES true
            "${_cargo_root_norm}/registry/src/*/freetype-sys-*/freetype2")
        if (_cargo_candidates)
            list(SORT _cargo_candidates COMPARE NATURAL ORDER DESCENDING)
            list(APPEND _candidates ${_cargo_candidates})
        endif()
    endforeach()

    list(REMOVE_DUPLICATES _candidates)
    foreach(_candidate IN LISTS _candidates)
        _charm_is_freetype_source_dir("${_candidate}" _is_freetype_dir)
        if (_is_freetype_dir)
            file(TO_CMAKE_PATH "${_candidate}" _candidate_norm)
            set(${out_var} "${_candidate_norm}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set(${out_var} "" PARENT_SCOPE)
endfunction()

function(_charm_add_freetype_subdirectory source_dir binary_dir)
    set(_had_policy_minimum FALSE)
    if (DEFINED CMAKE_POLICY_VERSION_MINIMUM)
        set(_had_policy_minimum TRUE)
        set(_previous_policy_minimum "${CMAKE_POLICY_VERSION_MINIMUM}")
    endif()

    if (NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM
        OR CMAKE_POLICY_VERSION_MINIMUM VERSION_LESS "3.10")
        set(CMAKE_POLICY_VERSION_MINIMUM "3.10")
    endif()

    add_subdirectory("${source_dir}" "${binary_dir}")

    if (_had_policy_minimum)
        set(CMAKE_POLICY_VERSION_MINIMUM "${_previous_policy_minimum}")
    else()
        unset(CMAKE_POLICY_VERSION_MINIMUM)
    endif()
endfunction()

if (DEFINED CHARM_FREETYPE_DIR AND NOT CHARM_FREETYPE_DIR STREQUAL "")
    file(TO_CMAKE_PATH "${CHARM_FREETYPE_DIR}" _charm_freetype_requested_dir)
else()
    set(_charm_freetype_requested_dir "")
endif()

_charm_detect_freetype_dir(_charm_freetype_detected_dir)

if (_charm_freetype_detected_dir)
    if (_charm_freetype_requested_dir
        AND NOT _charm_freetype_requested_dir STREQUAL _charm_freetype_detected_dir)
        message(STATUS
            "FreeType requested path unavailable, falling back to ${_charm_freetype_detected_dir}")
    elseif (NOT _charm_freetype_requested_dir)
        message(STATUS "Using detected FreeType source: ${_charm_freetype_detected_dir}")
    endif()
    set(CHARM_FREETYPE_DIR "${_charm_freetype_detected_dir}"
        CACHE PATH "Path to the FreeType source tree" FORCE)
else()
    set(CHARM_FREETYPE_DIR "${_charm_freetype_repo_dir}"
        CACHE PATH "Path to the FreeType source tree" FORCE)
endif()

function(charm_link_freetype target_name)
    if (TARGET freetype)
        target_link_libraries(${target_name} PRIVATE freetype)
        target_include_directories(${target_name} PRIVATE ${CHARM_FREETYPE_DIR}/include)
        target_compile_definitions(${target_name} PRIVATE CHARM_ENABLE_FREETYPE=1)
        return()
    endif()

    if (EXISTS "${CHARM_FREETYPE_DIR}/CMakeLists.txt")
        set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
        _charm_add_freetype_subdirectory(${CHARM_FREETYPE_DIR} ${CMAKE_BINARY_DIR}/freetype)
        if (WIN32)
            get_target_property(_charm_freetype_sources freetype SOURCES)
            if (_charm_freetype_sources)
                list(REMOVE_ITEM _charm_freetype_sources
                    "src/base/ftver.rc"
                    "${CHARM_FREETYPE_DIR}/src/base/ftver.rc")
                set_property(TARGET freetype PROPERTY SOURCES "${_charm_freetype_sources}")
            endif()
        endif()
        target_compile_definitions(freetype PRIVATE CHARM_LIB_BUILD=1)
        target_link_libraries(${target_name} PRIVATE freetype)
        target_include_directories(${target_name} PRIVATE ${CHARM_FREETYPE_DIR}/include)
        target_compile_definitions(${target_name} PRIVATE CHARM_ENABLE_FREETYPE=1)
    else()
        message(WARNING "FreeType requested but not found under ${CHARM_FREETYPE_DIR}")
    endif()
endfunction()
