include_guard(GLOBAL)

set(_VIVID_PROFILE_COMPILER_DIR "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(_VIVID_PROFILE_ROOT "${_VIVID_PROFILE_COMPILER_DIR}/.." ABSOLUTE)
include("${_VIVID_PROFILE_COMPILER_DIR}/widget_catalog_compiler.cmake")

function(_vivid_profile_key out_var value)
    string(SHA256 _key "${value}")
    set(${out_var} "${_key}" PARENT_SCOPE)
endfunction()

function(_vivid_normalize_uint out_var name value max_value)
    if(NOT "${value}" MATCHES "^[0-9]+$")
        message(FATAL_ERROR "${name} must be an unsigned integer, got '${value}'")
    endif()
    string(REGEX REPLACE "^0+" "" _normalized "${value}")
    if(_normalized STREQUAL "")
        set(_normalized 0)
    endif()
    if("${_normalized}" GREATER "${max_value}")
        message(FATAL_ERROR "${name} must be <= ${max_value}, got '${value}'")
    endif()
    set(${out_var} "${_normalized}" PARENT_SCOPE)
endfunction()

function(_vivid_profile_get profile field out_var)
    _vivid_profile_key(_profile_key "${profile}")
    get_property(_value GLOBAL PROPERTY "VIVID_PROFILE_${_profile_key}_${field}")
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

function(_vivid_profile_set profile field value)
    _vivid_profile_key(_profile_key "${profile}")
    set_property(GLOBAL PROPERTY "VIVID_PROFILE_${_profile_key}_${field}" "${value}")
endfunction()

function(_vivid_merge_payload_capacities out_var)
    set(_merged "${ARGN}")
    set(_result "")
    set(_pools "")
    foreach(_entry IN LISTS _merged)
        if(NOT _entry MATCHES "^([A-Za-z][A-Za-z0-9_]*)=([0-9]+)$")
            message(FATAL_ERROR
                "Vivid payload capacity must use Pool=Value, got '${_entry}'")
        endif()
        set(_pool "${CMAKE_MATCH_1}")
        _vivid_normalize_uint(
            _capacity "Vivid payload capacity ${_pool}" "${CMAKE_MATCH_2}" 65535)
        list(FIND _pools "${_pool}" _existing_index)
        if(NOT _existing_index EQUAL -1)
            list(REMOVE_AT _result ${_existing_index})
            list(REMOVE_AT _pools ${_existing_index})
        endif()
        list(APPEND _pools "${_pool}")
        list(APPEND _result "${_pool}=${_capacity}")
    endforeach()
    set(${out_var} "${_result}" PARENT_SCOPE)
endfunction()

function(vivid_define_product_profile)
    set(_options)
    set(_one_value
        NAME EXTENDS SOA_MAX_NODES SOA_TEXT_ARENA_BYTES SEMANTIC_SLOT_CAP
        STYLE_PATCH_SLOT_CAP STYLE_CLASS_MAX STYLE_RULE_CAP STYLE_METRICS_POOL_CAP
        DRAW_CMD_MAX_COMMANDS DRAW_CMD_TEXT_BYTES DRAW_CMD_BLOB_BYTES
        FLOAT_WIDGETS)
    set(_multi_value
        ROOT_MODULES WIDGET_KINDS OBJECT_WIDGET_KINDS PAYLOAD_CAPACITIES)
    cmake_parse_arguments(PARSE_ARGV 0 PROFILE
        "${_options}" "${_one_value}" "${_multi_value}")

    if(PROFILE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "vivid_define_product_profile: unknown arguments: ${PROFILE_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT PROFILE_NAME MATCHES "^[A-Za-z][A-Za-z0-9_.-]*$")
        message(FATAL_ERROR
            "vivid_define_product_profile: invalid NAME '${PROFILE_NAME}'")
    endif()

    _vivid_profile_key(_profile_key "${PROFILE_NAME}")
    get_property(_already_defined GLOBAL
        PROPERTY "VIVID_PROFILE_${_profile_key}_DEFINED" SET)
    if(_already_defined)
        message(FATAL_ERROR "Vivid product profile '${PROFILE_NAME}' is already defined")
    endif()

    if(PROFILE_EXTENDS STREQUAL PROFILE_NAME)
        message(FATAL_ERROR
            "Vivid product profile inheritance cycle: ${PROFILE_NAME};${PROFILE_NAME}")
    endif()

    set(_roots "")
    set(_kinds "")
    set(_object_kinds "")
    set(_payload_caps "")
    if(PROFILE_EXTENDS)
        _vivid_profile_get("${PROFILE_EXTENDS}" DEFINED _base_defined)
        if(NOT _base_defined)
            message(FATAL_ERROR
                "Vivid product profile '${PROFILE_NAME}' extends unknown profile '${PROFILE_EXTENDS}'")
        endif()
        _vivid_profile_get("${PROFILE_EXTENDS}" ROOT_MODULES _roots)
        _vivid_profile_get("${PROFILE_EXTENDS}" WIDGET_KINDS _kinds)
        _vivid_profile_get(
            "${PROFILE_EXTENDS}" OBJECT_WIDGET_KINDS _object_kinds)
        _vivid_profile_get("${PROFILE_EXTENDS}" PAYLOAD_CAPACITIES _payload_caps)
        foreach(_field IN LISTS _one_value)
            if(_field STREQUAL "NAME" OR _field STREQUAL "EXTENDS")
                continue()
            endif()
            if(NOT DEFINED PROFILE_${_field} OR "${PROFILE_${_field}}" STREQUAL "")
                _vivid_profile_get("${PROFILE_EXTENDS}" "${_field}" _base_value)
                set(PROFILE_${_field} "${_base_value}")
            endif()
        endforeach()
    endif()

    list(APPEND _roots ${PROFILE_ROOT_MODULES})
    list(APPEND _kinds ${PROFILE_WIDGET_KINDS})
    list(APPEND _object_kinds ${PROFILE_OBJECT_WIDGET_KINDS})
    list(REMOVE_DUPLICATES _roots)
    list(REMOVE_DUPLICATES _kinds)
    list(REMOVE_DUPLICATES _object_kinds)
    _vivid_merge_payload_capacities(
        _payload_caps ${_payload_caps} ${PROFILE_PAYLOAD_CAPACITIES})

    if(NOT _roots)
        message(FATAL_ERROR "Vivid product profile '${PROFILE_NAME}' has no ROOT_MODULES")
    endif()
    if(NOT _kinds)
        message(FATAL_ERROR "Vivid product profile '${PROFILE_NAME}' has no WIDGET_KINDS")
    endif()

    foreach(_field IN ITEMS
            SOA_MAX_NODES SOA_TEXT_ARENA_BYTES SEMANTIC_SLOT_CAP STYLE_PATCH_SLOT_CAP
            STYLE_CLASS_MAX STYLE_RULE_CAP
            STYLE_METRICS_POOL_CAP DRAW_CMD_MAX_COMMANDS DRAW_CMD_TEXT_BYTES
            DRAW_CMD_BLOB_BYTES FLOAT_WIDGETS)
        if(NOT DEFINED PROFILE_${_field} OR "${PROFILE_${_field}}" STREQUAL "")
            message(FATAL_ERROR
                "Vivid product profile '${PROFILE_NAME}' requires ${_field}")
        endif()
    endforeach()

    foreach(_field IN ITEMS
            SOA_MAX_NODES SOA_TEXT_ARENA_BYTES STYLE_CLASS_MAX STYLE_RULE_CAP
            DRAW_CMD_MAX_COMMANDS DRAW_CMD_TEXT_BYTES DRAW_CMD_BLOB_BYTES)
        _vivid_normalize_uint(
            _normalized_value
            "Vivid profile ${PROFILE_NAME} ${_field}"
            "${PROFILE_${_field}}" 4294967295)
        set(PROFILE_${_field} "${_normalized_value}")
        if("${PROFILE_${_field}}" LESS 1)
            message(FATAL_ERROR
                "Vivid profile ${PROFILE_NAME} ${_field} must be > 0")
        endif()
    endforeach()
    if("${PROFILE_STYLE_CLASS_MAX}" GREATER 256)
        message(FATAL_ERROR
            "Vivid profile ${PROFILE_NAME} STYLE_CLASS_MAX must be <= 256")
    endif()
    _vivid_normalize_uint(
        PROFILE_SEMANTIC_SLOT_CAP
        "Vivid profile ${PROFILE_NAME} SEMANTIC_SLOT_CAP"
        "${PROFILE_SEMANTIC_SLOT_CAP}" 255)
    if("${PROFILE_SEMANTIC_SLOT_CAP}" LESS 1)
        message(FATAL_ERROR
            "Vivid profile ${PROFILE_NAME} SEMANTIC_SLOT_CAP must be > 0")
    endif()
    if("${PROFILE_SEMANTIC_SLOT_CAP}" GREATER "${PROFILE_SOA_MAX_NODES}")
        message(FATAL_ERROR
            "Vivid profile ${PROFILE_NAME} SEMANTIC_SLOT_CAP must be <= SOA_MAX_NODES")
    endif()
    _vivid_normalize_uint(
        PROFILE_STYLE_PATCH_SLOT_CAP
        "Vivid profile ${PROFILE_NAME} STYLE_PATCH_SLOT_CAP"
        "${PROFILE_STYLE_PATCH_SLOT_CAP}" 255)
    if("${PROFILE_STYLE_PATCH_SLOT_CAP}" LESS 1)
        message(FATAL_ERROR
            "Vivid profile ${PROFILE_NAME} STYLE_PATCH_SLOT_CAP must be > 0")
    endif()
    if("${PROFILE_STYLE_PATCH_SLOT_CAP}" GREATER "${PROFILE_SOA_MAX_NODES}")
        message(FATAL_ERROR
            "Vivid profile ${PROFILE_NAME} STYLE_PATCH_SLOT_CAP must be <= SOA_MAX_NODES")
    endif()
    _vivid_normalize_uint(
        PROFILE_STYLE_METRICS_POOL_CAP
        "Vivid profile ${PROFILE_NAME} STYLE_METRICS_POOL_CAP"
        "${PROFILE_STYLE_METRICS_POOL_CAP}" 255)
    if("${PROFILE_STYLE_METRICS_POOL_CAP}" LESS 1)
        message(FATAL_ERROR
            "Vivid profile ${PROFILE_NAME} STYLE_METRICS_POOL_CAP must be > 0")
    endif()
    if(NOT PROFILE_FLOAT_WIDGETS MATCHES "^(ON|OFF|TRUE|FALSE|0|1)$")
        message(FATAL_ERROR
            "Vivid profile ${PROFILE_NAME} FLOAT_WIDGETS must be ON or OFF")
    endif()
    if(PROFILE_FLOAT_WIDGETS)
        set(PROFILE_FLOAT_WIDGETS ON)
    else()
        set(PROFILE_FLOAT_WIDGETS OFF)
    endif()

    list(SORT _roots)
    list(SORT _kinds)
    list(SORT _object_kinds)
    list(SORT _payload_caps)
    vivid_widget_catalog_fingerprint(_catalog_fingerprint)
    set(_canonical
        "schema=4\ncatalog=${_catalog_fingerprint}\nroots=${_roots}\n"
        "kinds=${_kinds}\nobject_kinds=${_object_kinds}\n"
        "payloads=${_payload_caps}\n"
        "soa_max_nodes=${PROFILE_SOA_MAX_NODES}\n"
        "soa_text_arena_bytes=${PROFILE_SOA_TEXT_ARENA_BYTES}\n"
        "semantic_slot_cap=${PROFILE_SEMANTIC_SLOT_CAP}\n"
        "style=${PROFILE_STYLE_PATCH_SLOT_CAP},${PROFILE_STYLE_CLASS_MAX},${PROFILE_STYLE_RULE_CAP},${PROFILE_STYLE_METRICS_POOL_CAP}\n"
        "draw_cmd=${PROFILE_DRAW_CMD_MAX_COMMANDS},${PROFILE_DRAW_CMD_TEXT_BYTES},${PROFILE_DRAW_CMD_BLOB_BYTES}\n"
        "float_widgets=${PROFILE_FLOAT_WIDGETS}\n")
    string(SHA256 _fingerprint "${_canonical}")

    _vivid_profile_set("${PROFILE_NAME}" DEFINED TRUE)
    _vivid_profile_set("${PROFILE_NAME}" NAME "${PROFILE_NAME}")
    _vivid_profile_set("${PROFILE_NAME}" EXTENDS "${PROFILE_EXTENDS}")
    _vivid_profile_set("${PROFILE_NAME}" ROOT_MODULES "${_roots}")
    _vivid_profile_set("${PROFILE_NAME}" WIDGET_KINDS "${_kinds}")
    _vivid_profile_set(
        "${PROFILE_NAME}" OBJECT_WIDGET_KINDS "${_object_kinds}")
    _vivid_profile_set("${PROFILE_NAME}" PAYLOAD_CAPACITIES "${_payload_caps}")
    foreach(_field IN ITEMS
            SOA_MAX_NODES SOA_TEXT_ARENA_BYTES SEMANTIC_SLOT_CAP STYLE_PATCH_SLOT_CAP
            STYLE_CLASS_MAX STYLE_RULE_CAP
            STYLE_METRICS_POOL_CAP DRAW_CMD_MAX_COMMANDS DRAW_CMD_TEXT_BYTES
            DRAW_CMD_BLOB_BYTES FLOAT_WIDGETS)
        _vivid_profile_set("${PROFILE_NAME}" "${_field}" "${PROFILE_${_field}}")
    endforeach()
    _vivid_profile_set("${PROFILE_NAME}" CANONICAL "${_canonical}")
    _vivid_profile_set("${PROFILE_NAME}" CATALOG_FINGERPRINT "${_catalog_fingerprint}")
    _vivid_profile_set("${PROFILE_NAME}" FINGERPRINT "${_fingerprint}")
endfunction()

function(_vivid_target_get target field out_var)
    _vivid_profile_key(_target_key "${target}")
    get_property(_value GLOBAL PROPERTY "VIVID_TARGET_${_target_key}_${field}")
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

function(_vivid_target_set target field value)
    _vivid_profile_key(_target_key "${target}")
    set_property(GLOBAL PROPERTY "VIVID_TARGET_${_target_key}_${field}" "${value}")
endfunction()

function(vivid_configure_product_target)
    set(_options DRAW_DETAIL_EVIDENCE)
    set(_one_value
        TARGET PROFILE SCREEN_WIDTH SCREEN_HEIGHT PIXEL_FORMAT
        LAYER_CACHE_SLOTS LAYER_CACHE_WIDTH LAYER_CACHE_HEIGHT
        RUNTIME_SCENE_INSTANCES STATIC_MEMORY_BUDGET_BYTES
        STATIC_MEMORY_MIN_HEADROOM_BYTES MAX_HOT_STACK_FRAME_BYTES)
    cmake_parse_arguments(PARSE_ARGV 0 TARGET_PROFILE
        "${_options}" "${_one_value}" "")

    if(TARGET_PROFILE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "vivid_configure_product_target: unknown arguments: ${TARGET_PROFILE_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT TARGET_PROFILE_TARGET)
        message(FATAL_ERROR "vivid_configure_product_target requires TARGET")
    endif()
    if(NOT TARGET_PROFILE_PROFILE)
        message(FATAL_ERROR "vivid_configure_product_target requires PROFILE")
    endif()
    _vivid_profile_get("${TARGET_PROFILE_PROFILE}" DEFINED _profile_defined)
    if(NOT _profile_defined)
        message(FATAL_ERROR
            "Vivid target '${TARGET_PROFILE_TARGET}' selects unknown profile '${TARGET_PROFILE_PROFILE}'")
    endif()

    foreach(_field IN LISTS _one_value)
        if(_field STREQUAL "TARGET" OR _field STREQUAL "PROFILE")
            continue()
        endif()
        if(NOT DEFINED TARGET_PROFILE_${_field}
            OR "${TARGET_PROFILE_${_field}}" STREQUAL "")
            message(FATAL_ERROR
                "Vivid target '${TARGET_PROFILE_TARGET}' requires ${_field}")
        endif()
    endforeach()
    if(NOT TARGET_PROFILE_PIXEL_FORMAT MATCHES "^(RGB565|RGB888)$")
        message(FATAL_ERROR
            "Vivid target '${TARGET_PROFILE_TARGET}' PIXEL_FORMAT must be RGB565 or RGB888")
    endif()
    foreach(_field IN ITEMS
            SCREEN_WIDTH SCREEN_HEIGHT LAYER_CACHE_SLOTS LAYER_CACHE_WIDTH
            LAYER_CACHE_HEIGHT RUNTIME_SCENE_INSTANCES STATIC_MEMORY_BUDGET_BYTES
            STATIC_MEMORY_MIN_HEADROOM_BYTES MAX_HOT_STACK_FRAME_BYTES)
        _vivid_normalize_uint(
            _normalized_value
            "Vivid target ${TARGET_PROFILE_TARGET} ${_field}"
            "${TARGET_PROFILE_${_field}}" 4294967295)
        set(TARGET_PROFILE_${_field} "${_normalized_value}")
        if("${TARGET_PROFILE_${_field}}" LESS 1)
            message(FATAL_ERROR
                "Vivid target ${TARGET_PROFILE_TARGET} ${_field} must be > 0")
        endif()
    endforeach()

    _vivid_target_get("${TARGET_PROFILE_TARGET}" CONFIGURED _configured)
    _vivid_target_get("${TARGET_PROFILE_TARGET}" PROFILE _configured_profile)
    if(_configured AND NOT _configured_profile STREQUAL TARGET_PROFILE_PROFILE)
        message(FATAL_ERROR
            "Vivid target '${TARGET_PROFILE_TARGET}' already uses profile '${_configured_profile}', "
            "cannot also select '${TARGET_PROFILE_PROFILE}'")
    endif()

    _vivid_profile_get("${TARGET_PROFILE_PROFILE}" FINGERPRINT _profile_fingerprint)
    if(TARGET_PROFILE_DRAW_DETAIL_EVIDENCE)
        set(_draw_detail_evidence ON)
    else()
        set(_draw_detail_evidence OFF)
    endif()
    set(_envelope
        "schema=1\nscreen=${TARGET_PROFILE_SCREEN_WIDTH}x${TARGET_PROFILE_SCREEN_HEIGHT}:${TARGET_PROFILE_PIXEL_FORMAT}\n"
        "layer_cache=${TARGET_PROFILE_LAYER_CACHE_SLOTS},${TARGET_PROFILE_LAYER_CACHE_WIDTH}x${TARGET_PROFILE_LAYER_CACHE_HEIGHT}\n"
        "scene_instances=${TARGET_PROFILE_RUNTIME_SCENE_INSTANCES}\n"
        "memory=${TARGET_PROFILE_STATIC_MEMORY_BUDGET_BYTES},${TARGET_PROFILE_STATIC_MEMORY_MIN_HEADROOM_BYTES}\n"
        "stack=${TARGET_PROFILE_MAX_HOT_STACK_FRAME_BYTES}\n"
        "draw_detail_evidence=${_draw_detail_evidence}\n")
    string(SHA256 _target_fingerprint "${_profile_fingerprint}\n${_envelope}")
    if(_configured)
        _vivid_target_get(
            "${TARGET_PROFILE_TARGET}" TARGET_FINGERPRINT _configured_target_fingerprint)
        if(NOT _configured_target_fingerprint STREQUAL _target_fingerprint)
            message(FATAL_ERROR
                "Vivid target '${TARGET_PROFILE_TARGET}' is already configured with a different envelope")
        endif()
        return()
    endif()

    _vivid_target_set("${TARGET_PROFILE_TARGET}" CONFIGURED TRUE)
    _vivid_target_set("${TARGET_PROFILE_TARGET}" PROFILE "${TARGET_PROFILE_PROFILE}")
    _vivid_target_set("${TARGET_PROFILE_TARGET}" PROFILE_FINGERPRINT "${_profile_fingerprint}")
    _vivid_target_set("${TARGET_PROFILE_TARGET}" TARGET_FINGERPRINT "${_target_fingerprint}")
    _vivid_target_set("${TARGET_PROFILE_TARGET}" ENVELOPE "${_envelope}")
    foreach(_field IN ITEMS
            SCREEN_WIDTH SCREEN_HEIGHT PIXEL_FORMAT LAYER_CACHE_SLOTS
            LAYER_CACHE_WIDTH LAYER_CACHE_HEIGHT RUNTIME_SCENE_INSTANCES
            STATIC_MEMORY_BUDGET_BYTES STATIC_MEMORY_MIN_HEADROOM_BYTES
            MAX_HOT_STACK_FRAME_BYTES)
        _vivid_target_set(
            "${TARGET_PROFILE_TARGET}" "${_field}" "${TARGET_PROFILE_${_field}}")
    endforeach()
    _vivid_target_set(
        "${TARGET_PROFILE_TARGET}" DRAW_DETAIL_EVIDENCE "${_draw_detail_evidence}")
endfunction()

function(vivid_module_policy)
    set(_options)
    set(_one_value NAME ACCESS)
    cmake_parse_arguments(PARSE_ARGV 0 POLICY "${_options}" "${_one_value}" "")
    if(POLICY_UNPARSED_ARGUMENTS OR NOT POLICY_NAME
        OR NOT POLICY_ACCESS MATCHES "^(PRODUCT_ROOT|INTERNAL|HOST_ONLY)$")
        message(FATAL_ERROR
            "vivid_module_policy requires NAME and ACCESS=PRODUCT_ROOT|INTERNAL|HOST_ONLY")
    endif()
    _vivid_profile_key(_module_key "${POLICY_NAME}")
    _vivid_module_property("${POLICY_NAME}" SOURCE _module_source)
    if(NOT _module_source)
        message(FATAL_ERROR
            "Vivid module policy references unknown module '${POLICY_NAME}'")
    endif()
    get_property(_existing_policy GLOBAL PROPERTY "VIVID_MODULE_POLICY_${_module_key}")
    if(_existing_policy AND NOT _existing_policy STREQUAL POLICY_ACCESS)
        message(FATAL_ERROR
            "Vivid module '${POLICY_NAME}' has conflicting policies '${_existing_policy}' and '${POLICY_ACCESS}'")
    endif()
    set_property(GLOBAL PROPERTY "VIVID_MODULE_POLICY_${_module_key}" "${POLICY_ACCESS}")
endfunction()

function(_vivid_module_property module field out_var)
    _vivid_profile_key(_module_key "${module}")
    get_property(_value GLOBAL PROPERTY "VIVID_MODULE_${_module_key}_${field}")
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

function(_vivid_register_module module source imports)
    _vivid_profile_key(_module_key "${module}")
    get_property(_existing GLOBAL PROPERTY "VIVID_MODULE_${_module_key}_SOURCE")
    if(_existing AND NOT "${_existing}" STREQUAL "${source}")
        message(FATAL_ERROR
            "Duplicate Vivid module '${module}': '${_existing}' and '${source}'")
    endif()
    set_property(GLOBAL PROPERTY "VIVID_MODULE_${_module_key}_NAME" "${module}")
    set_property(GLOBAL PROPERTY "VIVID_MODULE_${_module_key}_SOURCE" "${source}")
    set_property(GLOBAL PROPERTY "VIVID_MODULE_${_module_key}_IMPORTS" "${imports}")
    get_property(_modules GLOBAL PROPERTY VIVID_MODULE_NAMES)
    list(APPEND _modules "${module}")
    list(REMOVE_DUPLICATES _modules)
    set_property(GLOBAL PROPERTY VIVID_MODULE_NAMES "${_modules}")
endfunction()

function(vivid_build_module_inventory)
    get_property(_ready GLOBAL PROPERTY VIVID_MODULE_INVENTORY_READY)
    if(_ready)
        return()
    endif()

    if(CMAKE_SCRIPT_MODE_FILE)
        file(GLOB_RECURSE _sources "${_VIVID_PROFILE_ROOT}/*.cppm")
    else()
        file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
            "${_VIVID_PROFILE_ROOT}/*.cppm")
    endif()
    list(FILTER _sources EXCLUDE REGEX "/cmake-build-")
    foreach(_source IN LISTS _sources)
        file(STRINGS "${_source}" _lines ENCODING UTF-8)
        set(_module "")
        set(_raw_imports "")
        foreach(_line IN LISTS _lines)
            if(_line MATCHES
                "^[ \t]*(export[ \t]+)?module[ \t]+(charm\\.[A-Za-z0-9_.]+(:[A-Za-z0-9_.]+)?)[ \t]*;")
                if(_module)
                    message(FATAL_ERROR
                        "Vivid source '${_source}' declares more than one named module")
                endif()
                set(_module "${CMAKE_MATCH_2}")
            elseif(_line MATCHES
                "^[ \t]*(export[ \t]+)?import[ \t]+(charm\\.[A-Za-z0-9_.]+(:[A-Za-z0-9_.]+)?)[ \t]*;")
                list(APPEND _raw_imports "${CMAKE_MATCH_2}")
            elseif(_line MATCHES
                "^[ \t]*(export[ \t]+)?import[ \t]+(:[A-Za-z0-9_.]+)[ \t]*;")
                list(APPEND _raw_imports "${CMAKE_MATCH_2}")
            elseif(_line MATCHES "^[ \t]*(export[ \t]+)?import[ \t]+[^;]+$")
                message(FATAL_ERROR
                    "Vivid import must be a single semicolon-terminated line: ${_source}: ${_line}")
            endif()
        endforeach()
        if(NOT _module)
            message(FATAL_ERROR "Vivid source '${_source}' has no named module declaration")
        endif()

        string(REGEX REPLACE ":.*$" "" _primary_module "${_module}")
        set(_imports "")
        foreach(_import IN LISTS _raw_imports)
            if(_import MATCHES "^:")
                list(APPEND _imports "${_primary_module}${_import}")
            else()
                list(APPEND _imports "${_import}")
            endif()
        endforeach()
        list(REMOVE_DUPLICATES _imports)
        _vivid_register_module("${_module}" "${_source}" "${_imports}")
    endforeach()

    include("${_VIVID_PROFILE_COMPILER_DIR}/vivid_module_policy.cmake")
    set_property(GLOBAL PROPERTY VIVID_MODULE_INVENTORY_READY TRUE)
endfunction()

function(_vivid_visit_module closure_key module path)
    _vivid_profile_key(_module_key "${module}")
    get_property(_state GLOBAL
        PROPERTY "VIVID_CLOSURE_${closure_key}_${_module_key}_STATE")
    if(_state STREQUAL "DONE")
        return()
    endif()
    if(_state STREQUAL "VISITING")
        message(FATAL_ERROR "Vivid module dependency cycle: ${path};${module}")
    endif()

    _vivid_module_property("${module}" SOURCE _source)
    if(NOT _source)
        get_property(_external GLOBAL PROPERTY "VIVID_CLOSURE_${closure_key}_EXTERNAL")
        list(APPEND _external "${module}")
        list(REMOVE_DUPLICATES _external)
        set_property(GLOBAL PROPERTY "VIVID_CLOSURE_${closure_key}_EXTERNAL" "${_external}")
        return()
    endif()

    get_property(_policy GLOBAL PROPERTY "VIVID_MODULE_POLICY_${_module_key}")
    if(NOT _policy)
        set(_policy INTERNAL)
    endif()
    if(_policy STREQUAL "HOST_ONLY")
        message(FATAL_ERROR
            "Vivid PRODUCT closure reaches host-only module '${module}' via ${path};${module}")
    endif()

    set_property(GLOBAL PROPERTY
        "VIVID_CLOSURE_${closure_key}_${_module_key}_STATE" VISITING)
    _vivid_module_property("${module}" IMPORTS _imports)
    foreach(_import IN LISTS _imports)
        _vivid_visit_module("${closure_key}" "${_import}" "${path};${module}")
    endforeach()
    set_property(GLOBAL PROPERTY
        "VIVID_CLOSURE_${closure_key}_${_module_key}_STATE" DONE)

    get_property(_modules GLOBAL PROPERTY "VIVID_CLOSURE_${closure_key}_MODULES")
    get_property(_sources GLOBAL PROPERTY "VIVID_CLOSURE_${closure_key}_SOURCES")
    list(APPEND _modules "${module}")
    list(APPEND _sources "${_source}")
    list(REMOVE_DUPLICATES _modules)
    list(REMOVE_DUPLICATES _sources)
    set_property(GLOBAL PROPERTY "VIVID_CLOSURE_${closure_key}_MODULES" "${_modules}")
    set_property(GLOBAL PROPERTY "VIVID_CLOSURE_${closure_key}_SOURCES" "${_sources}")
endfunction()

function(vivid_compute_product_module_closure out_sources out_modules out_external)
    set(_options)
    set(_one_value KEY)
    set(_multi_value ROOT_MODULES INTERNAL_ROOT_MODULES)
    cmake_parse_arguments(PARSE_ARGV 3 CLOSURE
        "${_options}" "${_one_value}" "${_multi_value}")
    if(CLOSURE_UNPARSED_ARGUMENTS OR NOT CLOSURE_KEY)
        message(FATAL_ERROR
            "vivid_compute_product_module_closure requires KEY and ROOT_MODULES")
    endif()
    vivid_build_module_inventory()
    _vivid_profile_key(_closure_key "${CLOSURE_KEY}")
    set_property(GLOBAL PROPERTY "VIVID_CLOSURE_${_closure_key}_MODULES" "")
    set_property(GLOBAL PROPERTY "VIVID_CLOSURE_${_closure_key}_SOURCES" "")
    set_property(GLOBAL PROPERTY "VIVID_CLOSURE_${_closure_key}_EXTERNAL" "")

    foreach(_root IN LISTS CLOSURE_ROOT_MODULES)
        _vivid_profile_key(_root_key "${_root}")
        _vivid_module_property("${_root}" SOURCE _root_source)
        if(NOT _root_source)
            message(FATAL_ERROR "Unknown Vivid PRODUCT root module '${_root}'")
        endif()
        get_property(_root_policy GLOBAL PROPERTY "VIVID_MODULE_POLICY_${_root_key}")
        if(NOT _root_policy)
            set(_root_policy INTERNAL)
        endif()
        if(NOT _root_policy STREQUAL "PRODUCT_ROOT")
            message(FATAL_ERROR
                "Vivid module '${_root}' is ${_root_policy}; PRODUCT profiles may select only PRODUCT_ROOT modules")
        endif()
        _vivid_visit_module("${_closure_key}" "${_root}" "<profile-root>")
    endforeach()
    foreach(_root IN LISTS CLOSURE_INTERNAL_ROOT_MODULES)
        _vivid_module_property("${_root}" SOURCE _root_source)
        if(NOT _root_source)
            message(FATAL_ERROR "Unknown Vivid internal closure root '${_root}'")
        endif()
        _vivid_visit_module("${_closure_key}" "${_root}" "<catalog-root>")
    endforeach()

    get_property(_modules GLOBAL PROPERTY "VIVID_CLOSURE_${_closure_key}_MODULES")
    get_property(_sources GLOBAL PROPERTY "VIVID_CLOSURE_${_closure_key}_SOURCES")
    get_property(_external GLOBAL PROPERTY "VIVID_CLOSURE_${_closure_key}_EXTERNAL")
    list(SORT _external)
    set(${out_sources} "${_sources}" PARENT_SCOPE)
    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_external} "${_external}" PARENT_SCOPE)
endfunction()
