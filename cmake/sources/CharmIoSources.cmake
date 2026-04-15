include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/CharmSourceHelpers.cmake")

function(charm_collect_io_sources out_modules out_base_dirs)
    charm_collect_cppm(_modules
        "${CHARM_SOURCE_ROOT}/Modules/io/*.cppm"
    )

    if (NOT CHARM_TARGET_HAS_WINSOCK)
        list(REMOVE_ITEM _modules
            "${CHARM_SOURCE_ROOT}/Modules/io/net/net.backend.win.cppm")
    endif()

    if (NOT CHARM_TARGET_HAS_WIN32)
        list(REMOVE_ITEM _modules
            "${CHARM_SOURCE_ROOT}/Modules/io/hal/hal_win.cppm")
    endif()

    if (NOT CHARM_TARGET_HAS_HOSTED_CXX)
        list(FILTER _modules EXCLUDE REGEX "/Modules/io/usb/mock/")
    endif()

    if (NOT CHARM_ENABLE_POSIX)
        list(FILTER _modules EXCLUDE REGEX "/Modules/io/posix/")
        list(REMOVE_ITEM _modules
            "${CHARM_SOURCE_ROOT}/Modules/io/net/net.posix.cppm")
    endif()

    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_base_dirs} "${CHARM_SOURCE_ROOT}/Modules" PARENT_SCOPE)
endfunction()
