include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/CharmSourceHelpers.cmake")

function(charm_collect_platform_sources out_modules out_base_dirs)
    charm_collect_cppm(_modules
        "${CHARM_SOURCE_ROOT}/Modules/platform/*.cppm"
    )

    if (NOT CHARM_TARGET_HAS_WIN32)
        list(FILTER _modules EXCLUDE REGEX "/Modules/platform/(boards/win_stub|win)/")
    endif()

    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_base_dirs} "${CHARM_SOURCE_ROOT}/Modules" PARENT_SCOPE)
endfunction()
