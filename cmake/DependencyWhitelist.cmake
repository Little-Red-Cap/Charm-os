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
    set(module_files "")
    file(GLOB_RECURSE module_files CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.cppm")

    set(import_removed "(^|\\n)[ \\t]*(export[ \\t]+)?import[ \\t]+charm\\.(foundation|runtime|domain)")

    charm_check_imports(
        "${module_files}"
        "${import_removed}"
        "Removed entry module imported")
endfunction()
