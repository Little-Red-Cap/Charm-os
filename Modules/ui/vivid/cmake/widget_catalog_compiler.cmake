include_guard(GLOBAL)

set(_VIVID_WIDGET_CATALOG_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(_vivid_widget_key out_var value)
    string(SHA256 _key "${value}")
    set(${out_var} "${_key}" PARENT_SCOPE)
endfunction()

function(_vivid_widget_set kind field value)
    _vivid_widget_key(_kind_key "${kind}")
    set_property(GLOBAL PROPERTY "VIVID_WIDGET_${_kind_key}_${field}" "${value}")
endfunction()

function(_vivid_widget_get kind field out_var)
    _vivid_widget_key(_kind_key "${kind}")
    get_property(_value GLOBAL PROPERTY "VIVID_WIDGET_${_kind_key}_${field}")
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

function(vivid_catalog_payload_pool)
    set(_one_value NAME MEMBER STATS_FIELD CAP_KIND)
    cmake_parse_arguments(PARSE_ARGV 0 POOL "" "${_one_value}" "")
    if(POOL_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "vivid_catalog_payload_pool: unknown arguments: ${POOL_UNPARSED_ARGUMENTS}")
    endif()
    foreach(_field IN LISTS _one_value)
        if(NOT POOL_${_field})
            message(FATAL_ERROR "vivid_catalog_payload_pool requires ${_field}")
        endif()
    endforeach()
    if(NOT POOL_NAME MATCHES "^[A-Za-z][A-Za-z0-9_]*$")
        message(FATAL_ERROR "Invalid Vivid payload pool name '${POOL_NAME}'")
    endif()
    get_property(_pools GLOBAL PROPERTY VIVID_PAYLOAD_POOLS)
    if(POOL_NAME IN_LIST _pools)
        message(FATAL_ERROR "Duplicate Vivid payload pool '${POOL_NAME}'")
    endif()
    list(APPEND _pools "${POOL_NAME}")
    set_property(GLOBAL PROPERTY VIVID_PAYLOAD_POOLS "${_pools}")
    _vivid_widget_key(_pool_key "payload:${POOL_NAME}")
    foreach(_field IN LISTS _one_value)
        set_property(GLOBAL PROPERTY
            "VIVID_PAYLOAD_${_pool_key}_${_field}" "${POOL_${_field}}")
    endforeach()
endfunction()

function(_vivid_payload_get pool field out_var)
    _vivid_widget_key(_pool_key "payload:${pool}")
    get_property(_value GLOBAL PROPERTY "VIVID_PAYLOAD_${_pool_key}_${field}")
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

function(vivid_catalog_widget)
    set(_options
        RUNTIME_ONLY HIT_TEST_FALSE FOCUSABLE CLIP_CHILDREN LAYOUT_LIST
        CLICK_ENABLED CHECKABLE SCROLL_ENABLED DRAG_ENABLED WHEEL_ENABLED
        EXTRA_ENABLED CAPTURE_ENABLED)
    set(_one_value
        ID KIND SCENE_SUPPORT MODULE CPP_TYPE THEME_BASE FACTORY FACTORY_POOL FACTORY_CREATE
        PAYLOAD_POOL STYLE CLICK CLICK_INDEX GROUP_KIND WHEEL_TARGET
        DRAG_BEHAVIOR DRAG_BEHAVIOR_ONLY WHEEL_TARGET_ONLY SCROLL_AXIS WHEEL_AXIS)
    cmake_parse_arguments(PARSE_ARGV 0 WIDGET
        "${_options}" "${_one_value}" "")
    if(WIDGET_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "vivid_catalog_widget(${WIDGET_KIND}): unknown arguments: ${WIDGET_UNPARSED_ARGUMENTS}")
    endif()
    foreach(_field IN ITEMS
            ID KIND SCENE_SUPPORT CPP_TYPE THEME_BASE FACTORY FACTORY_POOL FACTORY_CREATE
            PAYLOAD_POOL STYLE CLICK CLICK_INDEX GROUP_KIND WHEEL_TARGET
            DRAG_BEHAVIOR DRAG_BEHAVIOR_ONLY WHEEL_TARGET_ONLY SCROLL_AXIS WHEEL_AXIS)
        if(NOT DEFINED WIDGET_${_field} OR "${WIDGET_${_field}}" STREQUAL "")
            message(FATAL_ERROR "vivid_catalog_widget requires ${_field}")
        endif()
    endforeach()
    if(NOT WIDGET_RUNTIME_ONLY AND NOT WIDGET_MODULE MATCHES "^charm\\.")
        message(FATAL_ERROR
            "Vivid widget '${WIDGET_KIND}' requires MODULE or RUNTIME_ONLY")
    endif()
    if(WIDGET_RUNTIME_ONLY AND WIDGET_MODULE)
        message(FATAL_ERROR
            "Vivid widget '${WIDGET_KIND}' cannot use MODULE and RUNTIME_ONLY together")
    endif()
    if(NOT WIDGET_ID MATCHES "^[0-9]+$" OR WIDGET_ID LESS 1 OR WIDGET_ID GREATER 255)
        message(FATAL_ERROR
            "Vivid widget '${WIDGET_KIND}' ID must be in [1, 255]")
    endif()
    if(NOT WIDGET_KIND MATCHES "^[A-Za-z][A-Za-z0-9_]*$")
        message(FATAL_ERROR "Invalid Vivid WidgetKind '${WIDGET_KIND}'")
    endif()
    if(NOT WIDGET_SCENE_SUPPORT STREQUAL "Supported" AND
       NOT WIDGET_SCENE_SUPPORT STREQUAL "Unsupported")
        message(FATAL_ERROR
            "Vivid widget '${WIDGET_KIND}' SCENE_SUPPORT must be Supported or Unsupported")
    endif()

    get_property(_kinds GLOBAL PROPERTY VIVID_WIDGET_KINDS)
    get_property(_ids GLOBAL PROPERTY VIVID_WIDGET_IDS)
    if(WIDGET_KIND IN_LIST _kinds)
        message(FATAL_ERROR "Duplicate Vivid WidgetKind '${WIDGET_KIND}'")
    endif()
    if(WIDGET_ID IN_LIST _ids)
        message(FATAL_ERROR "Duplicate Vivid WidgetKind ID '${WIDGET_ID}'")
    endif()
    list(APPEND _kinds "${WIDGET_KIND}")
    list(APPEND _ids "${WIDGET_ID}")
    set_property(GLOBAL PROPERTY VIVID_WIDGET_KINDS "${_kinds}")
    set_property(GLOBAL PROPERTY VIVID_WIDGET_IDS "${_ids}")

    foreach(_field IN LISTS _one_value)
        _vivid_widget_set("${WIDGET_KIND}" "${_field}" "${WIDGET_${_field}}")
    endforeach()
    foreach(_field IN LISTS _options)
        if(WIDGET_${_field})
            _vivid_widget_set("${WIDGET_KIND}" "${_field}" 1)
        else()
            _vivid_widget_set("${WIDGET_KIND}" "${_field}" 0)
        endif()
    endforeach()
endfunction()

function(vivid_load_widget_catalog)
    get_property(_loaded GLOBAL PROPERTY VIVID_WIDGET_CATALOG_LOADED)
    if(_loaded)
        return()
    endif()
    set_property(GLOBAL PROPERTY VIVID_WIDGET_KINDS "")
    set_property(GLOBAL PROPERTY VIVID_WIDGET_IDS "")
    set_property(GLOBAL PROPERTY VIVID_PAYLOAD_POOLS "")
    include("${_VIVID_WIDGET_CATALOG_DIR}/widget_catalog.cmake")

    get_property(_kinds GLOBAL PROPERTY VIVID_WIDGET_KINDS)
    get_property(_pools GLOBAL PROPERTY VIVID_PAYLOAD_POOLS)
    list(LENGTH _kinds _kind_count)
    if(NOT _kind_count EQUAL 72)
        message(FATAL_ERROR
            "Vivid widget catalog must contain 72 kinds, found ${_kind_count}")
    endif()
    foreach(_kind IN LISTS _kinds)
        _vivid_widget_get("${_kind}" PAYLOAD_POOL _payload_pool)
        if(NOT _payload_pool STREQUAL "None" AND NOT _payload_pool IN_LIST _pools)
            message(FATAL_ERROR
                "Vivid widget '${_kind}' references unknown payload pool '${_payload_pool}'")
        endif()
    endforeach()

    file(STRINGS
        "${_VIVID_WIDGET_CATALOG_DIR}/widget_kind_abi.expected"
        _expected_abi ENCODING UTF-8)
    list(LENGTH _expected_abi _expected_count)
    if(NOT _expected_count EQUAL _kind_count)
        message(FATAL_ERROR
            "Vivid WidgetKind ABI manifest has ${_expected_count} entries; catalog has ${_kind_count}")
    endif()
    foreach(_entry IN LISTS _expected_abi)
        if(NOT _entry MATCHES "^([0-9]+)=([A-Za-z][A-Za-z0-9_]*)$")
            message(FATAL_ERROR "Invalid Vivid WidgetKind ABI entry '${_entry}'")
        endif()
        set(_expected_id "${CMAKE_MATCH_1}")
        set(_expected_kind "${CMAKE_MATCH_2}")
        if(NOT _expected_kind IN_LIST _kinds)
            message(FATAL_ERROR
                "Vivid WidgetKind ABI entry '${_entry}' is missing from catalog")
        endif()
        _vivid_widget_get("${_expected_kind}" ID _actual_id)
        if(NOT _actual_id EQUAL _expected_id)
            message(FATAL_ERROR
                "Vivid WidgetKind ABI drift for ${_expected_kind}: expected ${_expected_id}, got ${_actual_id}")
        endif()
    endforeach()
    set_property(GLOBAL PROPERTY VIVID_WIDGET_CATALOG_LOADED TRUE)
endfunction()

function(vivid_widget_catalog_fingerprint out_var)
    vivid_load_widget_catalog()
    get_property(_kinds GLOBAL PROPERTY VIVID_WIDGET_KINDS)
    get_property(_pools GLOBAL PROPERTY VIVID_PAYLOAD_POOLS)

    set(_canonical "schema=2\n")
    foreach(_pool IN LISTS _pools)
        string(APPEND _canonical "pool=${_pool}")
        foreach(_field IN ITEMS MEMBER STATS_FIELD CAP_KIND)
            _vivid_payload_get("${_pool}" "${_field}" _value)
            string(APPEND _canonical "|${_field}=${_value}")
        endforeach()
        string(APPEND _canonical "\n")
    endforeach()

    foreach(_kind IN LISTS _kinds)
        string(APPEND _canonical "widget=${_kind}")
        foreach(_field IN ITEMS
                ID SCENE_SUPPORT MODULE CPP_TYPE THEME_BASE FACTORY FACTORY_POOL FACTORY_CREATE
                PAYLOAD_POOL STYLE CLICK CLICK_INDEX GROUP_KIND WHEEL_TARGET
                DRAG_BEHAVIOR DRAG_BEHAVIOR_ONLY WHEEL_TARGET_ONLY
                SCROLL_AXIS WHEEL_AXIS RUNTIME_ONLY HIT_TEST_FALSE FOCUSABLE
                CLIP_CHILDREN LAYOUT_LIST CLICK_ENABLED CHECKABLE SCROLL_ENABLED
                DRAG_ENABLED WHEEL_ENABLED EXTRA_ENABLED CAPTURE_ENABLED)
            _vivid_widget_get("${_kind}" "${_field}" _value)
            string(APPEND _canonical "|${_field}=${_value}")
        endforeach()
        string(APPEND _canonical "\n")
    endforeach()

    string(SHA256 _fingerprint "${_canonical}")
    set(${out_var} "${_fingerprint}" PARENT_SCOPE)
endfunction()

function(vivid_widget_profile_resolve out_modules out_pools out_defines)
    set(_options)
    set(_one_value PROFILE)
    set(_multi_value KINDS PAYLOAD_CAPACITIES)
    cmake_parse_arguments(PARSE_ARGV 3 ACTIVE
        "${_options}" "${_one_value}" "${_multi_value}")
    vivid_load_widget_catalog()
    get_property(_catalog_kinds GLOBAL PROPERTY VIVID_WIDGET_KINDS)
    get_property(_catalog_pools GLOBAL PROPERTY VIVID_PAYLOAD_POOLS)
    set(_modules "")
    set(_pools "")
    set(_defines "")
    foreach(_kind IN LISTS ACTIVE_KINDS)
        if(NOT _kind IN_LIST _catalog_kinds)
            message(FATAL_ERROR
                "Vivid profile '${ACTIVE_PROFILE}' selects unknown WidgetKind '${_kind}'")
        endif()
        _vivid_widget_get("${_kind}" SCENE_SUPPORT _scene_support)
        if(NOT _scene_support STREQUAL "Supported")
            message(FATAL_ERROR
                "Vivid profile '${ACTIVE_PROFILE}' selects WidgetKind '${_kind}' "
                "without Scene runtime support")
        endif()
        _vivid_widget_get("${_kind}" MODULE _module)
        _vivid_widget_get("${_kind}" PAYLOAD_POOL _payload_pool)
        if(_module)
            list(APPEND _modules "${_module}")
        endif()
        if(NOT _payload_pool STREQUAL "None")
            list(APPEND _pools "${_payload_pool}")
        endif()
        string(APPEND _defines "#define CHARM_VIVID_ENABLE_WIDGET_${_kind} 1\n")
    endforeach()
    list(REMOVE_DUPLICATES _modules)
    list(REMOVE_DUPLICATES _pools)
    list(SORT _modules)
    list(SORT _pools)

    set(_declared_pools "")
    foreach(_entry IN LISTS ACTIVE_PAYLOAD_CAPACITIES)
        if(NOT _entry MATCHES "^([A-Za-z][A-Za-z0-9_]*)=([0-9]+)$")
            message(FATAL_ERROR
                "Vivid profile '${ACTIVE_PROFILE}' has invalid payload capacity '${_entry}'")
        endif()
        set(_pool "${CMAKE_MATCH_1}")
        set(_capacity "${CMAKE_MATCH_2}")
        if(NOT _pool IN_LIST _catalog_pools)
            message(FATAL_ERROR
                "Vivid profile '${ACTIVE_PROFILE}' sets unknown payload pool '${_pool}'")
        endif()
        if(NOT _pool IN_LIST _pools)
            message(FATAL_ERROR
                "Vivid profile '${ACTIVE_PROFILE}' sets payload pool '${_pool}' with no active consumer")
        endif()
        if(_capacity LESS 1 OR _capacity GREATER 65535)
            message(FATAL_ERROR
                "Vivid profile '${ACTIVE_PROFILE}' payload pool '${_pool}' must be in [1, 65535]")
        endif()
        list(APPEND _declared_pools "${_pool}")
    endforeach()
    foreach(_pool IN LISTS _pools)
        if(NOT _pool IN_LIST _declared_pools)
            message(FATAL_ERROR
                "Vivid profile '${ACTIVE_PROFILE}' must declare payload pool '${_pool}'")
        endif()
    endforeach()
    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_pools} "${_pools}" PARENT_SCOPE)
    set(${out_defines} "${_defines}" PARENT_SCOPE)
