function(charm_escape_regex input output_var)
    set(tmp "${input}")
    string(REGEX REPLACE "([][.^$*+?()|{}\\\\])" "\\\\1" tmp "${tmp}")
    set(${output_var} "${tmp}" PARENT_SCOPE)
endfunction()

function(charm_check_banned_header files header)
    charm_escape_regex("${header}" header_regex)
    set(include_regex "(\\n|\\r|^)[ \\t]*#\\s*include\\s*<${header_regex}>")
    set(import_regex "(\\n|\\r|^)[ \\t]*import\\s*<${header_regex}>")
    foreach(path IN LISTS files)
        if (NOT EXISTS "${path}")
            continue()
        endif()
        file(READ "${path}" content)
        string(REGEX MATCH "${include_regex}" include_match "${content}")
        if (include_match)
            message(FATAL_ERROR "Banned header <${header}> used in ${path}")
        endif()
        string(REGEX MATCH "${import_regex}" import_match "${content}")
        if (import_match)
            message(FATAL_ERROR "Banned header <${header}> used in ${path}")
        endif()
    endforeach()
endfunction()

function(charm_enforce_banned_headers)
    if ("${CHARM_BANNED_HEADERS}" STREQUAL "")
        return()
    endif()

    set(target_files "")
    file(GLOB_RECURSE target_files CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.hpp")
    list(FILTER target_files EXCLUDE REGEX "[\\\\/]Modules/thirdparty[\\\\/]")

    foreach(exclude_regex IN LISTS CHARM_BANNED_HEADERS_EXCLUDE_REGEX)
        list(FILTER target_files EXCLUDE REGEX "${exclude_regex}")
    endforeach()

    if (CHARM_BANNED_HEADER_CHECK_EXAMPLES)
        file(GLOB_RECURSE example_files CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/Examples/*.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Examples/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/Examples/*.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/Examples/*.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/Examples/*.hpp")
        list(APPEND target_files ${example_files})
    endif()

    foreach(header IN LISTS CHARM_BANNED_HEADERS)
        if (NOT "${header}" STREQUAL "")
            charm_check_banned_header("${target_files}" "${header}")
        endif()
    endforeach()
endfunction()
