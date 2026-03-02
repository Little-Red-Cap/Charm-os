function(charm_check_banned_apis files disallow_regex message)
    foreach(path IN LISTS files)
        if (NOT EXISTS "${path}")
            continue()
        endif()
        file(READ "${path}" content)
        string(REGEX MATCH "${disallow_regex}" match "${content}")
        if (match)
            message(FATAL_ERROR "${message}: ${path}")
        endif()
    endforeach()
endfunction()

function(charm_enforce_banned_api_check)
    set(target_files "")
    file(GLOB_RECURSE target_files CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.hpp")
    list(FILTER target_files EXCLUDE REGEX "[\\\\/]Modules/thirdparty[\\\\/]")

    if (CHARM_BANNED_API_CHECK_EXAMPLES)
        file(GLOB_RECURSE example_files CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/Examples/*.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Examples/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/Examples/*.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/Examples/*.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/Examples/*.hpp")
        list(APPEND target_files ${example_files})
    endif()

    # Ban printf-family usage in project code. Use out.format/out.print instead.
    set(banned_regex "(^|[^A-Za-z0-9_:])((std::)|(::))?(printf|fprintf|vprintf|vfprintf|sprintf|vsprintf|snprintf|vsnprintf)[ \t]*\\(")
    charm_check_banned_apis(
        "${target_files}"
        "${banned_regex}"
        "Banned printf-family usage (use out.format/out.print instead)")
endfunction()