endfunction()

function(vivid_payload_capacity_from_profile out_var pool capacities)
    set(_capacity 0)
    foreach(_entry IN LISTS capacities)
        if(_entry MATCHES "^${pool}=([0-9]+)$")
            set(_capacity "${CMAKE_MATCH_1}")
            break()
        endif()
    endforeach()
    set(${out_var} "${_capacity}" PARENT_SCOPE)
endfunction()

function(_vivid_generated_guarded_call out_var kind macro_name arguments)
    string(CONCAT _text
        "#if CHARM_VIVID_ENABLE_WIDGET_${kind}\n"
        "${macro_name}(${kind}${arguments})\n"
        "#endif\n")
    set(${out_var} "${_text}" PARENT_SCOPE)
endfunction()

function(vivid_generate_widget_catalog output_dir)
    vivid_load_widget_catalog()
    file(MAKE_DIRECTORY "${output_dir}")
    get_property(_kinds GLOBAL PROPERTY VIVID_WIDGET_KINDS)
    get_property(_pools GLOBAL PROPERTY VIVID_PAYLOAD_POOLS)

    set(_enum "")
    set(_names "")
    set(_feature_defaults "#pragma once\n")
    set(_feature_table "")
    set(_click "")
    set(_scroll "")
    set(_extra "")
    set(_wheel "")
    set(_drag "")
    set(_capture "")
    set(_hit_test "")
    set(_focusable "")
    set(_clip "")
    set(_layout "")
    set(_style_interactive "")
    set(_style_press "")
    set(_payload_map "")
    set(_max_id 0)

    foreach(_kind IN LISTS _kinds)
        foreach(_field IN ITEMS
                ID PAYLOAD_POOL STYLE CLICK CLICK_INDEX GROUP_KIND WHEEL_TARGET
                DRAG_BEHAVIOR DRAG_BEHAVIOR_ONLY WHEEL_TARGET_ONLY
                SCROLL_AXIS WHEEL_AXIS CLICK_ENABLED CHECKABLE SCROLL_ENABLED
                DRAG_ENABLED WHEEL_ENABLED EXTRA_ENABLED CAPTURE_ENABLED
                HIT_TEST_FALSE FOCUSABLE CLIP_CHILDREN LAYOUT_LIST)
            _vivid_widget_get("${_kind}" "${_field}" _${_field})
        endforeach()
        if(_ID GREATER _max_id)
            set(_max_id "${_ID}")
        endif()
        string(APPEND _enum "    ${_kind} = ${_ID},\n")
        string(APPEND _names "        case WidgetKind::${_kind}: return \"${_kind}\";\n")
        string(APPEND _feature_defaults
            "#ifndef CHARM_VIVID_ENABLE_WIDGET_${_kind}\n"
            "#define CHARM_VIVID_ENABLE_WIDGET_${_kind} VIVID_WIDGET_DEFAULT\n"
            "#endif\n")
        string(APPEND _feature_table
            "        table.enabled[static_cast<std::size_t>(WidgetKind::${_kind})] = "
            "(CHARM_VIVID_ENABLE_WIDGET_${_kind} != 0);\n")
        if(_CLICK_ENABLED)
            _vivid_generated_guarded_call(_line "${_kind}"
                VIVID_WIDGET_BEHAVIOR_CLICK
                ", ${_CLICK}, ${_CLICK_INDEX}, ${_GROUP_KIND}, ${_CHECKABLE}")
            string(APPEND _click "${_line}")
        endif()
        if(_SCROLL_ENABLED)
            _vivid_generated_guarded_call(_line "${_kind}"
                VIVID_WIDGET_BEHAVIOR_SCROLL
                ", ${_WHEEL_TARGET}, ${_DRAG_BEHAVIOR}")
            string(APPEND _scroll "${_line}")
        endif()
        if(_EXTRA_ENABLED)
            _vivid_generated_guarded_call(_line "${_kind}"
                VIVID_WIDGET_BEHAVIOR_EXTRA ", ${_SCROLL_AXIS}, ${_WHEEL_AXIS}")
            string(APPEND _extra "${_line}")
        endif()
        if(_WHEEL_ENABLED)
            _vivid_generated_guarded_call(_line "${_kind}"
                VIVID_WIDGET_BEHAVIOR_WHEEL ", ${_WHEEL_TARGET_ONLY}")
            string(APPEND _wheel "${_line}")
        endif()
        if(_DRAG_ENABLED)
            _vivid_generated_guarded_call(_line "${_kind}"
                VIVID_WIDGET_BEHAVIOR_DRAG ", ${_DRAG_BEHAVIOR_ONLY}")
            string(APPEND _drag "${_line}")
        endif()
        if(_CAPTURE_ENABLED)
            _vivid_generated_guarded_call(_line "${_kind}"
                VIVID_WIDGET_BEHAVIOR_CAPTURE "")
            string(APPEND _capture "${_line}")
        endif()
        foreach(_flag IN ITEMS HIT_TEST_FALSE FOCUSABLE CLIP_CHILDREN LAYOUT_LIST)
            if(_${_flag})
                if(_flag STREQUAL "HIT_TEST_FALSE")
                    set(_macro VIVID_WIDGET_DEFAULT_HIT_TEST_FALSE)
                    set(_target _hit_test)
                elseif(_flag STREQUAL "FOCUSABLE")
                    set(_macro VIVID_WIDGET_DEFAULT_FOCUSABLE)
                    set(_target _focusable)
                elseif(_flag STREQUAL "CLIP_CHILDREN")
                    set(_macro VIVID_WIDGET_DEFAULT_CLIP_CHILDREN)
                    set(_target _clip)
                else()
                    set(_macro VIVID_WIDGET_DEFAULT_LAYOUT_LIST)
                    set(_target _layout)
                endif()
                _vivid_generated_guarded_call(_line "${_kind}" "${_macro}" "")
                string(APPEND ${_target} "${_line}")
            endif()
        endforeach()
        if(_STYLE STREQUAL "Interactive")
            _vivid_generated_guarded_call(_line "${_kind}"
                VIVID_WIDGET_STYLE_INTERACTIVE "")
            string(APPEND _style_interactive "${_line}")
        elseif(_STYLE STREQUAL "PressOnly")
            _vivid_generated_guarded_call(_line "${_kind}"
                VIVID_WIDGET_STYLE_PRESS_ONLY "")
            string(APPEND _style_press "${_line}")
        elseif(NOT _STYLE STREQUAL "Readonly")
            message(FATAL_ERROR
                "Vivid widget '${_kind}' has unknown STYLE '${_STYLE}'")
        endif()
        if(NOT _PAYLOAD_POOL STREQUAL "None")
            string(APPEND _payload_map
                "VIVID_WIDGET_PAYLOAD(${_kind}, ${_PAYLOAD_POOL})\n")
        endif()
    endforeach()

    set(_payload_kinds "")
    foreach(_pool IN LISTS _pools)
        _vivid_payload_get("${_pool}" MEMBER _member)
        _vivid_payload_get("${_pool}" STATS_FIELD _stats)
        _vivid_payload_get("${_pool}" CAP_KIND _cap_kind)
        string(APPEND _payload_kinds
            "VIVID_PAYLOAD_KIND(${_pool}, ${_member}, ${_stats}, ${_cap_kind})\n")
    endforeach()

    file(WRITE "${output_dir}/widget_kind_enum.generated.inc" "${_enum}")
    file(WRITE "${output_dir}/widget_kind_name.generated.inc" "${_names}")
    file(WRITE "${output_dir}/widget_feature_defaults.generated.hpp" "${_feature_defaults}")
    file(WRITE "${output_dir}/widget_feature_table.generated.inc" "${_feature_table}")
    file(WRITE "${output_dir}/widget_behavior_click.generated.inc" "${_click}")
    file(WRITE "${output_dir}/widget_behavior_scroll.generated.inc" "${_scroll}")
    file(WRITE "${output_dir}/widget_behavior_extra.generated.inc" "${_extra}")
    file(WRITE "${output_dir}/widget_behavior_wheel.generated.inc" "${_wheel}")
    file(WRITE "${output_dir}/widget_behavior_drag.generated.inc" "${_drag}")
    file(WRITE "${output_dir}/widget_behavior_capture.generated.inc" "${_capture}")
    file(WRITE "${output_dir}/widget_default_hit_test_false.generated.inc" "${_hit_test}")
    file(WRITE "${output_dir}/widget_default_focusable.generated.inc" "${_focusable}")
    file(WRITE "${output_dir}/widget_default_clip_children.generated.inc" "${_clip}")
    file(WRITE "${output_dir}/widget_default_layout_list.generated.inc" "${_layout}")
    file(WRITE "${output_dir}/widget_style_interactive.generated.inc" "${_style_interactive}")
    file(WRITE "${output_dir}/widget_style_press_only.generated.inc" "${_style_press}")
    file(WRITE "${output_dir}/widget_payload_map.generated.inc" "${_payload_map}")
    file(WRITE "${output_dir}/payload_kinds.generated.inc" "${_payload_kinds}")
    file(WRITE "${output_dir}/widget_catalog_constants.generated.hpp"
        "#pragma once\n#define CHARM_VIVID_WIDGET_KIND_COUNT ${_max_id}+1\n")
endfunction()
