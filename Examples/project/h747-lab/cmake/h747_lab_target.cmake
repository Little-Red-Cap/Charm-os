include_guard(GLOBAL)

function(h747_lab_collect_services out_sources out_include_dirs)
    set(_sources)
    set(_include_dirs)
    foreach(_service IN LISTS ARGN)
        if(NOT DEFINED H747_LAB_SERVICE_${_service}_SOURCES)
            message(FATAL_ERROR "Unknown h747-lab service '${_service}'")
        endif()
        list(APPEND _sources ${H747_LAB_SERVICE_${_service}_SOURCES})
        list(APPEND _include_dirs ${H747_LAB_SERVICE_${_service}_INCLUDE_DIRS})
    endforeach()
    list(REMOVE_DUPLICATES _sources)
    list(REMOVE_DUPLICATES _include_dirs)
    set(${out_sources} ${_sources} PARENT_SCOPE)
    set(${out_include_dirs} ${_include_dirs} PARENT_SCOPE)
endfunction()

function(h747_lab_check_vivid_product_modules target_name)
    foreach(_module IN LISTS ARGN)
        cmake_path(NORMAL_PATH _module OUTPUT_VARIABLE _module_norm)
        if(_module_norm MATCHES "/Modules/ui/vivid/gfx/snapshot\\.cppm$")
            message(FATAL_ERROR
                "${target_name}: PRODUCT Vivid module set must not include charm.gfx.snapshot")
        endif()
        if(_module_norm MATCHES "/Modules/ui/vivid/gfx/display_policy\\.cppm$")
            message(FATAL_ERROR
                "${target_name}: PRODUCT Vivid module set must not include charm.gfx.display_policy")
        endif()
        if(NOT _module_norm MATCHES "/Modules/ui/vivid/")
            continue()
        endif()
        if(NOT EXISTS "${_module_norm}")
            continue()
        endif()

        file(READ "${_module_norm}" _module_text)
        set(_hits)
        if(_module_text MATCHES "std::vector")
            list(APPEND _hits "std::vector")
        endif()
        if(_module_text MATCHES "std::string[^_]")
            list(APPEND _hits "std::string")
        endif()
        if(_module_text MATCHES "(^|[^A-Za-z0-9_])throw([^A-Za-z0-9_]|$)")
            list(APPEND _hits "throw")
        endif()
        if(_module_text MATCHES "(^|[^A-Za-z0-9_])try([^A-Za-z0-9_]|$)")
            list(APPEND _hits "try")
        endif()
        if(_module_text MATCHES "(^|[^A-Za-z0-9_])catch([^A-Za-z0-9_]|$)")
            list(APPEND _hits "catch")
        endif()
        if(_module_text MATCHES "(^|[^A-Za-z0-9_])new[ \t\r\n]")
            list(APPEND _hits "new")
        endif()
        if(_module_text MATCHES "(^|[^A-Za-z0-9_])delete[ \t\r\n]")
            list(APPEND _hits "delete")
        endif()
        if(_hits)
            list(REMOVE_DUPLICATES _hits)
            string(REPLACE ";" ", " _hit_text "${_hits}")
            message(FATAL_ERROR
                "${target_name}: PRODUCT Vivid module '${_module_norm}' contains high-risk token(s): ${_hit_text}. "
                "Make the module MCU-clean or add an explicit product gate before admitting it.")
        endif()
    endforeach()
endfunction()

function(h747_lab_check_player_md3_vivid_product_whitelist target_name)
    if(NOT target_name STREQUAL "h747_lab_player_md3")
        return()
    endif()
    if(NOT CHARM_VIVID_FEATURESET STREQUAL "PRODUCT")
        return()
    endif()

    foreach(_required_list IN ITEMS
            CHARM_VIVID_PRODUCT_CORE_MODULES
            CHARM_VIVID_PRODUCT_GFX_MODULES
            CHARM_VIVID_PRODUCT_WIDGETS)
        if(NOT DEFINED ${_required_list} OR NOT ${_required_list})
            message(FATAL_ERROR
                "${target_name}: ${_required_list} must be explicit in PRODUCT firmware")
        endif()
    endforeach()

    foreach(_module IN LISTS ARGN)
        cmake_path(NORMAL_PATH _module OUTPUT_VARIABLE _module_norm)
        set(_allowed TRUE)
        set(_module_name "")
        set(_declared_modules "")
        if(_module_norm MATCHES "/Modules/ui/vivid/core/([^/]+)\\.cppm$")
            set(_allowed FALSE)
            set(_module_name "${CMAKE_MATCH_1}")
            set(_declared_modules ${CHARM_VIVID_PRODUCT_CORE_MODULES})
        elseif(_module_norm MATCHES "/Modules/ui/vivid/gfx/([^/]+)\\.cppm$")
            set(_allowed FALSE)
            set(_module_name "${CMAKE_MATCH_1}")
            set(_declared_modules ${CHARM_VIVID_PRODUCT_GFX_MODULES})
        endif()

        if(_allowed)
            continue()
        endif()
        foreach(_declared_module IN LISTS _declared_modules)
            if(_module_name STREQUAL _declared_module)
                set(_allowed TRUE)
                break()
            endif()
        endforeach()
        if(NOT _allowed)
            message(FATAL_ERROR
                "${target_name}: Vivid PRODUCT module '${_module_norm}' is not declared "
                "in the product core/gfx whitelist")
        endif()
    endforeach()
endfunction()

