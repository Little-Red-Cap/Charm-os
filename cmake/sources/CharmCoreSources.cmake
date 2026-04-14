include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/CharmSourceHelpers.cmake")

function(charm_collect_core_sources out_modules out_base_dirs)
    charm_collect_cppm(_modules
        "${CHARM_SOURCE_ROOT}/Modules/control/*.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/core/*.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/gfx/*.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/init/*.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/ui/vivid/gfx/color.cppm"
    )

    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_base_dirs} "${CHARM_SOURCE_ROOT}/Modules" PARENT_SCOPE)
endfunction()
