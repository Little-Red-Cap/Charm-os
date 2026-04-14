include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/CharmSourceHelpers.cmake")

function(charm_collect_platform_sources out_modules out_base_dirs)
    charm_collect_cppm(_modules
        "${CHARM_SOURCE_ROOT}/Modules/platform/*.cppm"
    )

    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_base_dirs} "${CHARM_SOURCE_ROOT}/Modules" PARENT_SCOPE)
endfunction()
