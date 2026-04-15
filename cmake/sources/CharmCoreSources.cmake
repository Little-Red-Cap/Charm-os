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

    if (NOT CHARM_TARGET_HAS_CXX_MATH)
        list(REMOVE_ITEM _modules
            "${CHARM_SOURCE_ROOT}/Modules/core/alg/alg_arc.cppm"
            "${CHARM_SOURCE_ROOT}/Modules/core/alg/alg_color.cppm"
            "${CHARM_SOURCE_ROOT}/Modules/core/alg/alg_color_extract.cppm"
            "${CHARM_SOURCE_ROOT}/Modules/core/alg/alg_fft.cppm"
            "${CHARM_SOURCE_ROOT}/Modules/core/alg/alg_filters.cppm"
            "${CHARM_SOURCE_ROOT}/Modules/core/alg/alg_round_rect.cppm")
    endif()

    if (NOT CHARM_TARGET_HAS_HOSTED_CXX)
        list(REMOVE_ITEM _modules
            "${CHARM_SOURCE_ROOT}/Modules/core/alg/alg_color_extract.cppm")
    endif()

    if (NOT CHARM_ENABLE_FREETYPE)
        list(REMOVE_ITEM _modules
            "${CHARM_SOURCE_ROOT}/Modules/gfx/font/font_provider_freetype.cppm")
    endif()

    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_base_dirs} "${CHARM_SOURCE_ROOT}/Modules" PARENT_SCOPE)
endfunction()
