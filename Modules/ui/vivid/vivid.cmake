set(_VIVID_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

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

function(vivid_widget_feature_define_name out_var widget_module_name)
    string(REPLACE "_" ";" _vivid_widget_parts "${widget_module_name}")
    set(_vivid_widget_define "")
    foreach(_vivid_widget_part IN LISTS _vivid_widget_parts)
        if (_vivid_widget_part STREQUAL "")
            continue()
        endif()
        string(SUBSTRING "${_vivid_widget_part}" 0 1 _vivid_widget_first)
        string(SUBSTRING "${_vivid_widget_part}" 1 -1 _vivid_widget_rest)
        string(TOUPPER "${_vivid_widget_first}" _vivid_widget_first)
        string(APPEND _vivid_widget_define "${_vivid_widget_first}${_vivid_widget_rest}")
    endforeach()
    if (_vivid_widget_define STREQUAL "Scrollbar")
        set(_vivid_widget_define "ScrollBar")
    elseif (_vivid_widget_define STREQUAL "Switcher")
        set(_vivid_widget_define "Switch")
    endif()
    set(${out_var} "${_vivid_widget_define}" PARENT_SCOPE)
endfunction()

function(vivid_widget_module_file_name out_var widget_module_name)
    if (widget_module_name STREQUAL "switcher")
        set(_vivid_widget_file "switch")
    else()
        set(_vivid_widget_file "${widget_module_name}")
    endif()
    set(${out_var} "${_vivid_widget_file}" PARENT_SCOPE)
endfunction()

function(vivid_collect_product_widget_modules out_var)
    if (NOT CHARM_VIVID_PRODUCT_WIDGETS)
        message(FATAL_ERROR
            "CHARM_VIVID_FEATURESET=PRODUCT requires CHARM_VIVID_PRODUCT_WIDGETS")
    endif()

    set(_vivid_product_modules "")
    foreach(_vivid_product_widget IN LISTS CHARM_VIVID_PRODUCT_WIDGETS)
        if (_vivid_product_widget STREQUAL "")
            message(FATAL_ERROR "Empty CHARM_VIVID_PRODUCT_WIDGETS entry")
        endif()
        vivid_widget_module_file_name(_vivid_product_widget_file "${_vivid_product_widget}")
        set(_vivid_product_widget_module
            "${_VIVID_CMAKE_DIR}/widgets/${_vivid_product_widget_file}.cppm")
        if (NOT EXISTS "${_vivid_product_widget_module}")
            message(FATAL_ERROR
                "Unknown Vivid PRODUCT widget '${_vivid_product_widget}': "
                "${_vivid_product_widget_module} does not exist")
        endif()
        list(APPEND _vivid_product_modules "${_vivid_product_widget_module}")
    endforeach()
    list(REMOVE_DUPLICATES _vivid_product_modules)
    set(${out_var} "${_vivid_product_modules}" PARENT_SCOPE)
endfunction()

function(vivid_collect_product_area_modules out_var area list_var)
    if (NOT area STREQUAL "core" AND NOT area STREQUAL "gfx")
        message(FATAL_ERROR "Unknown Vivid PRODUCT module area '${area}'")
    endif()
    if (NOT DEFINED ${list_var} OR NOT ${list_var})
        message(FATAL_ERROR
            "CHARM_VIVID_FEATURESET=PRODUCT requires ${list_var}")
    endif()

    set(_vivid_product_modules "")
    foreach(_vivid_product_module IN LISTS ${list_var})
        if (_vivid_product_module STREQUAL "")
            message(FATAL_ERROR "Empty ${list_var} entry")
        endif()
        if (_vivid_product_module MATCHES "[/\\\\]" OR _vivid_product_module MATCHES "\\.cppm$")
            message(FATAL_ERROR
                "${list_var} entries must be module basenames without path or .cppm: "
                "'${_vivid_product_module}'")
        endif()
        set(_vivid_product_module_path
            "${_VIVID_CMAKE_DIR}/${area}/${_vivid_product_module}.cppm")
        if (NOT EXISTS "${_vivid_product_module_path}")
            message(FATAL_ERROR
                "Unknown Vivid PRODUCT ${area} module '${_vivid_product_module}': "
                "${_vivid_product_module_path} does not exist")
        endif()
        if (_vivid_product_module STREQUAL "snapshot"
            OR _vivid_product_module STREQUAL "display_policy"
            OR _vivid_product_module STREQUAL "host_tools")
            message(FATAL_ERROR
                "Host-only Vivid ${area} module '${_vivid_product_module}' is not allowed "
                "in CHARM_VIVID_FEATURESET=PRODUCT")
        endif()
        list(APPEND _vivid_product_modules "${_vivid_product_module_path}")
    endforeach()
    list(REMOVE_DUPLICATES _vivid_product_modules)
    set(${out_var} "${_vivid_product_modules}" PARENT_SCOPE)
endfunction()

function(vivid_collect_product_core_modules out_var)
    vivid_collect_product_area_modules(
        _vivid_product_modules core CHARM_VIVID_PRODUCT_CORE_MODULES)
    set(${out_var} "${_vivid_product_modules}" PARENT_SCOPE)
endfunction()

function(vivid_collect_product_gfx_modules out_var)
    vivid_collect_product_area_modules(
        _vivid_product_modules gfx CHARM_VIVID_PRODUCT_GFX_MODULES)
    set(${out_var} "${_vivid_product_modules}" PARENT_SCOPE)
endfunction()

function(vivid_product_widget_defines out_var)
    set(_vivid_product_defines "")
    foreach(_vivid_product_widget IN LISTS CHARM_VIVID_PRODUCT_WIDGETS)
        vivid_widget_feature_define_name(_vivid_product_define "${_vivid_product_widget}")
        if (_vivid_product_define STREQUAL "")
            message(FATAL_ERROR "Empty CHARM_VIVID_PRODUCT_WIDGETS entry")
        endif()
        string(APPEND _vivid_product_defines
            "#define CHARM_VIVID_ENABLE_WIDGET_${_vivid_product_define} 1\n")
    endforeach()
    set(${out_var} "${_vivid_product_defines}" PARENT_SCOPE)
endfunction()

function(vivid_product_widget_enabled_by_define out_var widget_define)
    set(_vivid_product_widget_enabled OFF)
    foreach(_vivid_product_widget IN LISTS CHARM_VIVID_PRODUCT_WIDGETS)
        vivid_widget_feature_define_name(_vivid_product_define "${_vivid_product_widget}")
        if (_vivid_product_define STREQUAL "${widget_define}")
            set(_vivid_product_widget_enabled ON)
            break()
        endif()
    endforeach()
    set(${out_var} "${_vivid_product_widget_enabled}" PARENT_SCOPE)
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

function(vivid_apply_product_payload_cap cap_var knob_suffix)
    set(_vivid_payload_knob "CHARM_VIVID_PAYLOAD_CAP_${knob_suffix}")
    set(_vivid_payload_widget_enabled OFF)
    set(_vivid_payload_widget_names "")
    foreach(_vivid_payload_widget_define IN LISTS ARGN)
        list(APPEND _vivid_payload_widget_names "${_vivid_payload_widget_define}")
        vivid_product_widget_enabled_by_define(
            _vivid_payload_widget_define_enabled
            "${_vivid_payload_widget_define}")
        if (_vivid_payload_widget_define_enabled)
            set(_vivid_payload_widget_enabled ON)
        endif()
    endforeach()
    string(REPLACE ";" "/" _vivid_payload_widget_text "${_vivid_payload_widget_names}")
    if (_vivid_payload_widget_enabled)
        if (NOT DEFINED ${_vivid_payload_knob} OR "${${_vivid_payload_knob}}" STREQUAL "")
            message(FATAL_ERROR
                "CHARM_VIVID_FEATURESET=PRODUCT requires explicit ${_vivid_payload_knob} "
                "because widget ${_vivid_payload_widget_text} is enabled")
        endif()
        vivid_require_uint16_capacity("${_vivid_payload_knob}" "${${_vivid_payload_knob}}")
        set(${cap_var} "${${_vivid_payload_knob}}" PARENT_SCOPE)
    else()
        if (DEFINED ${_vivid_payload_knob} AND NOT "${${_vivid_payload_knob}}" STREQUAL "")
            vivid_require_uint16_capacity("${_vivid_payload_knob}" "${${_vivid_payload_knob}}")
            if (NOT "${${_vivid_payload_knob}}" STREQUAL "0")
                message(FATAL_ERROR
                    "${_vivid_payload_knob}=${${_vivid_payload_knob}} is not allowed because "
                    "widget ${_vivid_payload_widget_text} is not enabled in CHARM_VIVID_PRODUCT_WIDGETS")
            endif()
        endif()
        set(${cap_var} 0 PARENT_SCOPE)
    endif()
endfunction()

function(vivid_collect_modules target_name module_list_var base_dirs_var)
    if (NOT CHARM_ENABLE_UI_VIVID)
        return()
    endif()

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
    vivid_cache_default(CHARM_VIVID_STYLE_CLASS_MAX STRING 256 "Vivid style class capacity")
    vivid_cache_default(CHARM_VIVID_STYLE_RULE_CAP STRING 32 "Vivid stylesheet rule capacity")
    vivid_cache_default(CHARM_VIVID_STYLE_METRICS_POOL_CAP STRING 64 "Vivid stylesheet metrics pool capacity")
    vivid_cache_default(CHARM_VIVID_DRAW_CMD_MAX_COMMANDS STRING 1024 "Vivid DrawCmd command capacity")
    vivid_cache_default(CHARM_VIVID_DRAW_CMD_TEXT_BYTES STRING 4096 "Vivid DrawCmd text arena bytes")
    vivid_cache_default(CHARM_VIVID_DRAW_CMD_BLOB_BYTES STRING 2048 "Vivid DrawCmd blob arena bytes")
    vivid_cache_default(CHARM_VIVID_MAX_HOT_STACK_FRAME_BYTES STRING 4096 "Vivid selected-module stack frame limit")
    vivid_cache_default(CHARM_VIVID_DRAW_DETAIL_EVIDENCE BOOL OFF "Enable DrawCmd detail evidence")
    vivid_cache_default(CHARM_VIVID_RUNTIME_SCENE_INSTANCES STRING "" "Vivid resident Scene instance count; required by PRODUCT and MCU_MIN")
    vivid_cache_default(CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES STRING "" "Vivid resident RAM budget; required by PRODUCT and MCU_MIN")
    vivid_cache_default(CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES STRING "" "Minimum Vivid resident RAM headroom; required by PRODUCT and MCU_MIN")
    foreach(_vivid_payload_cap IN ITEMS
            LABEL BUTTON IMAGE TEXT_INPUT TEXT_AREA NUMBER_INPUT
            SEGMENTED_CONTROL STEPPER TOGGLE_GROUP CHECKBOX RADIO
            LIST_ITEM TEXT_LIST LIST_VIEW TABLE_VIEW TREE_VIEW NUMBER_LIST
            ROLLER SWITCH SLIDER PROGRESS SCROLLBAR LIST SCROLL_CONTAINER SPINNER)
        vivid_cache_default(CHARM_VIVID_PAYLOAD_CAP_${_vivid_payload_cap}
            STRING "" "Vivid payload pool capacity override for ${_vivid_payload_cap}")
    endforeach()

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
    vivid_require_uint16_capacity("CHARM_VIVID_STYLE_CLASS_MAX" "${CHARM_VIVID_STYLE_CLASS_MAX}")
    vivid_require_uint16_capacity("CHARM_VIVID_STYLE_RULE_CAP" "${CHARM_VIVID_STYLE_RULE_CAP}")
    vivid_require_uint16_capacity("CHARM_VIVID_STYLE_METRICS_POOL_CAP" "${CHARM_VIVID_STYLE_METRICS_POOL_CAP}")
    if ("${CHARM_VIVID_STYLE_CLASS_MAX}" LESS 1)
        message(FATAL_ERROR "CHARM_VIVID_STYLE_CLASS_MAX must be > 0")
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
        if (NOT CHARM_VIVID_PRODUCT_WIDGETS)
            message(FATAL_ERROR
                "CHARM_VIVID_FEATURESET=PRODUCT requires CHARM_VIVID_PRODUCT_WIDGETS")
        endif()
        vivid_apply_product_payload_cap(SOA_POOL_CAP_LABEL LABEL Label TextBox)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_BUTTON BUTTON Button IconButton)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_IMAGE IMAGE Image ImageBox)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_TEXT_INPUT TEXT_INPUT TextInput)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_TEXT_AREA TEXT_AREA TextArea)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_NUMBER_INPUT NUMBER_INPUT NumberInput)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_SEGMENTED_CONTROL SEGMENTED_CONTROL SegmentedControl TabView)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_STEPPER STEPPER Stepper)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_TOGGLE_GROUP TOGGLE_GROUP ToggleGroup)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_CHECKBOX CHECKBOX Checkbox)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_RADIO RADIO Radio)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_LIST_ITEM LIST_ITEM ListItem MenuItem)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_TEXT_LIST TEXT_LIST TextList ConsoleBox)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_LIST_VIEW LIST_VIEW ListView IconList)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_TABLE_VIEW TABLE_VIEW TableView)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_TREE_VIEW TREE_VIEW TreeView)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_NUMBER_LIST NUMBER_LIST NumberList)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_ROLLER ROLLER Roller)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_SWITCH SWITCH Switch)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_SLIDER SLIDER Slider)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_PROGRESS PROGRESS Progress ProgressWheel ProgressBarSimple ProgressBarRound ProgressBarDrill ProgressFlowing)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_SCROLLBAR SCROLLBAR ScrollBar)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_LIST LIST List)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_SCROLL_CONTAINER SCROLL_CONTAINER ScrollContainer)
        vivid_apply_product_payload_cap(SOA_POOL_CAP_SPINNER SPINNER Spinner BusyWheel)
        vivid_product_widget_defines(VIVID_PRODUCT_WIDGET_DEFINES)
    else()
        set(VIVID_PRODUCT_WIDGET_DEFINES "")
    endif()

    math(EXPR _vivid_payload_slot_cap_total
        "${SOA_POOL_CAP_LABEL} + ${SOA_POOL_CAP_BUTTON} + ${SOA_POOL_CAP_IMAGE} + ${SOA_POOL_CAP_TEXT_INPUT} + ${SOA_POOL_CAP_TEXT_AREA} + ${SOA_POOL_CAP_NUMBER_INPUT} + ${SOA_POOL_CAP_SEGMENTED_CONTROL} + ${SOA_POOL_CAP_STEPPER} + ${SOA_POOL_CAP_TOGGLE_GROUP} + ${SOA_POOL_CAP_CHECKBOX} + ${SOA_POOL_CAP_RADIO} + ${SOA_POOL_CAP_LIST_ITEM} + ${SOA_POOL_CAP_TEXT_LIST} + ${SOA_POOL_CAP_LIST_VIEW} + ${SOA_POOL_CAP_TABLE_VIEW} + ${SOA_POOL_CAP_TREE_VIEW} + ${SOA_POOL_CAP_NUMBER_LIST} + ${SOA_POOL_CAP_ROLLER} + ${SOA_POOL_CAP_SWITCH} + ${SOA_POOL_CAP_SLIDER} + ${SOA_POOL_CAP_PROGRESS} + ${SOA_POOL_CAP_SCROLLBAR} + ${SOA_POOL_CAP_LIST} + ${SOA_POOL_CAP_SCROLL_CONTAINER} + ${SOA_POOL_CAP_SPINNER}")

    # Conservative configure-time model. scene.cppm validates this upper bound
    # against target-ABI sizeof values, so configuration drift cannot undercount.
    math(EXPR _vivid_draw_cmd_buffer_upper_bytes
        "${VIVID_DRAW_CMD_MAX_COMMANDS} * 128 + ${VIVID_DRAW_CMD_TEXT_BYTES} + ${VIVID_DRAW_CMD_BLOB_BYTES} + 4096")
    math(EXPR _vivid_draw_cmd_compaction_workspace_upper_bytes
        "${VIVID_DRAW_CMD_MAX_COMMANDS} * 4 + 32768")
    set(_vivid_draw_cmd_executor_workspace_upper_bytes 32768)
    math(EXPR _vivid_soa_traversal_workspace_upper_bytes
        "${CHARM_VIVID_SOA_MAX_NODES} * 256")
    set(_vivid_soa_node_upper_bytes 512)
    set(_vivid_payload_slot_upper_bytes 512)
    set(_vivid_soa_fixed_upper_bytes 65536)
    set(_vivid_scene_fixed_upper_bytes 65536)
    set(_vivid_global_fixed_upper_bytes 262144)
    math(EXPR _vivid_pixel_snapshot_upper_bytes
        "${CHARM_VIVID_LAYER_CACHE_SLOTS} * ${CHARM_VIVID_LAYER_CACHE_WIDTH} * ${CHARM_VIVID_LAYER_CACHE_HEIGHT} * ${_vivid_screen_bytes_per_pixel}")
    math(EXPR _vivid_command_buffer_upper_bytes
        "(${CHARM_VIVID_LAYER_CACHE_SLOTS} + 1) * ${_vivid_draw_cmd_buffer_upper_bytes}")
    math(EXPR _vivid_soa_upper_bytes
        "${CHARM_VIVID_SOA_MAX_NODES} * ${_vivid_soa_node_upper_bytes} + ${_vivid_payload_slot_cap_total} * ${_vivid_payload_slot_upper_bytes} + ${_vivid_text_arena_bytes} + ${_vivid_soa_fixed_upper_bytes}")
    math(EXPR _vivid_scene_upper_bytes
        "${_vivid_pixel_snapshot_upper_bytes} + ${_vivid_command_buffer_upper_bytes} + ${_vivid_draw_cmd_compaction_workspace_upper_bytes} + ${_vivid_draw_cmd_executor_workspace_upper_bytes} + ${_vivid_soa_traversal_workspace_upper_bytes} + ${_vivid_soa_upper_bytes} + ${_vivid_scene_fixed_upper_bytes}")
    math(EXPR _vivid_global_upper_bytes
        "${_vivid_global_fixed_upper_bytes} + ${CHARM_VIVID_STYLE_CLASS_MAX} * 256 + ${CHARM_VIVID_STYLE_RULE_CAP} * 256 + ${CHARM_VIVID_STYLE_METRICS_POOL_CAP} * 64")
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

    set(_vivid_static_memory_manifest
        "featureset=${CHARM_VIVID_FEATURESET}\n"
        "admission_required=${VIVID_STATIC_MEMORY_ADMISSION_REQUIRED}\n"
        "status=${_vivid_admission_status}\n"
        "scene_instances=${VIVID_RUNTIME_SCENE_INSTANCES}\n"
        "pixel_snapshot_upper_bytes=${_vivid_pixel_snapshot_upper_bytes}\n"
        "command_buffer_upper_bytes=${_vivid_command_buffer_upper_bytes}\n"
        "draw_cmd_max_commands=${VIVID_DRAW_CMD_MAX_COMMANDS}\n"
        "draw_cmd_text_bytes=${VIVID_DRAW_CMD_TEXT_BYTES}\n"
        "draw_cmd_blob_bytes=${VIVID_DRAW_CMD_BLOB_BYTES}\n"
        "max_hot_stack_frame_bytes=${VIVID_MAX_HOT_STACK_FRAME_BYTES}\n"
        "draw_cmd_compaction_workspace_upper_bytes=${_vivid_draw_cmd_compaction_workspace_upper_bytes}\n"
        "draw_cmd_executor_workspace_upper_bytes=${_vivid_draw_cmd_executor_workspace_upper_bytes}\n"
        "soa_traversal_workspace_upper_bytes=${_vivid_soa_traversal_workspace_upper_bytes}\n"
        "soa_upper_bytes=${_vivid_soa_upper_bytes}\n"
        "scene_upper_bytes=${_vivid_scene_upper_bytes}\n"
        "global_upper_bytes=${_vivid_global_upper_bytes}\n"
        "upper_bound_bytes=${VIVID_STATIC_MEMORY_UPPER_BOUND_BYTES}\n"
        "budget_bytes=${VIVID_STATIC_MEMORY_BUDGET_BYTES}\n"
        "min_headroom_bytes=${VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES}\n"
        "configured_headroom_bytes=${_vivid_configured_headroom_bytes}\n")
    list(JOIN _vivid_static_memory_manifest "" _vivid_static_memory_manifest_text)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/vivid")
    file(WRITE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/vivid/static_memory_admission.txt"
        "${_vivid_static_memory_manifest_text}")
    message(STATUS
        "Vivid static memory: featureset=${CHARM_VIVID_FEATURESET} "
        "upper_bound=${VIVID_STATIC_MEMORY_UPPER_BOUND_BYTES} "
        "budget=${VIVID_STATIC_MEMORY_BUDGET_BYTES} "
        "headroom=${_vivid_configured_headroom_bytes} "
        "status=${_vivid_admission_status}")

    set(vivid_pool_caps_output_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/vivid")
    file(MAKE_DIRECTORY "${vivid_pool_caps_output_dir}")
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
        set(_vivid_stack_usage_root
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target_name}.dir/Modules/ui/vivid")
        set(_vivid_stack_usage_manifest
            "${CMAKE_CURRENT_BINARY_DIR}/generated/vivid/stack_usage_manifest.txt")
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
        if (CHARM_VIVID_PRODUCT_CORE_MODULES)
            vivid_collect_product_core_modules(_vivid_product_core_modules)
            list(FILTER ${module_list_var} EXCLUDE REGEX "/Modules/ui/vivid/core/")
            list(APPEND ${module_list_var} ${_vivid_product_core_modules})
        endif()
        if (CHARM_VIVID_PRODUCT_GFX_MODULES)
            vivid_collect_product_gfx_modules(_vivid_product_gfx_modules)
            list(FILTER ${module_list_var} EXCLUDE REGEX "/Modules/ui/vivid/gfx/")
            list(APPEND ${module_list_var} ${_vivid_product_gfx_modules})
        endif()
        vivid_collect_product_widget_modules(_vivid_product_widget_modules)
        list(FILTER ${module_list_var} EXCLUDE REGEX "/Modules/ui/vivid/widgets/")
        list(FILTER ${module_list_var} EXCLUDE REGEX
            "/Modules/ui/vivid/gfx/(snapshot|host_tools)\\.cppm$")
        list(APPEND ${module_list_var} ${_vivid_product_widget_modules})
    endif()
    if (CHARM_VIVID_FEATURESET STREQUAL "MCU_MIN")
        list(FILTER ${module_list_var} EXCLUDE REGEX "/Modules/ui/vivid/widgets/")
    endif()
    if (CHARM_VIVID_FEATURESET STREQUAL "MCU_MIN")
        list(REMOVE_ITEM ${module_list_var}
            "${PROJECT_SOURCE_DIR}/Modules/ui/vivid/core/object.cppm"
            "${PROJECT_SOURCE_DIR}/Modules/ui/vivid/core/input_interaction.cppm"
            "${PROJECT_SOURCE_DIR}/Modules/ui/vivid/core/string.cppm"
            "${PROJECT_SOURCE_DIR}/Modules/ui/vivid/core/virtual_list.cppm"
        )
    endif()

    if(VIVID_STATIC_MEMORY_ADMISSION_REQUIRED)
        get_filename_component(
            _vivid_repo_root "${_VIVID_CMAKE_DIR}/../../.." ABSOLUTE)
        set(_vivid_stack_usage_source_manifest
            "${CMAKE_CURRENT_BINARY_DIR}/generated/vivid/stack_usage_sources.txt")
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
                "-DVIVID_STACK_USAGE_ENFORCE=ON"
                -P "${_VIVID_CMAKE_DIR}/cmake/stack_usage_gate.cmake"
            VERBATIM)
    endif()

    set(${module_list_var} "${${module_list_var}}" PARENT_SCOPE)
endfunction()
