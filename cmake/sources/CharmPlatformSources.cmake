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

function(charm_collect_platform_support_sources out_sources out_include_dirs)
    set(_sources
        "${CHARM_SOURCE_ROOT}/targets/armv7a/common/armv7a_handoff_contract.cpp"
    )
    set(_include_dirs
        "${CHARM_SOURCE_ROOT}/targets/armv7a/common"
    )

    set(${out_sources} "${_sources}" PARENT_SCOPE)
    set(${out_include_dirs} "${_include_dirs}" PARENT_SCOPE)
endfunction()
