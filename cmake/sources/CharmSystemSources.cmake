include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/CharmSourceHelpers.cmake")

function(charm_collect_system_sources out_modules out_base_dirs)
    charm_collect_cppm(_modules
        "${CHARM_SOURCE_ROOT}/Modules/system/*.cppm"
    )

    if (NOT CHARM_TARGET_HAS_WIN32)
        list(REMOVE_ITEM _modules
            "${CHARM_SOURCE_ROOT}/Modules/system/bringup/system_bringup_win_stub.cppm")
    endif()

    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_base_dirs} "${CHARM_SOURCE_ROOT}/Modules" PARENT_SCOPE)
endfunction()
