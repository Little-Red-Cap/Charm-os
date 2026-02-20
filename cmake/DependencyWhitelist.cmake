function(charm_check_imports files disallow_regex message)
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

function(charm_enforce_dependency_whitelist)
    set(foundation_files "")
    file(GLOB_RECURSE foundation_files CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/core/*.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/io/out/*.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/charm.foundation.cppm")

    set(runtime_files "")
    file(GLOB_RECURSE runtime_files CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/system/*.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/io/*.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/platform/*.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/charm.runtime.cppm")
    list(FILTER runtime_files EXCLUDE REGEX "/Modules/io/out/")

    set(import_runtime "(^|\\n)[ \\t]*(export[ \\t]+)?import[ \\t]+charm\\.runtime")
    set(import_domain "(^|\\n)[ \\t]*(export[ \\t]+)?import[ \\t]+charm\\.domain")

    charm_check_imports(
        "${foundation_files}"
        "${import_runtime}"
        "Foundation 不能 import charm.runtime")
    charm_check_imports(
        "${foundation_files}"
        "${import_domain}"
        "Foundation 不能 import charm.domain")
    charm_check_imports(
        "${runtime_files}"
        "${import_domain}"
        "Runtime 不能 import charm.domain")
endfunction()

