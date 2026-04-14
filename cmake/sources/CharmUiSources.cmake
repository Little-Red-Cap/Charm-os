include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/CharmSourceHelpers.cmake")

function(charm_collect_ui_sources target out_modules out_base_dirs)
    set(_modules)
    set(_base_dirs "${CHARM_SOURCE_ROOT}/Modules")

    if (CHARM_ENABLE_UI_INK OR CHARM_ENABLE_UI_VIVID)
        charm_collect_cppm(_common_modules
            "${CHARM_SOURCE_ROOT}/Modules/ui/common/*.cppm"
        )
        list(APPEND _modules ${_common_modules})
    endif()

    if (CHARM_ENABLE_UI_INK)
        charm_collect_cppm(_ink_modules
            "${CHARM_SOURCE_ROOT}/Modules/ui/ink/*.cppm"
        )
        list(APPEND _modules ${_ink_modules})
    endif()

    if (CHARM_ENABLE_UI_VIVID)
        charm_collect_cppm(_vivid_modules
            "${CHARM_SOURCE_ROOT}/Modules/ui/vivid/*.cppm"
        )
        list(REMOVE_ITEM _vivid_modules
            "${CHARM_SOURCE_ROOT}/Modules/ui/vivid/gfx/color.cppm")
        list(APPEND _modules ${_vivid_modules})

        include("${CHARM_SOURCE_ROOT}/Modules/ui/vivid/vivid.cmake")
        vivid_collect_modules(${target} _modules _base_dirs)
    endif()

    list(REMOVE_DUPLICATES _modules)
    list(REMOVE_DUPLICATES _base_dirs)
    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_base_dirs} "${_base_dirs}" PARENT_SCOPE)
endfunction()