function(h747_lab_write_player_md3_vivid_product_modules target_name)
    if(NOT target_name STREQUAL "h747_lab_player_md3")
        return()
    endif()
    if(NOT CHARM_VIVID_FEATURESET STREQUAL "PRODUCT")
        return()
    endif()

    set(_core_modules)
    set(_gfx_modules)
    set(_widget_modules)
    foreach(_module IN LISTS ARGN)
        cmake_path(NORMAL_PATH _module OUTPUT_VARIABLE _module_norm)
        if(_module_norm MATCHES "/Modules/ui/vivid/core/([^/]+)\\.cppm$")
            list(APPEND _core_modules "${CMAKE_MATCH_1}")
        elseif(_module_norm MATCHES "/Modules/ui/vivid/gfx/([^/]+)\\.cppm$")
            list(APPEND _gfx_modules "${CMAKE_MATCH_1}")
        elseif(_module_norm MATCHES "/Modules/ui/vivid/widgets/([^/]+)\\.cppm$")
            list(APPEND _widget_modules "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _core_modules)
    list(REMOVE_DUPLICATES _gfx_modules)
    list(REMOVE_DUPLICATES _widget_modules)
    list(SORT _core_modules)
    list(SORT _gfx_modules)
    list(SORT _widget_modules)

    set(_artifact_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/vivid")
    set(_artifact "${_artifact_dir}/player_md3_product_modules.txt")
    file(MAKE_DIRECTORY "${_artifact_dir}")
    file(WRITE "${_artifact}"
        "# Generated by h747_lab_collect_vivid_mcu_modules().\n"
        "# This is the H747 Player MD3 Vivid PRODUCT module evidence.\n"
        "target=${target_name}\n"
        "featureset=${CHARM_VIVID_FEATURESET}\n"
        "layered_transitions=${CHARM_PLAYER_LAYERED_TRANSITIONS}\n"
        "cover_theme_extract=${CHARM_PLAYER_COVER_THEME_EXTRACT}\n")
    file(APPEND "${_artifact}" "\n[core]\n")
    foreach(_module IN LISTS _core_modules)
        file(APPEND "${_artifact}" "${_module}\n")
    endforeach()
    file(APPEND "${_artifact}" "\n[gfx]\n")
    foreach(_module IN LISTS _gfx_modules)
        file(APPEND "${_artifact}" "${_module}\n")
    endforeach()
    file(APPEND "${_artifact}" "\n[widgets]\n")
    foreach(_module IN LISTS _widget_modules)
        file(APPEND "${_artifact}" "${_module}\n")
    endforeach()
endfunction()

function(h747_lab_check_player_md3_host_only_boundary target_name)
    if(NOT target_name STREQUAL "h747_lab_player_md3")
        return()
    endif()

    foreach(_module IN LISTS ARGN)
        cmake_path(NORMAL_PATH _module OUTPUT_VARIABLE _module_norm)
        if(NOT CHARM_PLAYER_FILE_FONTS)
            if(_module_norm MATCHES "/Modules/gfx/font/font_provider_freetype\\.cppm$"
               OR _module_norm MATCHES "/Modules/ui/vivid/font/font_package\\.cppm$")
                message(FATAL_ERROR
                    "${target_name}: file-font module '${_module_norm}' entered the default H747 Player MD3 firmware. "
                    "Enable CHARM_PLAYER_FILE_FONTS explicitly before admitting file-font backend modules.")
            endif()
        endif()
        if(NOT CHARM_PLAYER_DEBUG_UI
           AND _module_norm MATCHES "/Examples/project/player/app-vivid-MaterialDesign3/player\\.ui_debug\\.cppm$")
            message(FATAL_ERROR
                "${target_name}: player.ui_debug entered the default H747 Player MD3 firmware. "
                "Enable CHARM_PLAYER_DEBUG_UI explicitly before admitting debug-only UI modules.")
        endif()
        if(NOT CHARM_PLAYER_DEBUG_UI
           AND (_module_norm MATCHES "/Modules/ui/vivid/widgets/table_view\\.cppm$"
                OR _module_norm MATCHES "/Modules/ui/vivid/widgets/tree_view\\.cppm$"))
            message(FATAL_ERROR
                "${target_name}: debug-only Vivid widget '${_module_norm}' entered the default H747 Player MD3 firmware. "
                "Enable CHARM_PLAYER_DEBUG_UI explicitly before admitting debug/demo widgets.")
        endif()
        if(NOT CHARM_PLAYER_LAYERED_TRANSITIONS
           AND (_module_norm MATCHES "/Modules/ui/vivid/core/motion_[^/]+\\.cppm$"
                OR _module_norm MATCHES "/Modules/ui/vivid/core/page_transition\\.cppm$"
                OR _module_norm MATCHES "/Modules/ui/vivid/gfx/snapshot\\.cppm$"))
            message(FATAL_ERROR
                "${target_name}: layered transition module '${_module_norm}' entered the default StaticCut firmware. "
                "Enable CHARM_PLAYER_LAYERED_TRANSITIONS explicitly before admitting layered transition modules.")
        endif()
        if(NOT CHARM_PLAYER_COVER_THEME_EXTRACT
           AND _module_norm MATCHES "/Modules/core/alg/alg_color_extract\\.cppm$")
            message(FATAL_ERROR
                "${target_name}: dynamic cover theme extractor '${_module_norm}' entered the default H747 Player MD3 firmware. "
                "Enable CHARM_PLAYER_COVER_THEME_EXTRACT explicitly before admitting Material color extraction.")
        endif()
    endforeach()
endfunction()

function(h747_lab_check_player_md3_target_sources target_name)
    if(NOT target_name STREQUAL "h747_lab_player_md3")
        return()
    endif()

    foreach(_source IN LISTS ARGN)
        cmake_path(NORMAL_PATH _source OUTPUT_VARIABLE _source_norm)
        if(NOT CHARM_PLAYER_COVER_THEME_EXTRACT
           AND (_source_norm MATCHES "/Modules/core/alg/alg_color_extract\\.cppm$"
                OR _source_norm MATCHES "/Modules/thirdparty/material_color_utils/"))
            message(FATAL_ERROR
                "${target_name}: dynamic cover theme source '${_source_norm}' entered the default H747 Player MD3 target. "
                "Enable CHARM_PLAYER_COVER_THEME_EXTRACT explicitly before admitting Material color extraction.")
        endif()
        if(NOT CHARM_PLAYER_FILE_FONTS
           AND (_source_norm MATCHES "/Modules/gfx/font/font_provider_freetype\\.cppm$"
                OR _source_norm MATCHES "/Modules/gfx/font/font_provider_vfs\\.cppm$"
                OR _source_norm MATCHES "/Modules/ui/vivid/font/font_package\\.cppm$"))
            message(FATAL_ERROR
                "${target_name}: file-font source '${_source_norm}' entered the default H747 Player MD3 target. "
                "Enable CHARM_PLAYER_FILE_FONTS explicitly before admitting file-font backend sources.")
        endif()
        if(NOT CHARM_PLAYER_DEBUG_UI
           AND _source_norm MATCHES "/Examples/project/player/app-vivid-MaterialDesign3/player\\.ui_debug\\.cppm$")
            message(FATAL_ERROR
                "${target_name}: debug UI source '${_source_norm}' entered the default H747 Player MD3 target. "
                "Enable CHARM_PLAYER_DEBUG_UI explicitly before admitting debug UI sources.")
        endif()
        if(NOT CHARM_PLAYER_LAYERED_TRANSITIONS
           AND (_source_norm MATCHES "/Modules/ui/vivid/core/motion_[^/]+\\.cppm$"
                OR _source_norm MATCHES "/Modules/ui/vivid/core/page_transition\\.cppm$"
                OR _source_norm MATCHES "/Modules/ui/vivid/gfx/snapshot\\.cppm$"))
            message(FATAL_ERROR
                "${target_name}: layered transition source '${_source_norm}' entered the default StaticCut target. "
                "Enable CHARM_PLAYER_LAYERED_TRANSITIONS explicitly before admitting layered transition sources.")
        endif()
    endforeach()
endfunction()

function(h747_lab_check_player_md3_controller_cover_boundary target_name)
    if(NOT target_name STREQUAL "h747_lab_player_md3")
        return()
    endif()
    if(NOT CHARM_PLAYER_MCU_STRICT)
        return()
    endif()
    if(NOT DEFINED CHARM_PLAYER_LIST_COVER_CACHE_ENTRIES
        OR NOT CHARM_PLAYER_LIST_COVER_CACHE_ENTRIES STREQUAL "0")
        message(FATAL_ERROR
            "${target_name}: default H747 Player MD3 must explicitly keep "
            "CHARM_PLAYER_LIST_COVER_CACHE_ENTRIES=0. "
            "Enable a dedicated product profile before admitting list cover cache slots.")
    endif()

    file(GLOB _controller_files
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.controller.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.controller.*.inc")
    foreach(_controller_file IN LISTS _controller_files)
        if(NOT EXISTS "${_controller_file}")
            continue()
        endif()
        file(READ "${_controller_file}" _controller_text)
        set(_hits)
        if(_controller_text MATCHES "(^|[^A-Za-z0-9_])CoverImage([^A-Za-z0-9_]|$)")
            list(APPEND _hits "CoverImage")
        endif()
        if(_controller_text MATCHES "(^|[^A-Za-z0-9_])load_cover_image[ \t\r\n]*\\(")
            list(APPEND _hits "load_cover_image")
        endif()
        if(_controller_text MATCHES "(^|[^A-Za-z0-9_])release_cover_image[ \t\r\n]*\\(")
            list(APPEND _hits "release_cover_image")
        endif()
        if(_hits)
            list(REMOVE_DUPLICATES _hits)
            string(REPLACE ";" ", " _hit_text "${_hits}")
            message(FATAL_ERROR
                "${target_name}: shared PlayerController cover path contains old dynamic cover API token(s): "
                "${_hit_text} in ${_controller_file}. Controller must keep only ResolvedCover metadata; "
                "host decode buffers belong inside player.cover.")
        endif()
    endforeach()
endfunction()

function(h747_lab_check_player_md3_vivid_capacity_profile target_name)
    if(NOT target_name STREQUAL "h747_lab_player_md3")
        return()
    endif()
    if(NOT CHARM_VIVID_FEATURESET STREQUAL "PRODUCT")
        return()
    endif()

    set(_caps_file "${CMAKE_CURRENT_BINARY_DIR}/generated/vivid/soa_pool_caps.cppm")
    if(NOT EXISTS "${_caps_file}")
        message(FATAL_ERROR
            "${target_name}: missing generated Vivid SoA capacity artifact: ${_caps_file}")
    endif()
    file(READ "${_caps_file}" _caps_text)

    set(_expected_text_arena "${CHARM_VIVID_SOA_TEXT_ARENA_BYTES}")
    if(NOT _expected_text_arena)
        message(FATAL_ERROR
            "${target_name}: CHARM_VIVID_SOA_TEXT_ARENA_BYTES must be explicit for PRODUCT")
    endif()
    if(NOT _caps_text MATCHES "constexpr std::size_t kSoaTextArenaBytes = ${_expected_text_arena};")
        message(FATAL_ERROR
            "${target_name}: generated kSoaTextArenaBytes does not match PRODUCT profile "
            "(${_expected_text_arena}). Check ${_caps_file}")
    endif()

    if(NOT _caps_text MATCHES "constexpr std::size_t kDefaultPoolCap = ${CHARM_VIVID_SOA_MAX_NODES};")
        message(FATAL_ERROR
            "${target_name}: generated kDefaultPoolCap does not match CHARM_VIVID_SOA_MAX_NODES "
            "(${CHARM_VIVID_SOA_MAX_NODES}). Check ${_caps_file}")
    endif()

    set(_expected_zero_caps
        TextInput
        TextArea
        NumberInput
        Stepper
        ToggleGroup
        Checkbox
        Radio
        ListItem
        NumberList
        Roller
        List)
    if(NOT CHARM_PLAYER_DEBUG_UI)
        list(APPEND _expected_zero_caps TableView TreeView)
    endif()
    foreach(_cap_name IN LISTS _expected_zero_caps)
        if(NOT _caps_text MATCHES "constexpr std::size_t kPoolCap${_cap_name} = 0;")
            message(FATAL_ERROR
                "${target_name}: PRODUCT profile expected kPoolCap${_cap_name}=0. "
                "This usually means a disabled widget payload pool returned to firmware. "
                "Check ${_caps_file}")
        endif()
    endforeach()

    if(NOT _caps_text MATCHES "constexpr std::size_t kPoolCapLabel = ${CHARM_VIVID_PAYLOAD_CAP_LABEL};")
        message(FATAL_ERROR "${target_name}: generated Label payload cap does not match PRODUCT profile")
    endif()
    if(NOT _caps_text MATCHES "constexpr std::size_t kPoolCapButton = ${CHARM_VIVID_PAYLOAD_CAP_BUTTON};")
        message(FATAL_ERROR "${target_name}: generated Button payload cap does not match PRODUCT profile")
    endif()
    if(NOT _caps_text MATCHES "constexpr std::size_t kPoolCapImage = ${CHARM_VIVID_PAYLOAD_CAP_IMAGE};")
        message(FATAL_ERROR "${target_name}: generated Image payload cap does not match PRODUCT profile")
    endif()
    if(NOT _caps_text MATCHES "constexpr std::size_t kPoolCapListView = ${CHARM_VIVID_PAYLOAD_CAP_LIST_VIEW};")
        message(FATAL_ERROR "${target_name}: generated ListView payload cap does not match PRODUCT profile")
    endif()

    set(_config_file "${CMAKE_CURRENT_BINARY_DIR}/generated/vivid/config.generated.cppm")
    if(NOT EXISTS "${_config_file}")
        message(FATAL_ERROR
            "${target_name}: missing generated Vivid config artifact: ${_config_file}")
    endif()
    file(READ "${_config_file}" _config_text)
    if(NOT _config_text MATCHES "StyleConfig\\{[ \t\r\n]*${CHARM_VIVID_STYLE_CLASS_MAX},[ \t\r\n]*${CHARM_VIVID_STYLE_RULE_CAP},[ \t\r\n]*${CHARM_VIVID_STYLE_METRICS_POOL_CAP}[ \t\r\n]*\\}")
        message(FATAL_ERROR
            "${target_name}: generated Vivid style profile does not match "
            "CHARM_VIVID_STYLE_CLASS_MAX=${CHARM_VIVID_STYLE_CLASS_MAX}, "
            "CHARM_VIVID_STYLE_RULE_CAP=${CHARM_VIVID_STYLE_RULE_CAP}, "
            "CHARM_VIVID_STYLE_METRICS_POOL_CAP=${CHARM_VIVID_STYLE_METRICS_POOL_CAP}. "
            "Check ${_config_file}")
    endif()
endfunction()

function(h747_lab_collect_vivid_mcu_modules target_name out_modules out_base_dirs)
    set(CHARM_SOURCE_ROOT "${CHARM_ROOT}")
    set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
    set(CHARM_ENABLE_UI_VIVID ON CACHE BOOL "" FORCE)
    if(target_name STREQUAL "h747_lab_player_md3" AND CHARM_PLAYER_FILE_FONTS)
        set(CHARM_ENABLE_FREETYPE ON CACHE BOOL "" FORCE)
    else()
        set(CHARM_ENABLE_FREETYPE OFF CACHE BOOL "" FORCE)
    endif()
    if(DEFINED H747_LAB_VIVID_FEATURESET)
        set(_h747_vivid_featureset "${H747_LAB_VIVID_FEATURESET}")
    else()
        set(_h747_vivid_featureset "MCU_MIN")
    endif()
    set(CHARM_VIVID_FEATURESET "${_h747_vivid_featureset}" CACHE STRING "" FORCE)
    set(CHARM_VIVID_SCREEN_WIDTH "720" CACHE STRING "" FORCE)
    set(CHARM_VIVID_SCREEN_HEIGHT "1280" CACHE STRING "" FORCE)
    set(CHARM_VIVID_SCREEN_PIXEL_FORMAT "RGB888" CACHE STRING "" FORCE)
    set(CHARM_VIVID_LAYER_CACHE_SLOTS "1" CACHE STRING "" FORCE)
    set(CHARM_VIVID_LAYER_CACHE_WIDTH "720" CACHE STRING "" FORCE)
    set(CHARM_VIVID_LAYER_CACHE_HEIGHT "1280" CACHE STRING "" FORCE)
    if(CHARM_VIVID_FEATURESET STREQUAL "FULL" OR CHARM_VIVID_FEATURESET STREQUAL "PRODUCT")
        set(CHARM_VIVID_ENABLE_FLOAT_WIDGETS ON CACHE BOOL "" FORCE)
        if(NOT DEFINED CHARM_VIVID_SOA_MAX_NODES OR CHARM_VIVID_SOA_MAX_NODES STREQUAL "")
            set(CHARM_VIVID_SOA_MAX_NODES "384" CACHE STRING "" FORCE)
        endif()
    else()
        set(CHARM_VIVID_ENABLE_FLOAT_WIDGETS OFF CACHE BOOL "" FORCE)
        if(NOT DEFINED CHARM_VIVID_SOA_MAX_NODES OR CHARM_VIVID_SOA_MAX_NODES STREQUAL "")
            set(CHARM_VIVID_SOA_MAX_NODES "192" CACHE STRING "" FORCE)
        endif()
    endif()

    include("${CHARM_ROOT}/Modules/ui/vivid/vivid.cmake")

    set(_modules
        "${CHARM_ROOT}/Modules/gfx/font/font.cppm"
        "${CHARM_ROOT}/Modules/gfx/font/typography.cppm"
        "${CHARM_ROOT}/Modules/gfx/font/font_defaults_noto.cppm"
        "${CHARM_ROOT}/Modules/gfx/font/font_noto_ascii_12.cppm"
        "${CHARM_ROOT}/Modules/gfx/font/font_noto_ascii_16.cppm"
        "${CHARM_ROOT}/Modules/gfx/font/font_noto_sc_12.cppm"
        "${CHARM_ROOT}/Modules/gfx/font/font_noto_sc_16.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/core/config.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/core/geometry.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/core/handle.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/canvas.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/color.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/draw_cmd.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/draw_cmd_buffer.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/draw_cmd_executor.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/draw_cmd_schema.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/framebuffer.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/framebuffer_core.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/image.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/path.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/pixel_format.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/pixel_ops.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/render_core.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/text_box.cppm"
        "${CHARM_ROOT}/Modules/core/alg/alg_arc.cppm"
        "${CHARM_ROOT}/Modules/core/alg/alg_circle.cppm"
        "${CHARM_ROOT}/Modules/core/alg/alg_list_scroll.cppm"
        "${CHARM_ROOT}/Modules/core/alg/alg_round_rect.cppm"
        "${CHARM_ROOT}/Modules/core/alg/alg_scroll.cppm"
        "${CHARM_ROOT}/Modules/core/alg/alg_scroll_bounds.cppm"
        "${CHARM_ROOT}/Modules/core/alg/alg_scroll_thumb.cppm"
        "${CHARM_ROOT}/Modules/core/alg/alg_text_layout.cppm"
        "${CHARM_ROOT}/Modules/core/alg/alg_text_parse.cppm"
        "${CHARM_ROOT}/Modules/core/alg/alg_text_scroll.cppm"
        "${CHARM_ROOT}/Modules/core/alg/alg_virtual_list.cppm"
        "${CHARM_ROOT}/Modules/core/service/fixed_vector.cppm"
        "${CHARM_ROOT}/Modules/core/service/signal.cppm"
        "${CHARM_ROOT}/Modules/core/service/state.cppm"
        "${CHARM_ROOT}/Modules/core/service/service_dirty_rects.cppm"
        "${CHARM_ROOT}/Modules/core/util/core.cppm"
        "${CHARM_ROOT}/Modules/core/util/delegate.cppm"
        "${CHARM_ROOT}/Modules/ui/common/charm.core.event.cppm"
        "${CHARM_ROOT}/Modules/ui/common/ui.render_backend.cppm")

    if(NOT target_name STREQUAL "h747_lab_player_md3" OR CHARM_PLAYER_COVER_THEME_EXTRACT)
        list(APPEND _modules
            "${CHARM_ROOT}/Modules/core/alg/alg_color_extract.cppm")
    endif()

    if(CHARM_VIVID_FEATURESET STREQUAL "FULL")
        file(GLOB_RECURSE _full_vivid_modules
            "${CHARM_ROOT}/Modules/ui/vivid/core/*.cppm"
            "${CHARM_ROOT}/Modules/ui/vivid/gfx/*.cppm")
        list(FILTER _full_vivid_modules EXCLUDE REGEX "/Modules/ui/vivid/font/")
        list(FILTER _full_vivid_modules EXCLUDE REGEX "/Modules/ui/vivid/charm\\.ui\\.vivid")
        list(FILTER _full_vivid_modules EXCLUDE REGEX "/Modules/ui/vivid/gfx/snapshot\\.cppm$")
        list(FILTER _full_vivid_modules EXCLUDE REGEX "/Modules/ui/vivid/gfx/display_policy\\.cppm$")
        list(APPEND _modules ${_full_vivid_modules})
        file(GLOB_RECURSE _full_vivid_widget_modules
            "${CHARM_ROOT}/Modules/ui/vivid/widgets/*.cppm")
        list(APPEND _modules ${_full_vivid_widget_modules})
    elseif(CHARM_VIVID_FEATURESET STREQUAL "PRODUCT")
        vivid_collect_product_core_modules(_product_vivid_core_modules)
        vivid_collect_product_gfx_modules(_product_vivid_gfx_modules)
        vivid_collect_product_widget_modules(_product_vivid_widget_modules)
        list(APPEND _modules
            ${_product_vivid_core_modules}
            ${_product_vivid_gfx_modules}
            ${_product_vivid_widget_modules})
    endif()

    set(_base_dirs "${CHARM_ROOT}/Modules")
    vivid_collect_modules(${target_name} _modules _base_dirs)

    list(REMOVE_DUPLICATES _modules)
    list(REMOVE_DUPLICATES _base_dirs)
    if(CHARM_VIVID_FEATURESET STREQUAL "PRODUCT")
        h747_lab_check_player_md3_vivid_product_whitelist("${target_name}" ${_modules})
        h747_lab_check_vivid_product_modules("${target_name}" ${_modules})
        h747_lab_write_player_md3_vivid_product_modules("${target_name}" ${_modules})
    endif()
    h747_lab_check_player_md3_host_only_boundary("${target_name}" ${_modules})
    h747_lab_check_player_md3_vivid_capacity_profile("${target_name}")
    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_base_dirs} "${_base_dirs}" PARENT_SCOPE)
endfunction()

function(h747_lab_app_needs_elf_load out_var app_name)
    if(app_name STREQUAL "posix_lab" OR app_name STREQUAL "app_lab")
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(h747_lab_select_linker_script out_script target_name app_name)
    h747_lab_app_needs_elf_load(_needs_elf_load "${app_name}")
    if(_needs_elf_load)
        set(${out_script} "${STM32_LINKER_SCRIPT}" PARENT_SCOPE)
        return()
    endif()

    set(_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/linker")
    set(_generated_script "${_generated_dir}/${target_name}.ld")
    file(MAKE_DIRECTORY "${_generated_dir}")
    file(READ "${STM32_LINKER_SCRIPT}" _script_text)
    string(REPLACE "\r\n" "\n" _script_text "${_script_text}")

    set(_elf_load_block
"  .elf_load 0x24070000 (NOLOAD) :
  {
    . = ALIGN(32);
    __elf_load_start__ = .;
    . = . + 0x2000;
    __elf_load_end__ = .;
  } >RAM_D1
")
    string(FIND "${_script_text}" "${_elf_load_block}" _elf_load_pos)
    if(_elf_load_pos GREATER_EQUAL 0)
        string(REPLACE "${_elf_load_block}" "" _script_text "${_script_text}")
    endif()
    if(_script_text MATCHES "__elf_load_start__|__elf_load_end__|\\.elf_load")
        message(FATAL_ERROR
            "${target_name}: H747 Lab target without dynamic ELF loading would still reserve the ELF load region. "
            "Update h747_lab_select_linker_script() for the current linker script shape.")
    endif()

    file(WRITE "${_generated_script}" "${_script_text}")
    set(${out_script} "${_generated_script}" PARENT_SCOPE)
endfunction()

function(h747_lab_add_profile profile_name)
    set(_profile_manifest "${H747_LAB_ROOT}/profiles/${profile_name}/profile.cmake")
    if(NOT EXISTS "${_profile_manifest}")
        message(FATAL_ERROR "Missing h747-lab profile manifest: ${_profile_manifest}")
    endif()

    unset(H747_LAB_PROFILE_TARGET)
    unset(H747_LAB_PROFILE_BOARD)
    unset(H747_LAB_PROFILE_RUNTIME)
    unset(H747_LAB_PROFILE_APP)
    unset(H747_LAB_PROFILE_SERVICES)
    include("${_profile_manifest}")

    if(NOT H747_LAB_PROFILE_TARGET)
        message(FATAL_ERROR "${_profile_manifest}: H747_LAB_PROFILE_TARGET is required")
    endif()
    if(NOT H747_LAB_PROFILE_BOARD STREQUAL "h747_diy")
        message(FATAL_ERROR
            "${_profile_manifest}: unsupported H747_LAB_PROFILE_BOARD='${H747_LAB_PROFILE_BOARD}'")
    endif()
    if(NOT H747_LAB_PROFILE_RUNTIME STREQUAL "foundation")
        message(FATAL_ERROR
            "${_profile_manifest}: unsupported H747_LAB_PROFILE_RUNTIME='${H747_LAB_PROFILE_RUNTIME}'")
    endif()
    if(NOT H747_LAB_PROFILE_APP)
        message(FATAL_ERROR "${_profile_manifest}: H747_LAB_PROFILE_APP is required")
    endif()
    if(NOT H747_LAB_PROFILE_SERVICES)
        message(FATAL_ERROR "${_profile_manifest}: H747_LAB_PROFILE_SERVICES is required")
    endif()

    set(_app_manifest "${H747_LAB_ROOT}/apps/${H747_LAB_PROFILE_APP}/app.cmake")
    if(NOT EXISTS "${_app_manifest}")
        message(FATAL_ERROR "Missing h747-lab app manifest: ${_app_manifest}")
    endif()

    unset(H747_LAB_APP_NAME)
    unset(H747_LAB_APP_SOURCES)
    unset(H747_LAB_APP_INCLUDE_DIRS)
    unset(H747_LAB_APP_MODULE_SOURCES)
    unset(H747_LAB_APP_MODULE_BASE_DIRS)
    unset(H747_LAB_APP_COMPILE_DEFINITIONS)
    include("${_app_manifest}")

    if(NOT H747_LAB_APP_NAME STREQUAL H747_LAB_PROFILE_APP)
        message(FATAL_ERROR
            "${_app_manifest}: H747_LAB_APP_NAME='${H747_LAB_APP_NAME}' "
            "does not match profile app '${H747_LAB_PROFILE_APP}'")
    endif()
    if(NOT H747_LAB_APP_SOURCES)
        message(FATAL_ERROR "${_app_manifest}: H747_LAB_APP_SOURCES is required")
    endif()

    h747_lab_add_firmware(
        TARGET "${H747_LAB_PROFILE_TARGET}"
        PROFILE "${profile_name}"
        APP "${H747_LAB_PROFILE_APP}"
        APP_SOURCES ${H747_LAB_APP_SOURCES}
        APP_INCLUDE_DIRS ${H747_LAB_APP_INCLUDE_DIRS}
        APP_MODULE_SOURCES ${H747_LAB_APP_MODULE_SOURCES}
        APP_MODULE_BASE_DIRS ${H747_LAB_APP_MODULE_BASE_DIRS}
        APP_COMPILE_DEFINITIONS ${H747_LAB_APP_COMPILE_DEFINITIONS}
        SERVICES ${H747_LAB_PROFILE_SERVICES})
endfunction()

function(h747_lab_add_firmware)
    set(options)
    set(oneValueArgs TARGET PROFILE APP)
    set(multiValueArgs APP_SOURCES APP_INCLUDE_DIRS APP_MODULE_SOURCES APP_MODULE_BASE_DIRS APP_COMPILE_DEFINITIONS SERVICES)
    cmake_parse_arguments(H747_LAB_FW "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT H747_LAB_FW_TARGET)
        message(FATAL_ERROR "h747_lab_add_firmware(...): TARGET is required")
    endif()
    if(NOT H747_LAB_FW_PROFILE)
        message(FATAL_ERROR "h747_lab_add_firmware(...): PROFILE is required")
    endif()
    if(NOT H747_LAB_FW_APP)
        message(FATAL_ERROR "h747_lab_add_firmware(...): APP is required")
    endif()
    if(NOT H747_LAB_FW_APP_SOURCES)
        message(FATAL_ERROR "h747_lab_add_firmware(...): APP_SOURCES is required")
    endif()
    if(NOT H747_LAB_FW_SERVICES)
        message(FATAL_ERROR "h747_lab_add_firmware(...): SERVICES is required")
    endif()

    set(_profile_source "${H747_LAB_ROOT}/profiles/${H747_LAB_FW_PROFILE}/profile.cpp")
    if(NOT EXISTS "${_profile_source}")
        message(FATAL_ERROR "Missing h747-lab profile source: ${_profile_source}")
    endif()

    h747_lab_collect_services(_service_sources _service_include_dirs ${H747_LAB_FW_SERVICES})
    h747_lab_select_linker_script(
        _target_linker_script
        "${H747_LAB_FW_TARGET}"
        "${H747_LAB_FW_APP}")

    set(_generated_app_sources)
    if(H747_LAB_FW_APP STREQUAL "posix_lab")
        set(_elf_samples_dir "${CHARM_ROOT}/Examples/posix/elf_samples")
        set(_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/posix_lab_elf_samples")
        set(_generated_out_dir "${_generated_dir}/out")
        file(MAKE_DIRECTORY "${_generated_dir}")
        set(_elf_samples
            hello
            argv_dump
            env_dump
            stderr_demo
            exit_code
            cat_file
            write_file
            append_file
            fd_probe
            stat_probe)
        set(_generated_incs)
        foreach(_sample IN LISTS _elf_samples)
            list(APPEND _generated_incs "${_generated_dir}/${_sample}.elf.inc")
        endforeach()
        add_custom_command(
            OUTPUT ${_generated_incs}
            COMMAND powershell -ExecutionPolicy Bypass -File
                "${_elf_samples_dir}/build_elf_samples.ps1"
                -OutDir "${_generated_out_dir}"
                -IncDir "${_generated_dir}"
                -ElfBase 0x24070000
            DEPENDS
                "${_elf_samples_dir}/build_elf_samples.ps1"
                "${_elf_samples_dir}/elf_samples.ld"
                "${_elf_samples_dir}/elf_hostcall.h"
                "${_elf_samples_dir}/hello.c"
                "${_elf_samples_dir}/argv_dump.c"
                "${_elf_samples_dir}/env_dump.c"
                "${_elf_samples_dir}/stderr_demo.c"
                "${_elf_samples_dir}/exit_code.c"
                "${_elf_samples_dir}/cat_file.c"
                "${_elf_samples_dir}/write_file.c"
                "${_elf_samples_dir}/append_file.c"
                "${_elf_samples_dir}/fd_probe.c"
                "${_elf_samples_dir}/stat_probe.c"
            VERBATIM)
        add_custom_target(${H747_LAB_FW_TARGET}_elf_samples DEPENDS ${_generated_incs})
        list(APPEND _generated_app_sources ${_generated_incs})
    endif()

    if(H747_LAB_FW_APP STREQUAL "app_lab")
        set(_app_elf_samples_dir "${CHARM_ROOT}/Examples/app_abi/elf_samples")
        set(_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/app_lab_elf_samples")
        set(_generated_out_dir "${_generated_dir}/out")
        set(_pack_tool_build_dir "${_generated_dir}/cmake-build-app-abi-store-pack-tool")
        file(MAKE_DIRECTORY "${_generated_dir}")
        set(_app_elf_samples
            hello_app
            player_min)
        set(_generated_incs)
        foreach(_sample IN LISTS _app_elf_samples)
            list(APPEND _generated_incs "${_generated_dir}/${_sample}.elf.inc")
        endforeach()
        list(APPEND _generated_incs "${_generated_dir}/appstore.bin.inc")
        add_custom_command(
            OUTPUT ${_generated_incs}
            COMMAND powershell -ExecutionPolicy Bypass -File
                "${_app_elf_samples_dir}/build_app_elf_samples.ps1"
                -OutDir "${_generated_out_dir}"
                -IncDir "${_generated_dir}"
                -ElfBase 0x24070000
                -HostCompiler "D:/Toolchains/w64devkit/bin/g++.exe"
                -PackToolBuildDir "${_pack_tool_build_dir}"
                -StorePath "${_generated_out_dir}/appstore.bin"
            DEPENDS
                "${_app_elf_samples_dir}/build_app_elf_samples.ps1"
                "${_app_elf_samples_dir}/app_elf.ld"
                "${CHARM_ROOT}/Examples/app_abi/charm_app_api.h"
                "${CHARM_ROOT}/Examples/app_abi/charm_app_store.hpp"
                "${CHARM_ROOT}/Examples/app_abi/player_min_core.h"
                "${_app_elf_samples_dir}/hello_app.c"
                "${_app_elf_samples_dir}/player_min.c"
                "${CHARM_ROOT}/Examples/system/app_abi_store_pack_tool/main.cpp"
                "${CHARM_ROOT}/Examples/system/app_abi_store_pack_tool/CMakeLists.txt"
            VERBATIM)
        add_custom_target(${H747_LAB_FW_TARGET}_app_elf_samples DEPENDS ${_generated_incs})
        list(APPEND _generated_app_sources ${_generated_incs})
    endif()

    add_executable(${H747_LAB_FW_TARGET}
        ${H747_LAB_PLATFORM_SOURCES}
        ${H747_LAB_BOARD_SOURCES}
        ${H747_LAB_RUNTIME_SOURCES}
        ${_service_sources}
        ${H747_LAB_FW_APP_SOURCES}
        ${_generated_app_sources}
        "${_profile_source}"
    )

    if(H747_LAB_FW_TARGET STREQUAL "h747_lab_player_md3")
        set(CHARM_DR_LIBS_DIR "${CHARM_ROOT}/Modules/thirdparty/dr_libs" CACHE PATH "" FORCE)
        include("${CHARM_ROOT}/cmake/FatFs.cmake")
        include("${CHARM_ROOT}/cmake/DRLibs.cmake")
        charm_link_fatfs(${H747_LAB_FW_TARGET})
        charm_link_dr_libs(${H747_LAB_FW_TARGET})
        if(CHARM_PLAYER_COVER_THEME_EXTRACT)
            set(CHARM_MATERIAL_COLOR_UTILS_DIR "${CHARM_ROOT}/Modules/thirdparty/material_color_utils" CACHE PATH "" FORCE)
            target_sources(${H747_LAB_FW_TARGET} PRIVATE
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/utils/utils.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/cam/cam.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/cam/hct.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/cam/hct_solver.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/cam/viewing_conditions.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/contrast/contrast.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/dislike/dislike.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/dynamiccolor/dynamic_color.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/dynamiccolor/dynamic_scheme.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/dynamiccolor/material_dynamic_colors.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/palettes/tones.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/quantize/celebi.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/quantize/lab.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/quantize/wsmeans.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/quantize/wu.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/score/score.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/scheme/scheme_expressive.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/scheme/scheme_fruit_salad.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/scheme/scheme_tonal_spot.cc"
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/scheme/scheme_vibrant.cc")
            target_include_directories(${H747_LAB_FW_TARGET} PRIVATE
                "${CHARM_MATERIAL_COLOR_UTILS_DIR}")
        endif()
        if(CHARM_PLAYER_FILE_FONTS)
            include("${CHARM_ROOT}/cmake/FreeType.cmake")
            if((NOT DEFINED CHARM_FREETYPE_DIR OR CHARM_FREETYPE_DIR STREQUAL "")
               AND EXISTS "G:/Third_Party/freetype/CMakeLists.txt")
                set(CHARM_FREETYPE_DIR "G:/Third_Party/freetype" CACHE PATH "" FORCE)
            endif()
            charm_link_freetype(${H747_LAB_FW_TARGET})
        endif()
    endif()

    if(TARGET ${H747_LAB_FW_TARGET}_elf_samples)
        add_dependencies(${H747_LAB_FW_TARGET} ${H747_LAB_FW_TARGET}_elf_samples)
    endif()
    if(TARGET ${H747_LAB_FW_TARGET}_app_elf_samples)
        add_dependencies(${H747_LAB_FW_TARGET} ${H747_LAB_FW_TARGET}_app_elf_samples)
    endif()

    if(H747_LAB_FW_APP STREQUAL "player" OR H747_LAB_FW_APP STREQUAL "player_md3")
        h747_lab_collect_vivid_mcu_modules(
            ${H747_LAB_FW_TARGET}
            _vivid_module_sources
            _vivid_module_base_dirs)
        list(APPEND H747_LAB_FW_APP_MODULE_SOURCES ${_vivid_module_sources})
        list(APPEND H747_LAB_FW_APP_MODULE_BASE_DIRS ${_vivid_module_base_dirs})
        list(REMOVE_DUPLICATES H747_LAB_FW_APP_MODULE_SOURCES)
        list(REMOVE_DUPLICATES H747_LAB_FW_APP_MODULE_BASE_DIRS)
    endif()

    target_sources(${H747_LAB_FW_TARGET}
        PUBLIC
            FILE_SET modules TYPE CXX_MODULES
            BASE_DIRS
                "${CHARM_ROOT}/Modules"
                ${H747_LAB_FW_APP_MODULE_BASE_DIRS}
            FILES
                ${H747_LAB_MODULE_SOURCES}
                ${H747_LAB_FW_APP_MODULE_SOURCES}
    )

    h747_lab_check_player_md3_target_sources(
        "${H747_LAB_FW_TARGET}"
        ${H747_LAB_FW_APP_MODULE_SOURCES})
    h747_lab_check_player_md3_controller_cover_boundary("${H747_LAB_FW_TARGET}")

    target_include_directories(${H747_LAB_FW_TARGET} PRIVATE
        ${H747_LAB_COMMON_INCLUDE_DIRS}
        ${_service_include_dirs}
        ${H747_LAB_FW_APP_INCLUDE_DIRS}
        "${CHARM_ROOT}/Modules/io/out"
    )

    target_compile_definitions(${H747_LAB_FW_TARGET} PRIVATE
        ${H747_LAB_COMMON_DEFINITIONS}
        ${H747_LAB_FW_APP_COMPILE_DEFINITIONS}
        "H747_LAB_PROFILE_NAME=\"${H747_LAB_FW_PROFILE}\""
    )

    if(H747_LAB_FW_PROFILE STREQUAL "display_demo")
        if(H747_LAB_DISPLAY_PANEL_PROFILE STREQUAL "github4lane_2lane")
            target_compile_definitions(${H747_LAB_FW_TARGET} PRIVATE
                STM32H747_DISPLAY_MIN_PANEL_PROFILE_GITHUB4LANE_2LANE=1)
        else()
            target_compile_definitions(${H747_LAB_FW_TARGET} PRIVATE
                STM32H747_DISPLAY_MIN_PANEL_PROFILE_DTS_2LANE=1)
        endif()
    endif()

    target_compile_options(${H747_LAB_FW_TARGET} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-volatile>
    )

    if(H747_LAB_FW_TARGET STREQUAL "h747_lab_player_md3")
        target_compile_options(${H747_LAB_FW_TARGET} PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-Os>
            $<$<COMPILE_LANGUAGE:CXX>:-Os>
            $<$<COMPILE_LANGUAGE:CXX>:-fno-module-lazy>
        )
    endif()

    target_link_options(${H747_LAB_FW_TARGET} PRIVATE
        ${H747_LAB_TARGET_FLAGS}
        "-T${_target_linker_script}"
        --specs=nano.specs
        "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${H747_LAB_FW_TARGET}.map"
        -Wl,--gc-sections
        -Wl,--start-group
        -lc
        -lm
        -lstdc++
        -lsupc++
        -Wl,--end-group
        -Wl,--print-memory-usage
    )

    if(CMAKE_OBJCOPY)
        add_custom_command(TARGET ${H747_LAB_FW_TARGET} POST_BUILD
            BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/${H747_LAB_FW_TARGET}.bin"
            COMMAND ${CMAKE_OBJCOPY}
                -O binary
                $<TARGET_FILE:${H747_LAB_FW_TARGET}>
                "${CMAKE_CURRENT_BINARY_DIR}/${H747_LAB_FW_TARGET}.bin"
            VERBATIM)
    endif()

    h747_lab_add_player_md3_memory_evidence("${H747_LAB_FW_TARGET}")
endfunction()
