set(_VIVID_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

include("${CMAKE_CURRENT_LIST_DIR}/cmake/widget_catalog_compiler.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/product_profile_compiler.cmake")

macro(vivid_cache_default name type default_value doc)
    if (NOT DEFINED ${name})
        set(${name} "${default_value}" CACHE ${type} "${doc}")
    endif()
endmacro()

function(vivid_bool_literal out_var value)
    if (${value})
        set(${out_var} 1 PARENT_SCOPE)
    else()
        set(${out_var} 0 PARENT_SCOPE)
    endif()
endfunction()

function(vivid_reject_legacy_product_configuration)
    set(_legacy_names
        CHARM_VIVID_PRODUCT_CORE_MODULES
        CHARM_VIVID_PRODUCT_GFX_MODULES
        CHARM_VIVID_PRODUCT_WIDGETS)
    get_cmake_property(_known_variables VARIABLES)
    foreach(_name IN LISTS _known_variables)
        if(_name MATCHES "^CHARM_VIVID_PAYLOAD_CAP_")
            list(APPEND _legacy_names "${_name}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _legacy_names)

    set(_found)
    foreach(_name IN LISTS _legacy_names)
        if(DEFINED ${_name})
            list(APPEND _found "${_name}")
        endif()
    endforeach()
    if(_found)
        list(JOIN _found ", " _found_text)
        message(FATAL_ERROR
            "Vivid PRODUCT legacy configuration is no longer supported: ${_found_text}. "
            "Define a vivid product profile and target envelope instead.")
    endif()
endfunction()

function(vivid_payload_cap_variable_name out_var cap_kind)
    string(REGEX REPLACE "([a-z0-9])([A-Z])" "\\1_\\2" _suffix "${cap_kind}")
    string(TOUPPER "${_suffix}" _suffix)
    if(_suffix STREQUAL "SCROLL_BAR")
        set(_suffix "SCROLLBAR")
    endif()
    set(${out_var} "SOA_POOL_CAP_${_suffix}" PARENT_SCOPE)
endfunction()

function(vivid_json_escape out_var value)
    set(_escaped "${value}")
    string(REPLACE "\\" "\\\\" _escaped "${_escaped}")
    string(REPLACE "\"" "\\\"" _escaped "${_escaped}")
    string(REPLACE "\n" "\\n" _escaped "${_escaped}")
    string(REPLACE "\r" "\\r" _escaped "${_escaped}")
    string(REPLACE "\t" "\\t" _escaped "${_escaped}")
    set(${out_var} "${_escaped}" PARENT_SCOPE)
endfunction()

function(vivid_json_string_array out_var)
    set(_json "[")
    set(_separator "")
    foreach(_value IN LISTS ARGN)
        vivid_json_escape(_escaped "${_value}")
        string(APPEND _json "${_separator}\"${_escaped}\"")
        set(_separator ",")
    endforeach()
    string(APPEND _json "]")
    set(${out_var} "${_json}" PARENT_SCOPE)
endfunction()

function(vivid_filter_sources_provided_by_linked_targets out_var target_name)
    set(_provided_sources)
    get_target_property(_linked_targets ${target_name} LINK_LIBRARIES)
    if(_linked_targets AND NOT _linked_targets STREQUAL "_linked_targets-NOTFOUND")
        foreach(_linked_target IN LISTS _linked_targets)
            if(NOT TARGET ${_linked_target})
                continue()
            endif()
            get_target_property(_linked_sources ${_linked_target} SOURCES)
            get_target_property(_module_sets ${_linked_target} CXX_MODULE_SETS)
            foreach(_module_set IN LISTS _module_sets)
                get_target_property(
                    _module_set_sources ${_linked_target} "CXX_MODULE_SET_${_module_set}")
                if(_module_set_sources
                   AND NOT _module_set_sources STREQUAL "_module_set_sources-NOTFOUND")
                    list(APPEND _linked_sources ${_module_set_sources})
                endif()
            endforeach()
            foreach(_source IN LISTS _linked_sources)
                if(_source MATCHES "^\\$<")
                    continue()
                endif()
                get_filename_component(_source_abs "${_source}" ABSOLUTE)
                file(TO_CMAKE_PATH "${_source_abs}" _source_abs)
                list(APPEND _provided_sources "${_source_abs}")
            endforeach()
        endforeach()
    endif()
    list(REMOVE_DUPLICATES _provided_sources)

    set(_filtered)
    foreach(_source IN LISTS ARGN)
        get_filename_component(_source_abs "${_source}" ABSOLUTE)
        file(TO_CMAKE_PATH "${_source_abs}" _source_abs)
        if(NOT _source_abs IN_LIST _provided_sources)
            list(APPEND _filtered "${_source}")
        endif()
    endforeach()
    set(${out_var} "${_filtered}" PARENT_SCOPE)
endfunction()

function(vivid_require_uint16_capacity var_name value)
    if (NOT "${value}" MATCHES "^[0-9]+$")
        message(FATAL_ERROR "${var_name} must be a non-negative integer, got '${value}'")
    endif()
    if ("${value}" GREATER 65535)
        message(FATAL_ERROR "${var_name} must be <= 65535, got '${value}'")
    endif()
endfunction()

function(vivid_require_nonnegative_integer var_name value)
    if (NOT "${value}" MATCHES "^[0-9]+$")
        message(FATAL_ERROR "${var_name} must be a non-negative integer, got '${value}'")
    endif()
endfunction()

function(vivid_collect_modules target_name module_list_var base_dirs_var)
    if (NOT CHARM_ENABLE_UI_VIVID)
        return()
    endif()

    if(CHARM_VIVID_FEATURESET STREQUAL "PRODUCT")
        vivid_reject_legacy_product_configuration()
        _vivid_target_get("${target_name}" CONFIGURED _vivid_target_configured)
        if(NOT _vivid_target_configured)
            message(FATAL_ERROR
                "Vivid PRODUCT target '${target_name}' has no configured product profile. "
                "Call vivid_configure_product_target(TARGET ${target_name} ...) before source collection.")
        endif()
        _vivid_target_get("${target_name}" PROFILE _vivid_profile_name)
        _vivid_target_get("${target_name}" PROFILE_FINGERPRINT _vivid_profile_fingerprint)
        _vivid_target_get("${target_name}" TARGET_FINGERPRINT _vivid_target_fingerprint)
        _vivid_profile_get("${_vivid_profile_name}" CATALOG_FINGERPRINT _vivid_catalog_fingerprint)
        _vivid_profile_get("${_vivid_profile_name}" ROOT_MODULES _vivid_profile_roots)
        _vivid_profile_get("${_vivid_profile_name}" WIDGET_KINDS _vivid_profile_widget_kinds)
        _vivid_profile_get(
            "${_vivid_profile_name}" OBJECT_WIDGET_KINDS _vivid_profile_object_widget_kinds)
        _vivid_profile_get("${_vivid_profile_name}" PAYLOAD_CAPACITIES _vivid_profile_payload_capacities)

        foreach(_field IN ITEMS
                SOA_MAX_NODES SOA_TEXT_ARENA_BYTES SEMANTIC_SLOT_CAP STYLE_PATCH_SLOT_CAP
                STYLE_CLASS_MAX STYLE_RULE_CAP
                STYLE_METRICS_POOL_CAP DRAW_CMD_MAX_COMMANDS DRAW_CMD_TEXT_BYTES
                DRAW_CMD_BLOB_BYTES FLOAT_WIDGETS)
            _vivid_profile_get("${_vivid_profile_name}" "${_field}" _profile_value)
            set(_vivid_profile_${_field} "${_profile_value}")
        endforeach()
        foreach(_field IN ITEMS
                SCREEN_WIDTH SCREEN_HEIGHT PIXEL_FORMAT LAYER_CACHE_SLOTS
                LAYER_CACHE_WIDTH LAYER_CACHE_HEIGHT RUNTIME_SCENE_INSTANCES
                STATIC_MEMORY_BUDGET_BYTES STATIC_MEMORY_MIN_HEADROOM_BYTES
                MAX_HOT_STACK_FRAME_BYTES DRAW_DETAIL_EVIDENCE)
            _vivid_target_get("${target_name}" "${_field}" _target_value)
            set(_vivid_target_${_field} "${_target_value}")
        endforeach()

        set(CHARM_VIVID_SCREEN_WIDTH "${_vivid_target_SCREEN_WIDTH}")
        set(CHARM_VIVID_SCREEN_HEIGHT "${_vivid_target_SCREEN_HEIGHT}")
        set(CHARM_VIVID_SCREEN_PIXEL_FORMAT "${_vivid_target_PIXEL_FORMAT}")
        set(CHARM_VIVID_LAYER_CACHE_SLOTS "${_vivid_target_LAYER_CACHE_SLOTS}")
        set(CHARM_VIVID_LAYER_CACHE_WIDTH "${_vivid_target_LAYER_CACHE_WIDTH}")
        set(CHARM_VIVID_LAYER_CACHE_HEIGHT "${_vivid_target_LAYER_CACHE_HEIGHT}")
        set(CHARM_VIVID_ENABLE_FLOAT_WIDGETS "${_vivid_profile_FLOAT_WIDGETS}")
        set(CHARM_VIVID_SOA_MAX_NODES "${_vivid_profile_SOA_MAX_NODES}")
        set(CHARM_VIVID_SOA_TEXT_ARENA_BYTES "${_vivid_profile_SOA_TEXT_ARENA_BYTES}")
        set(CHARM_VIVID_SEMANTIC_SLOT_CAP "${_vivid_profile_SEMANTIC_SLOT_CAP}")
        set(CHARM_VIVID_STYLE_PATCH_SLOT_CAP "${_vivid_profile_STYLE_PATCH_SLOT_CAP}")
        set(CHARM_VIVID_STYLE_CLASS_MAX "${_vivid_profile_STYLE_CLASS_MAX}")
        set(CHARM_VIVID_STYLE_RULE_CAP "${_vivid_profile_STYLE_RULE_CAP}")
        set(CHARM_VIVID_STYLE_METRICS_POOL_CAP "${_vivid_profile_STYLE_METRICS_POOL_CAP}")
        set(CHARM_VIVID_DRAW_CMD_MAX_COMMANDS "${_vivid_profile_DRAW_CMD_MAX_COMMANDS}")
        set(CHARM_VIVID_DRAW_CMD_TEXT_BYTES "${_vivid_profile_DRAW_CMD_TEXT_BYTES}")
        set(CHARM_VIVID_DRAW_CMD_BLOB_BYTES "${_vivid_profile_DRAW_CMD_BLOB_BYTES}")
        set(CHARM_VIVID_MAX_HOT_STACK_FRAME_BYTES "${_vivid_target_MAX_HOT_STACK_FRAME_BYTES}")
        set(CHARM_VIVID_DRAW_DETAIL_EVIDENCE "${_vivid_target_DRAW_DETAIL_EVIDENCE}")
        set(CHARM_VIVID_RUNTIME_SCENE_INSTANCES "${_vivid_target_RUNTIME_SCENE_INSTANCES}")
        set(CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES "${_vivid_target_STATIC_MEMORY_BUDGET_BYTES}")
        set(CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES "${_vivid_target_STATIC_MEMORY_MIN_HEADROOM_BYTES}")

        vivid_widget_profile_resolve(
            _vivid_widget_module_roots
            _vivid_active_payload_pools
            VIVID_PRODUCT_WIDGET_DEFINES
            PROFILE "${_vivid_profile_name}"
            KINDS ${_vivid_profile_widget_kinds}
            OBJECT_KINDS ${_vivid_profile_object_widget_kinds}
            PAYLOAD_CAPACITIES ${_vivid_profile_payload_capacities})
        vivid_compute_product_module_closure(
            _vivid_product_sources
            _vivid_product_modules
            _vivid_external_requirements
            KEY "${target_name}:${_vivid_profile_fingerprint}"
            ROOT_MODULES ${_vivid_profile_roots}
            INTERNAL_ROOT_MODULES ${_vivid_widget_module_roots})
        vivid_filter_sources_provided_by_linked_targets(
            _vivid_product_sources "${target_name}" ${_vivid_product_sources})
    else()
        string(TOLOWER "${CHARM_VIVID_FEATURESET}" _vivid_profile_name)
        set(_vivid_profile_fingerprint "")
        set(_vivid_target_fingerprint "")
        set(_vivid_catalog_fingerprint "")
        set(VIVID_PRODUCT_WIDGET_DEFINES "")
        vivid_cache_default(CHARM_VIVID_SCREEN_WIDTH STRING 480 "Vivid screen width")
        vivid_cache_default(CHARM_VIVID_SCREEN_HEIGHT STRING 800 "Vivid screen height")
        vivid_cache_default(CHARM_VIVID_SCREEN_PIXEL_FORMAT STRING RGB888 "Vivid screen pixel format")
        set_property(CACHE CHARM_VIVID_SCREEN_PIXEL_FORMAT PROPERTY STRINGS RGB888 RGB565)
        math(EXPR _vivid_default_layer_cache_width "${CHARM_VIVID_SCREEN_WIDTH} / 2")
        math(EXPR _vivid_default_layer_cache_height "${CHARM_VIVID_SCREEN_HEIGHT} / 2")
        vivid_cache_default(CHARM_VIVID_LAYER_CACHE_SLOTS STRING 1 "Vivid layer cache slot count")
        vivid_cache_default(CHARM_VIVID_LAYER_CACHE_WIDTH STRING ${_vivid_default_layer_cache_width} "Vivid layer cache width")
        vivid_cache_default(CHARM_VIVID_LAYER_CACHE_HEIGHT STRING ${_vivid_default_layer_cache_height} "Vivid layer cache height")
        vivid_cache_default(CHARM_VIVID_ENABLE_FLOAT_WIDGETS BOOL ON "Enable float-backed vivid widgets")
        vivid_cache_default(CHARM_VIVID_SOA_MAX_NODES STRING 256 "Vivid SoA max node count")
        vivid_cache_default(CHARM_VIVID_SOA_TEXT_ARENA_BYTES STRING "" "Vivid SoA text arena bytes override; empty keeps the featureset default")
        vivid_cache_default(CHARM_VIVID_SEMANTIC_SLOT_CAP STRING "" "Vivid sparse semantic slot capacity; empty keeps min(nodes, 64)")
        vivid_cache_default(CHARM_VIVID_STYLE_PATCH_SLOT_CAP STRING "" "Vivid sparse StylePatch slot capacity; empty keeps min(nodes, 192)")
        vivid_cache_default(CHARM_VIVID_STYLE_CLASS_MAX STRING 256 "Vivid style class capacity")
        vivid_cache_default(CHARM_VIVID_STYLE_RULE_CAP STRING 32 "Vivid stylesheet rule capacity")
        vivid_cache_default(CHARM_VIVID_STYLE_METRICS_POOL_CAP STRING 64 "Vivid stylesheet metrics pool capacity")
        vivid_cache_default(CHARM_VIVID_DRAW_CMD_MAX_COMMANDS STRING 1024 "Vivid DrawCmd command capacity")
        vivid_cache_default(CHARM_VIVID_DRAW_CMD_TEXT_BYTES STRING 4096 "Vivid DrawCmd text arena bytes")
        vivid_cache_default(CHARM_VIVID_DRAW_CMD_BLOB_BYTES STRING 2048 "Vivid DrawCmd blob arena bytes")
        vivid_cache_default(CHARM_VIVID_MAX_HOT_STACK_FRAME_BYTES STRING 4096 "Vivid selected-module stack frame limit")
        vivid_cache_default(CHARM_VIVID_DRAW_DETAIL_EVIDENCE BOOL OFF "Enable DrawCmd detail evidence")
        vivid_cache_default(CHARM_VIVID_RUNTIME_SCENE_INSTANCES STRING "" "Vivid resident Scene instance count; required by MCU_MIN")
        vivid_cache_default(CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES STRING "" "Vivid resident RAM budget; required by MCU_MIN")
        vivid_cache_default(CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES STRING "" "Minimum Vivid resident RAM headroom; required by MCU_MIN")
    endif()

    target_compile_definitions(${target_name} PRIVATE CHARM_VIVID_SOA_ONLY=1 CHARM_VIVID_KERNEL_SOA=1)
    vivid_bool_literal(_vivid_enable_float_widgets ${CHARM_VIVID_ENABLE_FLOAT_WIDGETS})
    vivid_bool_literal(_vivid_draw_detail_evidence ${CHARM_VIVID_DRAW_DETAIL_EVIDENCE})
    if (CHARM_VIVID_FEATURESET STREQUAL "MCU_MIN")
        set(VIVID_FEATURESET_ENUM "mcu_min")
        set(VIVID_IS_MCU_MIN 1)
        set(VIVID_IS_PRODUCT 0)
        set(VIVID_STATIC_MEMORY_ADMISSION_REQUIRED 1)
    elseif (CHARM_VIVID_FEATURESET STREQUAL "FULL")
        set(VIVID_FEATURESET_ENUM "full")
        set(VIVID_IS_MCU_MIN 0)
        set(VIVID_IS_PRODUCT 0)
        set(VIVID_STATIC_MEMORY_ADMISSION_REQUIRED 0)
    elseif (CHARM_VIVID_FEATURESET STREQUAL "PRODUCT")
        set(VIVID_FEATURESET_ENUM "product")
        set(VIVID_IS_MCU_MIN 0)
        set(VIVID_IS_PRODUCT 1)
        set(VIVID_STATIC_MEMORY_ADMISSION_REQUIRED 1)
    else()
        message(FATAL_ERROR "Unknown CHARM_VIVID_FEATURESET: ${CHARM_VIVID_FEATURESET}")
    endif()

    if (CHARM_VIVID_SCREEN_PIXEL_FORMAT STREQUAL "RGB565")
        set(VIVID_SCREEN_PIXEL_FORMAT "PixelFormat::RGB565")
        set(_vivid_screen_bytes_per_pixel 2)
    elseif (CHARM_VIVID_SCREEN_PIXEL_FORMAT STREQUAL "RGB888")
        set(VIVID_SCREEN_PIXEL_FORMAT "PixelFormat::RGB888")
        set(_vivid_screen_bytes_per_pixel 3)
    else()
        message(FATAL_ERROR "Unknown CHARM_VIVID_SCREEN_PIXEL_FORMAT: ${CHARM_VIVID_SCREEN_PIXEL_FORMAT}")
    endif()

    set(VIVID_SCREEN_WIDTH ${CHARM_VIVID_SCREEN_WIDTH})
    set(VIVID_SCREEN_HEIGHT ${CHARM_VIVID_SCREEN_HEIGHT})
    set(VIVID_LAYER_CACHE_SLOTS ${CHARM_VIVID_LAYER_CACHE_SLOTS})
    set(VIVID_LAYER_CACHE_WIDTH ${CHARM_VIVID_LAYER_CACHE_WIDTH})
    set(VIVID_LAYER_CACHE_HEIGHT ${CHARM_VIVID_LAYER_CACHE_HEIGHT})
    set(VIVID_ENABLE_FLOAT_WIDGETS ${_vivid_enable_float_widgets})
    set(VIVID_SOA_MAX_NODES ${CHARM_VIVID_SOA_MAX_NODES})
    if("${CHARM_VIVID_SEMANTIC_SLOT_CAP}" STREQUAL "")
        set(CHARM_VIVID_SEMANTIC_SLOT_CAP 64)
        if("${CHARM_VIVID_SOA_MAX_NODES}" LESS "${CHARM_VIVID_SEMANTIC_SLOT_CAP}")
            set(CHARM_VIVID_SEMANTIC_SLOT_CAP "${CHARM_VIVID_SOA_MAX_NODES}")
        endif()
    endif()
    vivid_require_uint16_capacity(
        "CHARM_VIVID_SEMANTIC_SLOT_CAP" "${CHARM_VIVID_SEMANTIC_SLOT_CAP}")
    if("${CHARM_VIVID_SEMANTIC_SLOT_CAP}" LESS 1)
        message(FATAL_ERROR "CHARM_VIVID_SEMANTIC_SLOT_CAP must be > 0")
    endif()
    if("${CHARM_VIVID_SEMANTIC_SLOT_CAP}" GREATER 255)
        message(FATAL_ERROR "CHARM_VIVID_SEMANTIC_SLOT_CAP must be <= 255")
    endif()
    if("${CHARM_VIVID_SEMANTIC_SLOT_CAP}" GREATER "${CHARM_VIVID_SOA_MAX_NODES}")
        message(FATAL_ERROR
            "CHARM_VIVID_SEMANTIC_SLOT_CAP must be <= CHARM_VIVID_SOA_MAX_NODES")
    endif()
    set(VIVID_SEMANTIC_SLOT_CAP ${CHARM_VIVID_SEMANTIC_SLOT_CAP})
    if("${CHARM_VIVID_STYLE_PATCH_SLOT_CAP}" STREQUAL "")
        set(CHARM_VIVID_STYLE_PATCH_SLOT_CAP 192)
        if("${CHARM_VIVID_SOA_MAX_NODES}" LESS "${CHARM_VIVID_STYLE_PATCH_SLOT_CAP}")
            set(CHARM_VIVID_STYLE_PATCH_SLOT_CAP "${CHARM_VIVID_SOA_MAX_NODES}")
        endif()
    endif()
    vivid_require_uint16_capacity(
        "CHARM_VIVID_STYLE_PATCH_SLOT_CAP" "${CHARM_VIVID_STYLE_PATCH_SLOT_CAP}")
    if("${CHARM_VIVID_STYLE_PATCH_SLOT_CAP}" LESS 1)
        message(FATAL_ERROR "CHARM_VIVID_STYLE_PATCH_SLOT_CAP must be > 0")
    endif()
    if("${CHARM_VIVID_STYLE_PATCH_SLOT_CAP}" GREATER 255)
        message(FATAL_ERROR "CHARM_VIVID_STYLE_PATCH_SLOT_CAP must be <= 255")
    endif()
    if("${CHARM_VIVID_STYLE_PATCH_SLOT_CAP}" GREATER "${CHARM_VIVID_SOA_MAX_NODES}")
        message(FATAL_ERROR
            "CHARM_VIVID_STYLE_PATCH_SLOT_CAP must be <= CHARM_VIVID_SOA_MAX_NODES")
    endif()
    set(VIVID_STYLE_PATCH_SLOT_CAP ${CHARM_VIVID_STYLE_PATCH_SLOT_CAP})
    vivid_require_uint16_capacity("CHARM_VIVID_STYLE_CLASS_MAX" "${CHARM_VIVID_STYLE_CLASS_MAX}")
    vivid_require_uint16_capacity("CHARM_VIVID_STYLE_RULE_CAP" "${CHARM_VIVID_STYLE_RULE_CAP}")
    vivid_require_uint16_capacity("CHARM_VIVID_STYLE_METRICS_POOL_CAP" "${CHARM_VIVID_STYLE_METRICS_POOL_CAP}")
    if ("${CHARM_VIVID_STYLE_CLASS_MAX}" LESS 1)
        message(FATAL_ERROR "CHARM_VIVID_STYLE_CLASS_MAX must be > 0")
    endif()
    if ("${CHARM_VIVID_STYLE_CLASS_MAX}" GREATER 256)
        message(FATAL_ERROR "CHARM_VIVID_STYLE_CLASS_MAX must be <= 256")
    endif()
    if ("${CHARM_VIVID_STYLE_RULE_CAP}" LESS 1)
        message(FATAL_ERROR "CHARM_VIVID_STYLE_RULE_CAP must be > 0")
    endif()
    if ("${CHARM_VIVID_STYLE_METRICS_POOL_CAP}" LESS 1)
        message(FATAL_ERROR "CHARM_VIVID_STYLE_METRICS_POOL_CAP must be > 0")
    endif()
    if ("${CHARM_VIVID_STYLE_METRICS_POOL_CAP}" GREATER 255)
        message(FATAL_ERROR "CHARM_VIVID_STYLE_METRICS_POOL_CAP must be <= 255")
    endif()
    set(VIVID_STYLE_CLASS_MAX ${CHARM_VIVID_STYLE_CLASS_MAX})
    set(VIVID_STYLE_RULE_CAP ${CHARM_VIVID_STYLE_RULE_CAP})
    set(VIVID_STYLE_METRICS_POOL_CAP ${CHARM_VIVID_STYLE_METRICS_POOL_CAP})
    foreach(_vivid_draw_cmd_capacity IN ITEMS
            CHARM_VIVID_DRAW_CMD_MAX_COMMANDS
            CHARM_VIVID_DRAW_CMD_TEXT_BYTES
            CHARM_VIVID_DRAW_CMD_BLOB_BYTES)
        vivid_require_nonnegative_integer(
            "${_vivid_draw_cmd_capacity}"
            "${${_vivid_draw_cmd_capacity}}")
        if ("${${_vivid_draw_cmd_capacity}}" LESS 1)
            message(FATAL_ERROR "${_vivid_draw_cmd_capacity} must be > 0")
        endif()
        if ("${${_vivid_draw_cmd_capacity}}" GREATER 4294967295)
            message(FATAL_ERROR "${_vivid_draw_cmd_capacity} must be <= 4294967295")
        endif()
    endforeach()
    set(VIVID_DRAW_CMD_MAX_COMMANDS ${CHARM_VIVID_DRAW_CMD_MAX_COMMANDS})
    set(VIVID_DRAW_CMD_TEXT_BYTES ${CHARM_VIVID_DRAW_CMD_TEXT_BYTES})
    set(VIVID_DRAW_CMD_BLOB_BYTES ${CHARM_VIVID_DRAW_CMD_BLOB_BYTES})
    vivid_require_nonnegative_integer(
        "CHARM_VIVID_MAX_HOT_STACK_FRAME_BYTES"
        "${CHARM_VIVID_MAX_HOT_STACK_FRAME_BYTES}")
    if ("${CHARM_VIVID_MAX_HOT_STACK_FRAME_BYTES}" LESS 1)
        message(FATAL_ERROR "CHARM_VIVID_MAX_HOT_STACK_FRAME_BYTES must be > 0")
    endif()
    set(VIVID_MAX_HOT_STACK_FRAME_BYTES ${CHARM_VIVID_MAX_HOT_STACK_FRAME_BYTES})
    set(VIVID_DRAW_DETAIL_EVIDENCE ${_vivid_draw_detail_evidence})
    if ("${CHARM_VIVID_RUNTIME_SCENE_INSTANCES}" STREQUAL "")
        if (VIVID_STATIC_MEMORY_ADMISSION_REQUIRED)
            message(FATAL_ERROR
                "CHARM_VIVID_FEATURESET=${CHARM_VIVID_FEATURESET} requires explicit "
                "CHARM_VIVID_RUNTIME_SCENE_INSTANCES")
        endif()
        set(VIVID_RUNTIME_SCENE_INSTANCES 1)
    else()
        vivid_require_nonnegative_integer(
            "CHARM_VIVID_RUNTIME_SCENE_INSTANCES"
            "${CHARM_VIVID_RUNTIME_SCENE_INSTANCES}")
        if ("${CHARM_VIVID_RUNTIME_SCENE_INSTANCES}" LESS 1)
            message(FATAL_ERROR "CHARM_VIVID_RUNTIME_SCENE_INSTANCES must be > 0")
        endif()
        set(VIVID_RUNTIME_SCENE_INSTANCES ${CHARM_VIVID_RUNTIME_SCENE_INSTANCES})
    endif()

    set(SOA_POOL_CAP_DEFAULT "kDefaultPoolCap")
    set(SOA_TEXT_ARENA_BYTES "kDefaultTextArenaBytes")
    function(soa_compute_cap out_var peak floor scale_num scale_den extra)
        math(EXPR _cap_raw "(${peak} * ${scale_num} + (${scale_den} - 1)) / ${scale_den} + ${extra}")
        if (_cap_raw LESS ${floor})
            set(_cap_raw ${floor})
        endif()
        set(${out_var} ${_cap_raw} PARENT_SCOPE)
    endfunction()

    # Desktop demo scenes now include more static chrome and popup content at once.
    # Keep a little extra headroom for the highest-churn payload kinds in FULL builds.
    set(SOA_POOL_PEAK_LABEL 72)
    set(SOA_POOL_PEAK_BUTTON 40)
    # TODO(player-vivid-md3): 将 Label/Button/Image 等关键 SoA pool cap 做成更显式的产品级可调配置或 profile。
    # 当前估算已经被 Player 的 Home + Now Playing 实页验证过会逼近甚至打满默认容量，后续移植 MCU 前应收敛这套机制。
    set(SOA_POOL_PEAK_IMAGE 16)
    set(SOA_POOL_PEAK_TEXT_INPUT 1)
    set(SOA_POOL_PEAK_TEXT_AREA 1)
    set(SOA_POOL_PEAK_NUMBER_INPUT 1)
    set(SOA_POOL_PEAK_SEGMENTED_CONTROL 1)
    set(SOA_POOL_PEAK_STEPPER 1)
    set(SOA_POOL_PEAK_TOGGLE_GROUP 1)
    set(SOA_POOL_PEAK_CHECKBOX 2)
    set(SOA_POOL_PEAK_RADIO 1)
    set(SOA_POOL_PEAK_LIST_ITEM 10)
    set(SOA_POOL_PEAK_TEXT_LIST 1)
    set(SOA_POOL_PEAK_LIST_VIEW 1)
    set(SOA_POOL_PEAK_TABLE_VIEW 1)
    set(SOA_POOL_PEAK_TREE_VIEW 1)
    set(SOA_POOL_PEAK_NUMBER_LIST 1)
    set(SOA_POOL_PEAK_ROLLER 1)
    set(SOA_POOL_PEAK_SWITCH 1)
    set(SOA_POOL_PEAK_SLIDER 8)
    set(SOA_POOL_PEAK_PROGRESS 6)
    set(SOA_POOL_PEAK_SCROLLBAR 2)
    set(SOA_POOL_PEAK_LIST 1)
    set(SOA_POOL_PEAK_SCROLL_CONTAINER 2)
    set(SOA_POOL_PEAK_SPINNER 1)

    set(SOA_POOL_FLOOR_LABEL 1)
    set(SOA_POOL_FLOOR_BUTTON 1)
    set(SOA_POOL_FLOOR_IMAGE 1)
    set(SOA_POOL_FLOOR_TEXT_INPUT 1)
    set(SOA_POOL_FLOOR_TEXT_AREA 1)
    set(SOA_POOL_FLOOR_NUMBER_INPUT 1)
    set(SOA_POOL_FLOOR_SEGMENTED_CONTROL 1)
    set(SOA_POOL_FLOOR_STEPPER 1)
    set(SOA_POOL_FLOOR_TOGGLE_GROUP 1)
    set(SOA_POOL_FLOOR_CHECKBOX 1)
    set(SOA_POOL_FLOOR_RADIO 1)
    set(SOA_POOL_FLOOR_LIST_ITEM 1)
    set(SOA_POOL_FLOOR_TEXT_LIST 1)
    set(SOA_POOL_FLOOR_LIST_VIEW 1)
    set(SOA_POOL_FLOOR_TABLE_VIEW 1)
    set(SOA_POOL_FLOOR_TREE_VIEW 1)
    set(SOA_POOL_FLOOR_NUMBER_LIST 1)
    set(SOA_POOL_FLOOR_ROLLER 1)
    set(SOA_POOL_FLOOR_SWITCH 1)
    set(SOA_POOL_FLOOR_SLIDER 1)
    set(SOA_POOL_FLOOR_PROGRESS 1)
    set(SOA_POOL_FLOOR_SCROLLBAR 1)
    set(SOA_POOL_FLOOR_LIST 1)
    set(SOA_POOL_FLOOR_SCROLL_CONTAINER 1)
    set(SOA_POOL_FLOOR_SPINNER 1)

    if (CHARM_VIVID_FEATURESET STREQUAL "MCU_MIN")
        soa_compute_cap(SOA_POOL_CAP_LABEL "${SOA_POOL_PEAK_LABEL}" "${SOA_POOL_FLOOR_LABEL}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_BUTTON "${SOA_POOL_PEAK_BUTTON}" "${SOA_POOL_FLOOR_BUTTON}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_IMAGE "${SOA_POOL_PEAK_IMAGE}" "${SOA_POOL_FLOOR_IMAGE}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_TEXT_INPUT "${SOA_POOL_PEAK_TEXT_INPUT}" "${SOA_POOL_FLOOR_TEXT_INPUT}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_TEXT_AREA "${SOA_POOL_PEAK_TEXT_AREA}" "${SOA_POOL_FLOOR_TEXT_AREA}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_NUMBER_INPUT "${SOA_POOL_PEAK_NUMBER_INPUT}" "${SOA_POOL_FLOOR_NUMBER_INPUT}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_SEGMENTED_CONTROL "${SOA_POOL_PEAK_SEGMENTED_CONTROL}" "${SOA_POOL_FLOOR_SEGMENTED_CONTROL}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_STEPPER "${SOA_POOL_PEAK_STEPPER}" "${SOA_POOL_FLOOR_STEPPER}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_TOGGLE_GROUP "${SOA_POOL_PEAK_TOGGLE_GROUP}" "${SOA_POOL_FLOOR_TOGGLE_GROUP}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_CHECKBOX "${SOA_POOL_PEAK_CHECKBOX}" "${SOA_POOL_FLOOR_CHECKBOX}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_RADIO "${SOA_POOL_PEAK_RADIO}" "${SOA_POOL_FLOOR_RADIO}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_LIST_ITEM "${SOA_POOL_PEAK_LIST_ITEM}" "${SOA_POOL_FLOOR_LIST_ITEM}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_TEXT_LIST "${SOA_POOL_PEAK_TEXT_LIST}" "${SOA_POOL_FLOOR_TEXT_LIST}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_LIST_VIEW "${SOA_POOL_PEAK_LIST_VIEW}" "${SOA_POOL_FLOOR_LIST_VIEW}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_TABLE_VIEW "${SOA_POOL_PEAK_TABLE_VIEW}" "${SOA_POOL_FLOOR_TABLE_VIEW}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_TREE_VIEW "${SOA_POOL_PEAK_TREE_VIEW}" "${SOA_POOL_FLOOR_TREE_VIEW}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_NUMBER_LIST "${SOA_POOL_PEAK_NUMBER_LIST}" "${SOA_POOL_FLOOR_NUMBER_LIST}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_ROLLER "${SOA_POOL_PEAK_ROLLER}" "${SOA_POOL_FLOOR_ROLLER}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_SWITCH "${SOA_POOL_PEAK_SWITCH}" "${SOA_POOL_FLOOR_SWITCH}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_SLIDER "${SOA_POOL_PEAK_SLIDER}" "${SOA_POOL_FLOOR_SLIDER}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_PROGRESS "${SOA_POOL_PEAK_PROGRESS}" "${SOA_POOL_FLOOR_PROGRESS}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_SCROLLBAR "${SOA_POOL_PEAK_SCROLLBAR}" "${SOA_POOL_FLOOR_SCROLLBAR}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_LIST "${SOA_POOL_PEAK_LIST}" "${SOA_POOL_FLOOR_LIST}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_SCROLL_CONTAINER "${SOA_POOL_PEAK_SCROLL_CONTAINER}" "${SOA_POOL_FLOOR_SCROLL_CONTAINER}" 11 10 1)
        soa_compute_cap(SOA_POOL_CAP_SPINNER "${SOA_POOL_PEAK_SPINNER}" "${SOA_POOL_FLOOR_SPINNER}" 11 10 1)
    elseif (CHARM_VIVID_FEATURESET STREQUAL "FULL" OR CHARM_VIVID_FEATURESET STREQUAL "PRODUCT")
        set(SOA_TEXT_ARENA_BYTES 32768)
        soa_compute_cap(SOA_POOL_CAP_LABEL "${SOA_POOL_PEAK_LABEL}" "${SOA_POOL_FLOOR_LABEL}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_BUTTON "${SOA_POOL_PEAK_BUTTON}" "${SOA_POOL_FLOOR_BUTTON}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_IMAGE "${SOA_POOL_PEAK_IMAGE}" "${SOA_POOL_FLOOR_IMAGE}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_TEXT_INPUT "${SOA_POOL_PEAK_TEXT_INPUT}" "${SOA_POOL_FLOOR_TEXT_INPUT}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_TEXT_AREA "${SOA_POOL_PEAK_TEXT_AREA}" "${SOA_POOL_FLOOR_TEXT_AREA}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_NUMBER_INPUT "${SOA_POOL_PEAK_NUMBER_INPUT}" "${SOA_POOL_FLOOR_NUMBER_INPUT}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_SEGMENTED_CONTROL "${SOA_POOL_PEAK_SEGMENTED_CONTROL}" "${SOA_POOL_FLOOR_SEGMENTED_CONTROL}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_STEPPER "${SOA_POOL_PEAK_STEPPER}" "${SOA_POOL_FLOOR_STEPPER}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_TOGGLE_GROUP "${SOA_POOL_PEAK_TOGGLE_GROUP}" "${SOA_POOL_FLOOR_TOGGLE_GROUP}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_CHECKBOX "${SOA_POOL_PEAK_CHECKBOX}" "${SOA_POOL_FLOOR_CHECKBOX}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_RADIO "${SOA_POOL_PEAK_RADIO}" "${SOA_POOL_FLOOR_RADIO}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_LIST_ITEM "${SOA_POOL_PEAK_LIST_ITEM}" "${SOA_POOL_FLOOR_LIST_ITEM}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_TEXT_LIST "${SOA_POOL_PEAK_TEXT_LIST}" "${SOA_POOL_FLOOR_TEXT_LIST}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_LIST_VIEW "${SOA_POOL_PEAK_LIST_VIEW}" "${SOA_POOL_FLOOR_LIST_VIEW}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_TABLE_VIEW "${SOA_POOL_PEAK_TABLE_VIEW}" "${SOA_POOL_FLOOR_TABLE_VIEW}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_TREE_VIEW "${SOA_POOL_PEAK_TREE_VIEW}" "${SOA_POOL_FLOOR_TREE_VIEW}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_NUMBER_LIST "${SOA_POOL_PEAK_NUMBER_LIST}" "${SOA_POOL_FLOOR_NUMBER_LIST}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_ROLLER "${SOA_POOL_PEAK_ROLLER}" "${SOA_POOL_FLOOR_ROLLER}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_SWITCH "${SOA_POOL_PEAK_SWITCH}" "${SOA_POOL_FLOOR_SWITCH}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_SLIDER "${SOA_POOL_PEAK_SLIDER}" "${SOA_POOL_FLOOR_SLIDER}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_PROGRESS "${SOA_POOL_PEAK_PROGRESS}" "${SOA_POOL_FLOOR_PROGRESS}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_SCROLLBAR "${SOA_POOL_PEAK_SCROLLBAR}" "${SOA_POOL_FLOOR_SCROLLBAR}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_LIST "${SOA_POOL_PEAK_LIST}" "${SOA_POOL_FLOOR_LIST}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_SCROLL_CONTAINER "${SOA_POOL_PEAK_SCROLL_CONTAINER}" "${SOA_POOL_FLOOR_SCROLL_CONTAINER}" 5 4 2)
        soa_compute_cap(SOA_POOL_CAP_SPINNER "${SOA_POOL_PEAK_SPINNER}" "${SOA_POOL_FLOOR_SPINNER}" 5 4 2)
        if (SOA_POOL_CAP_LABEL LESS 96)
            set(SOA_POOL_CAP_LABEL 96)
        endif()
        if (SOA_POOL_CAP_BUTTON LESS 56)
            set(SOA_POOL_CAP_BUTTON 56)
        endif()
        if (SOA_POOL_CAP_IMAGE LESS 24)
            set(SOA_POOL_CAP_IMAGE 24)
        endif()
    endif()

    if (DEFINED CHARM_VIVID_SOA_TEXT_ARENA_BYTES
        AND NOT "${CHARM_VIVID_SOA_TEXT_ARENA_BYTES}" STREQUAL "")
        vivid_require_uint16_capacity(
            "CHARM_VIVID_SOA_TEXT_ARENA_BYTES"
            "${CHARM_VIVID_SOA_TEXT_ARENA_BYTES}")
        if ("${CHARM_VIVID_SOA_TEXT_ARENA_BYTES}" LESS 1)
            message(FATAL_ERROR "CHARM_VIVID_SOA_TEXT_ARENA_BYTES must be > 0")
        endif()
        set(SOA_TEXT_ARENA_BYTES "${CHARM_VIVID_SOA_TEXT_ARENA_BYTES}")
    endif()
    if (SOA_TEXT_ARENA_BYTES STREQUAL "kDefaultTextArenaBytes")
        set(_vivid_text_arena_bytes 8192)
    else()
        set(_vivid_text_arena_bytes ${SOA_TEXT_ARENA_BYTES})
    endif()

    if (CHARM_VIVID_FEATURESET STREQUAL "PRODUCT")
        get_property(_vivid_catalog_payload_pools GLOBAL PROPERTY VIVID_PAYLOAD_POOLS)
        foreach(_pool IN LISTS _vivid_catalog_payload_pools)
            _vivid_payload_get("${_pool}" CAP_KIND _cap_kind)
            vivid_payload_capacity_from_profile(
                _capacity "${_pool}" "${_vivid_profile_payload_capacities}")
            vivid_payload_cap_variable_name(_capacity_var "${_cap_kind}")
            set(${_capacity_var} "${_capacity}")
        endforeach()
    endif()

    math(EXPR _vivid_payload_slot_cap_total
        "${SOA_POOL_CAP_LABEL} + ${SOA_POOL_CAP_BUTTON} + ${SOA_POOL_CAP_IMAGE} + ${SOA_POOL_CAP_TEXT_INPUT} + ${SOA_POOL_CAP_TEXT_AREA} + ${SOA_POOL_CAP_NUMBER_INPUT} + ${SOA_POOL_CAP_SEGMENTED_CONTROL} + ${SOA_POOL_CAP_STEPPER} + ${SOA_POOL_CAP_TOGGLE_GROUP} + ${SOA_POOL_CAP_CHECKBOX} + ${SOA_POOL_CAP_RADIO} + ${SOA_POOL_CAP_LIST_ITEM} + ${SOA_POOL_CAP_TEXT_LIST} + ${SOA_POOL_CAP_LIST_VIEW} + ${SOA_POOL_CAP_TABLE_VIEW} + ${SOA_POOL_CAP_TREE_VIEW} + ${SOA_POOL_CAP_NUMBER_LIST} + ${SOA_POOL_CAP_ROLLER} + ${SOA_POOL_CAP_SWITCH} + ${SOA_POOL_CAP_SLIDER} + ${SOA_POOL_CAP_PROGRESS} + ${SOA_POOL_CAP_SCROLLBAR} + ${SOA_POOL_CAP_LIST} + ${SOA_POOL_CAP_SCROLL_CONTAINER} + ${SOA_POOL_CAP_SPINNER}")

    # Conservative configure-time model. scene.cppm validates this upper bound
    # against target-ABI sizeof values, so configuration drift cannot undercount.
    if(VIVID_DRAW_DETAIL_EVIDENCE)
        set(_vivid_draw_cmd_record_upper_bytes 64)
        set(_vivid_soa_traversal_frame_upper_bytes 56)
        set(_vivid_soa_node_upper_bytes 211)
    else()
        set(_vivid_draw_cmd_record_upper_bytes 60)
        set(_vivid_soa_traversal_frame_upper_bytes 52)
        set(_vivid_soa_node_upper_bytes 209)
    endif()
    math(EXPR _vivid_draw_cmd_arena_upper_bytes
        "${VIVID_DRAW_CMD_MAX_COMMANDS} * ${_vivid_draw_cmd_record_upper_bytes}")
    if(_vivid_draw_cmd_arena_upper_bytes LESS_EQUAL 65536)
        set(_vivid_draw_cmd_offset_bytes 2)
    else()
        set(_vivid_draw_cmd_offset_bytes 4)
    endif()
    math(EXPR _vivid_draw_cmd_buffer_upper_bytes
        "${_vivid_draw_cmd_arena_upper_bytes} + ${VIVID_DRAW_CMD_MAX_COMMANDS} * ${_vivid_draw_cmd_offset_bytes} + ${VIVID_DRAW_CMD_TEXT_BYTES} + ${VIVID_DRAW_CMD_BLOB_BYTES} + 4096")
    math(EXPR _vivid_draw_cmd_compaction_workspace_upper_bytes
        "${VIVID_DRAW_CMD_MAX_COMMANDS} * ${_vivid_draw_cmd_offset_bytes} + 2048")
    set(_vivid_draw_cmd_executor_workspace_upper_bytes 4096)
    set(VIVID_DRAW_CMD_RECORD_UPPER_BYTES ${_vivid_draw_cmd_record_upper_bytes})
    set(VIVID_DRAW_CMD_BUFFER_UPPER_BYTES ${_vivid_draw_cmd_buffer_upper_bytes})
    set(VIVID_DRAW_CMD_EXECUTOR_WORKSPACE_UPPER_BYTES
        ${_vivid_draw_cmd_executor_workspace_upper_bytes})
    math(EXPR _vivid_soa_traversal_workspace_upper_bytes
        "${CHARM_VIVID_SOA_MAX_NODES} * ${_vivid_soa_traversal_frame_upper_bytes}")
    set(_vivid_semantic_slot_upper_bytes 16)
    set(_vivid_semantic_pool_fixed_upper_bytes 32)
    math(EXPR _vivid_semantic_pool_upper_bytes
        "${VIVID_SEMANTIC_SLOT_CAP} * ${_vivid_semantic_slot_upper_bytes} + ${_vivid_semantic_pool_fixed_upper_bytes}")
    set(_vivid_style_patch_slot_upper_bytes 256)
    math(EXPR _vivid_style_patch_pool_upper_bytes
        "${VIVID_STYLE_PATCH_SLOT_CAP} * ${_vivid_style_patch_slot_upper_bytes}")
    set(_vivid_payload_slot_upper_bytes 512)
    set(_vivid_soa_fixed_upper_bytes 65536)
    set(_vivid_scene_fixed_upper_bytes 65536)
    set(_vivid_global_fixed_upper_bytes 262144)
    set(_vivid_runtime_globals_upper_bytes 1024)
    set(VIVID_RUNTIME_GLOBALS_UPPER_BYTES ${_vivid_runtime_globals_upper_bytes})
    math(EXPR _vivid_pixel_snapshot_upper_bytes
        "${CHARM_VIVID_LAYER_CACHE_SLOTS} * ${CHARM_VIVID_LAYER_CACHE_WIDTH} * ${CHARM_VIVID_LAYER_CACHE_HEIGHT} * ${_vivid_screen_bytes_per_pixel}")
    math(EXPR _vivid_command_buffer_upper_bytes
        "(${CHARM_VIVID_LAYER_CACHE_SLOTS} + 1) * ${_vivid_draw_cmd_buffer_upper_bytes}")
    math(EXPR _vivid_soa_upper_bytes
        "${CHARM_VIVID_SOA_MAX_NODES} * ${_vivid_soa_node_upper_bytes} + ${_vivid_semantic_pool_upper_bytes} + ${_vivid_style_patch_pool_upper_bytes} + ${_vivid_payload_slot_cap_total} * ${_vivid_payload_slot_upper_bytes} + ${_vivid_text_arena_bytes} + ${_vivid_soa_fixed_upper_bytes}")
    math(EXPR _vivid_scene_upper_bytes
        "${_vivid_pixel_snapshot_upper_bytes} + ${_vivid_command_buffer_upper_bytes} + ${_vivid_draw_cmd_compaction_workspace_upper_bytes} + ${_vivid_draw_cmd_executor_workspace_upper_bytes} + ${_vivid_soa_traversal_workspace_upper_bytes} + ${_vivid_soa_upper_bytes} + ${_vivid_scene_fixed_upper_bytes}")
    math(EXPR _vivid_global_upper_bytes
        "${_vivid_global_fixed_upper_bytes} + ${_vivid_runtime_globals_upper_bytes} + ${CHARM_VIVID_STYLE_CLASS_MAX} * 256 + ${CHARM_VIVID_STYLE_RULE_CAP} * 256 + ${CHARM_VIVID_STYLE_METRICS_POOL_CAP} * 64")
    math(EXPR VIVID_STATIC_MEMORY_UPPER_BOUND_BYTES
        "${VIVID_RUNTIME_SCENE_INSTANCES} * ${_vivid_scene_upper_bytes} + ${_vivid_global_upper_bytes}")

    if (VIVID_STATIC_MEMORY_ADMISSION_REQUIRED)
        if ("${CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES}" STREQUAL "")
            message(FATAL_ERROR
                "CHARM_VIVID_FEATURESET=${CHARM_VIVID_FEATURESET} requires explicit "
                "CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES")
        endif()
        if ("${CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES}" STREQUAL "")
            message(FATAL_ERROR
                "CHARM_VIVID_FEATURESET=${CHARM_VIVID_FEATURESET} requires explicit "
                "CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES")
        endif()
        vivid_require_nonnegative_integer(
            "CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES"
            "${CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES}")
        vivid_require_nonnegative_integer(
            "CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES"
            "${CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES}")
        if ("${CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES}" LESS 1)
            message(FATAL_ERROR "CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES must be > 0")
        endif()
        if ("${CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES}" LESS 1)
            message(FATAL_ERROR "CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES must be > 0")
        endif()
        if (VIVID_STATIC_MEMORY_UPPER_BOUND_BYTES GREATER CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES)
            message(FATAL_ERROR
                "Vivid static memory admission failed: featureset=${CHARM_VIVID_FEATURESET} "
                "upper_bound=${VIVID_STATIC_MEMORY_UPPER_BOUND_BYTES} "
                "min_headroom=${CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES} "
                "budget=${CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES}")
        endif()
        set(VIVID_STATIC_MEMORY_BUDGET_BYTES ${CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES})
        set(VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES ${CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES})
        math(EXPR _vivid_configured_headroom_bytes
            "${VIVID_STATIC_MEMORY_BUDGET_BYTES} - ${VIVID_STATIC_MEMORY_UPPER_BOUND_BYTES}")
        if (_vivid_configured_headroom_bytes LESS VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES)
            message(FATAL_ERROR
                "Vivid static memory admission failed: featureset=${CHARM_VIVID_FEATURESET} "
                "upper_bound=${VIVID_STATIC_MEMORY_UPPER_BOUND_BYTES} "
                "min_headroom=${VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES} "
                "budget=${VIVID_STATIC_MEMORY_BUDGET_BYTES}")
        endif()
        set(_vivid_admission_status admitted)
    else()
        set(VIVID_STATIC_MEMORY_BUDGET_BYTES 0)
        set(VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES 0)
        set(_vivid_configured_headroom_bytes 0)
        set(_vivid_admission_status profile_only)
    endif()

    set(_vivid_generated_dir
        "${CMAKE_CURRENT_BINARY_DIR}/generated/vivid/${target_name}/${_vivid_profile_name}")
    file(MAKE_DIRECTORY "${_vivid_generated_dir}")

    set(_vivid_static_memory_manifest
        "featureset=${CHARM_VIVID_FEATURESET}\n"
        "profile=${_vivid_profile_name}\n"
        "profile_fingerprint=${_vivid_profile_fingerprint}\n"
        "target_fingerprint=${_vivid_target_fingerprint}\n"
        "admission_required=${VIVID_STATIC_MEMORY_ADMISSION_REQUIRED}\n"
        "status=${_vivid_admission_status}\n"
        "scene_instances=${VIVID_RUNTIME_SCENE_INSTANCES}\n"
        "pixel_snapshot_upper_bytes=${_vivid_pixel_snapshot_upper_bytes}\n"
        "command_buffer_upper_bytes=${_vivid_command_buffer_upper_bytes}\n"
        "draw_cmd_max_commands=${VIVID_DRAW_CMD_MAX_COMMANDS}\n"
        "draw_cmd_text_bytes=${VIVID_DRAW_CMD_TEXT_BYTES}\n"
        "draw_cmd_blob_bytes=${VIVID_DRAW_CMD_BLOB_BYTES}\n"
        "draw_cmd_record_upper_bytes=${_vivid_draw_cmd_record_upper_bytes}\n"
        "draw_cmd_arena_upper_bytes=${_vivid_draw_cmd_arena_upper_bytes}\n"
        "draw_cmd_offset_bytes=${_vivid_draw_cmd_offset_bytes}\n"
        "draw_cmd_buffer_upper_bytes=${_vivid_draw_cmd_buffer_upper_bytes}\n"
        "draw_detail_evidence=${VIVID_DRAW_DETAIL_EVIDENCE}\n"
        "soa_max_nodes=${VIVID_SOA_MAX_NODES}\n"
        "soa_node_upper_bytes=${_vivid_soa_node_upper_bytes}\n"
        "semantic_slot_cap=${VIVID_SEMANTIC_SLOT_CAP}\n"
        "semantic_pool_upper_bytes=${_vivid_semantic_pool_upper_bytes}\n"
        "style_patch_slot_cap=${VIVID_STYLE_PATCH_SLOT_CAP}\n"
        "style_patch_pool_upper_bytes=${_vivid_style_patch_pool_upper_bytes}\n"
        "max_hot_stack_frame_bytes=${VIVID_MAX_HOT_STACK_FRAME_BYTES}\n"
        "draw_cmd_compaction_workspace_upper_bytes=${_vivid_draw_cmd_compaction_workspace_upper_bytes}\n"
        "draw_cmd_executor_workspace_upper_bytes=${_vivid_draw_cmd_executor_workspace_upper_bytes}\n"
        "soa_traversal_frame_upper_bytes=${_vivid_soa_traversal_frame_upper_bytes}\n"
        "soa_traversal_workspace_upper_bytes=${_vivid_soa_traversal_workspace_upper_bytes}\n"
        "soa_upper_bytes=${_vivid_soa_upper_bytes}\n"
        "scene_upper_bytes=${_vivid_scene_upper_bytes}\n"
        "runtime_globals_upper_bytes=${_vivid_runtime_globals_upper_bytes}\n"
        "global_upper_bytes=${_vivid_global_upper_bytes}\n"
        "upper_bound_bytes=${VIVID_STATIC_MEMORY_UPPER_BOUND_BYTES}\n"
        "budget_bytes=${VIVID_STATIC_MEMORY_BUDGET_BYTES}\n"
        "min_headroom_bytes=${VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES}\n"
        "configured_headroom_bytes=${_vivid_configured_headroom_bytes}\n")
    list(JOIN _vivid_static_memory_manifest "" _vivid_static_memory_manifest_text)
    file(WRITE
        "${_vivid_generated_dir}/static_memory_admission.txt"
        "${_vivid_static_memory_manifest_text}")
    message(STATUS
        "Vivid static memory: featureset=${CHARM_VIVID_FEATURESET} "
        "upper_bound=${VIVID_STATIC_MEMORY_UPPER_BOUND_BYTES} "
        "budget=${VIVID_STATIC_MEMORY_BUDGET_BYTES} "
        "headroom=${_vivid_configured_headroom_bytes} "
        "status=${_vivid_admission_status}")

    if(CHARM_VIVID_FEATURESET STREQUAL "PRODUCT")
        set(_payload_json "{")
        set(_separator "")
        foreach(_entry IN LISTS _vivid_profile_payload_capacities)
            if(NOT _entry MATCHES "^([A-Za-z][A-Za-z0-9_]*)=([0-9]+)$")
                message(FATAL_ERROR "Invalid Vivid payload evidence entry '${_entry}'")
            endif()
            string(APPEND _payload_json
                "${_separator}\"${CMAKE_MATCH_1}\":${CMAKE_MATCH_2}")
            set(_separator ",")
        endforeach()
        string(APPEND _payload_json "}")

        set(_profile_roots_json_values ${_vivid_profile_roots})
        set(_profile_kinds_json_values ${_vivid_profile_widget_kinds})
        set(_profile_object_kinds_json_values ${_vivid_profile_object_widget_kinds})
        vivid_json_string_array(_profile_roots_json ${_profile_roots_json_values})
        vivid_json_string_array(_profile_kinds_json ${_profile_kinds_json_values})
        vivid_json_string_array(
            _profile_object_kinds_json ${_profile_object_kinds_json_values})
        file(WRITE "${_vivid_generated_dir}/profile.json"
            "{\n"
            "  \"schema\": 4,\n"
            "  \"name\": \"${_vivid_profile_name}\",\n"
            "  \"catalog_fingerprint\": \"${_vivid_catalog_fingerprint}\",\n"
            "  \"profile_fingerprint\": \"${_vivid_profile_fingerprint}\",\n"
            "  \"root_modules\": ${_profile_roots_json},\n"
            "  \"widget_kinds\": ${_profile_kinds_json},\n"
            "  \"object_widget_kinds\": ${_profile_object_kinds_json},\n"
            "  \"payload_capacities\": ${_payload_json},\n"
            "  \"workset\": {\n"
            "    \"soa_max_nodes\": ${VIVID_SOA_MAX_NODES},\n"
            "    \"soa_text_arena_bytes\": ${_vivid_text_arena_bytes},\n"
            "    \"semantic_slot_cap\": ${VIVID_SEMANTIC_SLOT_CAP},\n"
            "    \"style_patch_slot_cap\": ${VIVID_STYLE_PATCH_SLOT_CAP},\n"
            "    \"style_class_max\": ${VIVID_STYLE_CLASS_MAX},\n"
            "    \"style_rule_cap\": ${VIVID_STYLE_RULE_CAP},\n"
            "    \"style_metrics_pool_cap\": ${VIVID_STYLE_METRICS_POOL_CAP},\n"
            "    \"draw_cmd_max_commands\": ${VIVID_DRAW_CMD_MAX_COMMANDS},\n"
            "    \"draw_cmd_text_bytes\": ${VIVID_DRAW_CMD_TEXT_BYTES},\n"
            "    \"draw_cmd_blob_bytes\": ${VIVID_DRAW_CMD_BLOB_BYTES},\n"
            "    \"float_widgets\": ${VIVID_ENABLE_FLOAT_WIDGETS}\n"
            "  }\n"
            "}\n")

        if(VIVID_DRAW_DETAIL_EVIDENCE)
            set(_draw_detail_json true)
        else()
            set(_draw_detail_json false)
        endif()
        file(WRITE "${_vivid_generated_dir}/target_envelope.json"
            "{\n"
            "  \"schema\": 1,\n"
            "  \"target\": \"${target_name}\",\n"
            "  \"profile\": \"${_vivid_profile_name}\",\n"
            "  \"profile_fingerprint\": \"${_vivid_profile_fingerprint}\",\n"
            "  \"target_fingerprint\": \"${_vivid_target_fingerprint}\",\n"
            "  \"screen\": {\"width\":${VIVID_SCREEN_WIDTH},\"height\":${VIVID_SCREEN_HEIGHT},\"pixel_format\":\"${CHARM_VIVID_SCREEN_PIXEL_FORMAT}\"},\n"
            "  \"layer_cache\": {\"slots\":${VIVID_LAYER_CACHE_SLOTS},\"width\":${VIVID_LAYER_CACHE_WIDTH},\"height\":${VIVID_LAYER_CACHE_HEIGHT}},\n"
            "  \"runtime_scene_instances\": ${VIVID_RUNTIME_SCENE_INSTANCES},\n"
            "  \"static_memory_budget_bytes\": ${VIVID_STATIC_MEMORY_BUDGET_BYTES},\n"
            "  \"static_memory_min_headroom_bytes\": ${VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES},\n"
            "  \"max_hot_stack_frame_bytes\": ${VIVID_MAX_HOT_STACK_FRAME_BYTES},\n"
            "  \"draw_detail_evidence\": ${_draw_detail_json}\n"
            "}\n")

        list(REMOVE_ITEM _vivid_external_requirements
            charm.core.config.generated
            charm.core.soa_pool_caps)
        set(_closure_modules_json_values ${_vivid_product_modules})
        set(_closure_external_json_values ${_vivid_external_requirements})
        list(SORT _closure_modules_json_values)
        list(SORT _closure_external_json_values)
        get_filename_component(_vivid_repo_root "${_VIVID_CMAKE_DIR}/../../.." ABSOLUTE)
        set(_closure_sources_json_values)
        foreach(_source IN LISTS _vivid_product_sources)
            file(RELATIVE_PATH _relative_source "${_vivid_repo_root}" "${_source}")
            file(TO_CMAKE_PATH "${_relative_source}" _relative_source)
            list(APPEND _closure_sources_json_values "${_relative_source}")
        endforeach()
        list(SORT _closure_sources_json_values)
        vivid_json_string_array(_closure_modules_json ${_closure_modules_json_values})
        vivid_json_string_array(_closure_sources_json ${_closure_sources_json_values})
        vivid_json_string_array(_closure_external_json ${_closure_external_json_values})
        file(WRITE "${_vivid_generated_dir}/module_closure.json"
            "{\n"
            "  \"schema\": 1,\n"
            "  \"target\": \"${target_name}\",\n"
            "  \"profile\": \"${_vivid_profile_name}\",\n"
            "  \"profile_fingerprint\": \"${_vivid_profile_fingerprint}\",\n"
            "  \"target_fingerprint\": \"${_vivid_target_fingerprint}\",\n"
            "  \"modules\": ${_closure_modules_json},\n"
            "  \"sources\": ${_closure_sources_json},\n"
            "  \"external_requirements\": ${_closure_external_json}\n"
            "}\n")

        file(WRITE "${_vivid_generated_dir}/admission.json"
            "{\n"
            "  \"schema\": 1,\n"
            "  \"target\": \"${target_name}\",\n"
            "  \"profile\": \"${_vivid_profile_name}\",\n"
            "  \"profile_fingerprint\": \"${_vivid_profile_fingerprint}\",\n"
            "  \"target_fingerprint\": \"${_vivid_target_fingerprint}\",\n"
            "  \"status\": \"${_vivid_admission_status}\",\n"
            "  \"payload_slot_capacity_total\": ${_vivid_payload_slot_cap_total},\n"
            "  \"static_memory\": {\n"
            "    \"upper_bound_bytes\": ${VIVID_STATIC_MEMORY_UPPER_BOUND_BYTES},\n"
            "    \"budget_bytes\": ${VIVID_STATIC_MEMORY_BUDGET_BYTES},\n"
            "    \"min_headroom_bytes\": ${VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES},\n"
            "    \"configured_headroom_bytes\": ${_vivid_configured_headroom_bytes},\n"
            "    \"scene_upper_bytes\": ${_vivid_scene_upper_bytes},\n"
            "    \"runtime_globals_upper_bytes\": ${_vivid_runtime_globals_upper_bytes},\n"
            "    \"global_upper_bytes\": ${_vivid_global_upper_bytes},\n"
            "    \"command_buffer_upper_bytes\": ${_vivid_command_buffer_upper_bytes},\n"
            "    \"draw_cmd_record_upper_bytes\": ${_vivid_draw_cmd_record_upper_bytes},\n"
            "    \"draw_cmd_arena_upper_bytes\": ${_vivid_draw_cmd_arena_upper_bytes},\n"
            "    \"draw_cmd_offset_bytes\": ${_vivid_draw_cmd_offset_bytes},\n"
            "    \"draw_cmd_buffer_upper_bytes\": ${_vivid_draw_cmd_buffer_upper_bytes},\n"
            "    \"compaction_workspace_upper_bytes\": ${_vivid_draw_cmd_compaction_workspace_upper_bytes},\n"
            "    \"executor_workspace_upper_bytes\": ${_vivid_draw_cmd_executor_workspace_upper_bytes},\n"
            "    \"soa_node_upper_bytes\": ${_vivid_soa_node_upper_bytes},\n"
            "    \"semantic_pool_upper_bytes\": ${_vivid_semantic_pool_upper_bytes},\n"
            "    \"style_patch_pool_upper_bytes\": ${_vivid_style_patch_pool_upper_bytes},\n"
            "    \"draw_detail_evidence\": ${_draw_detail_json},\n"
            "    \"soa_traversal_frame_upper_bytes\": ${_vivid_soa_traversal_frame_upper_bytes},\n"
            "    \"soa_traversal_workspace_upper_bytes\": ${_vivid_soa_traversal_workspace_upper_bytes}\n"
            "  },\n"
            "  \"max_hot_stack_frame_bytes\": ${VIVID_MAX_HOT_STACK_FRAME_BYTES}\n"
            "}\n")
    endif()

    set(vivid_pool_caps_output_dir "${_vivid_generated_dir}")
    file(MAKE_DIRECTORY "${vivid_pool_caps_output_dir}")
    vivid_generate_widget_catalog("${vivid_pool_caps_output_dir}")
    set(vivid_features_header "${vivid_pool_caps_output_dir}/vivid_features.generated.hpp")
    set(vivid_config_cppm "${vivid_pool_caps_output_dir}/config.generated.cppm")
    set(vivid_pool_caps_cppm "${vivid_pool_caps_output_dir}/soa_pool_caps.cppm")
    configure_file(
        "${_VIVID_CMAKE_DIR}/cmake/features.generated.hpp.in"
        "${vivid_features_header}"
        @ONLY)

    if(VIVID_STATIC_MEMORY_ADMISSION_REQUIRED)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            message(FATAL_ERROR
                "Vivid PRODUCT/MCU_MIN stack admission requires GNU or Clang -fstack-usage")
        endif()
        target_compile_options(${target_name} PRIVATE -fstack-usage)
        get_target_property(
            _vivid_stack_usage_root ${target_name} CHARM_VIVID_STACK_USAGE_ROOT)
        if(NOT _vivid_stack_usage_root)
            set(_vivid_stack_usage_root
                "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target_name}.dir/Modules/ui/vivid")
        endif()
        set(_vivid_stack_usage_manifest
            "${_vivid_generated_dir}/stack_usage_manifest.txt")
    endif()
    configure_file(
        "${_VIVID_CMAKE_DIR}/cmake/config.generated.cppm.in"
        "${vivid_config_cppm}"
        @ONLY)
    configure_file(
        "${_VIVID_CMAKE_DIR}/cmake/soa_pool_caps.cppm.in"
        "${vivid_pool_caps_cppm}"
        @ONLY)

    list(APPEND ${module_list_var} "${vivid_config_cppm}")
    list(APPEND ${module_list_var} "${vivid_pool_caps_cppm}")
    set(${base_dirs_var} "${${base_dirs_var}}" "${vivid_pool_caps_output_dir}" PARENT_SCOPE)
    target_include_directories(${target_name} PRIVATE
        "${_VIVID_CMAKE_DIR}/core"
        "${vivid_pool_caps_output_dir}")

    if (CHARM_VIVID_FEATURESET STREQUAL "PRODUCT")
        list(FILTER ${module_list_var} EXCLUDE REGEX
            "[/\\\\]Modules[/\\\\]ui[/\\\\]vivid[/\\\\].*\\.cppm$")
        list(APPEND ${module_list_var} ${_vivid_product_sources})
    endif()
    if (CHARM_VIVID_FEATURESET STREQUAL "MCU_MIN")
        list(FILTER ${module_list_var} EXCLUDE REGEX "/Modules/ui/vivid/widgets/")
    endif()
    if (CHARM_VIVID_FEATURESET STREQUAL "MCU_MIN")
        list(REMOVE_ITEM ${module_list_var}
            "${PROJECT_SOURCE_DIR}/Modules/ui/vivid/core/object.cppm"
            "${PROJECT_SOURCE_DIR}/Modules/ui/vivid/core/input_interaction.cppm"
            "${PROJECT_SOURCE_DIR}/Modules/ui/vivid/core/perf_overlay_runtime.cppm"
            "${PROJECT_SOURCE_DIR}/Modules/ui/vivid/core/virtual_list.cppm"
        )
    endif()

    if(VIVID_STATIC_MEMORY_ADMISSION_REQUIRED)
        get_filename_component(
            _vivid_repo_root "${_VIVID_CMAKE_DIR}/../../.." ABSOLUTE)
        set(_vivid_stack_usage_source_manifest
            "${_vivid_generated_dir}/stack_usage_sources.txt")
        set(_vivid_stack_usage_sources "")
        foreach(_vivid_module_source IN LISTS ${module_list_var})
            get_filename_component(
                _vivid_module_source_abs "${_vivid_module_source}" ABSOLUTE)
            file(TO_CMAKE_PATH
                "${_vivid_module_source_abs}" _vivid_module_source_normalized)
            string(FIND
                "${_vivid_module_source_normalized}" "/Modules/ui/vivid/" _vivid_module_source_pos)
            if(_vivid_module_source_pos EQUAL -1)
                continue()
            endif()
            file(RELATIVE_PATH
                _vivid_module_source_relative
                "${_vivid_repo_root}"
                "${_vivid_module_source_abs}")
            file(TO_CMAKE_PATH
                "${_vivid_module_source_relative}" _vivid_module_source_relative)
            string(APPEND _vivid_stack_usage_sources
                "${_vivid_module_source_relative}\n")
        endforeach()
        file(WRITE
            "${_vivid_stack_usage_source_manifest}"
            "${_vivid_stack_usage_sources}")
        add_custom_command(TARGET ${target_name} POST_BUILD
            BYPRODUCTS "${_vivid_stack_usage_manifest}"
            COMMAND ${CMAKE_COMMAND}
                "-DVIVID_STACK_USAGE_ROOT=${_vivid_stack_usage_root}"
                "-DVIVID_STACK_USAGE_SOURCE_MANIFEST=${_vivid_stack_usage_source_manifest}"
                "-DVIVID_STACK_USAGE_OUT=${_vivid_stack_usage_manifest}"
                "-DVIVID_STACK_USAGE_MAX_BYTES=${VIVID_MAX_HOT_STACK_FRAME_BYTES}"
                "-DVIVID_STACK_USAGE_PROFILE_FINGERPRINT=${_vivid_profile_fingerprint}"
                "-DVIVID_STACK_USAGE_TARGET_FINGERPRINT=${_vivid_target_fingerprint}"
                "-DVIVID_STACK_USAGE_ENFORCE=ON"
                -P "${_VIVID_CMAKE_DIR}/cmake/stack_usage_gate.cmake"
            VERBATIM)
    endif()

    set(${module_list_var} "${${module_list_var}}" PARENT_SCOPE)
endfunction()
