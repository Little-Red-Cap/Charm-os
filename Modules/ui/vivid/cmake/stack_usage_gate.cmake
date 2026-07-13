if(NOT DEFINED VIVID_STACK_USAGE_ROOT OR VIVID_STACK_USAGE_ROOT STREQUAL "")
    message(FATAL_ERROR "vivid-stack-usage: missing VIVID_STACK_USAGE_ROOT")
endif()
if(NOT DEFINED VIVID_STACK_USAGE_OUT OR VIVID_STACK_USAGE_OUT STREQUAL "")
    message(FATAL_ERROR "vivid-stack-usage: missing VIVID_STACK_USAGE_OUT")
endif()
if(NOT DEFINED VIVID_STACK_USAGE_SOURCE_MANIFEST
    OR NOT EXISTS "${VIVID_STACK_USAGE_SOURCE_MANIFEST}")
    message(FATAL_ERROR
        "vivid-stack-usage: missing VIVID_STACK_USAGE_SOURCE_MANIFEST")
endif()
if(NOT DEFINED VIVID_STACK_USAGE_MAX_BYTES OR NOT VIVID_STACK_USAGE_MAX_BYTES MATCHES "^[0-9]+$")
    message(FATAL_ERROR "vivid-stack-usage: invalid VIVID_STACK_USAGE_MAX_BYTES")
endif()
if(NOT DEFINED VIVID_STACK_USAGE_ENFORCE)
    set(VIVID_STACK_USAGE_ENFORCE ON)
endif()

file(GLOB_RECURSE _vivid_stack_usage_files LIST_DIRECTORIES FALSE
    "${VIVID_STACK_USAGE_ROOT}/*.su")
if(NOT _vivid_stack_usage_files)
    message(FATAL_ERROR
        "vivid-stack-usage: no .su files under '${VIVID_STACK_USAGE_ROOT}'")
endif()
file(STRINGS
    "${VIVID_STACK_USAGE_SOURCE_MANIFEST}"
    _vivid_stack_usage_sources
    ENCODING UTF-8)
if(NOT _vivid_stack_usage_sources)
    message(FATAL_ERROR "vivid-stack-usage: selected Vivid module list is empty")
endif()
list(TRANSFORM _vivid_stack_usage_sources STRIP)

set(_vivid_stack_entry_count 0)
set(_vivid_stack_max_bytes 0)
set(_vivid_stack_max_function "")
set(_vivid_stack_violations "")
set(_vivid_stack_entries "")

foreach(_vivid_stack_usage_file IN LISTS _vivid_stack_usage_files)
    file(STRINGS "${_vivid_stack_usage_file}" _vivid_stack_usage_lines ENCODING UTF-8)
    foreach(_vivid_stack_usage_line IN LISTS _vivid_stack_usage_lines)
        string(REPLACE "\t" ";" _vivid_stack_usage_fields "${_vivid_stack_usage_line}")
        list(LENGTH _vivid_stack_usage_fields _vivid_stack_usage_field_count)
        if(_vivid_stack_usage_field_count LESS 3)
            continue()
        endif()
        list(GET _vivid_stack_usage_fields 0 _vivid_stack_function)
        list(GET _vivid_stack_usage_fields 1 _vivid_stack_bytes)
        list(GET _vivid_stack_usage_fields 2 _vivid_stack_kind)
        string(REPLACE "\\" "/" _vivid_stack_function "${_vivid_stack_function}")
        if(NOT _vivid_stack_bytes MATCHES "^[0-9]+$")
            continue()
        endif()
        set(_vivid_stack_source_selected FALSE)
        foreach(_vivid_stack_usage_source IN LISTS _vivid_stack_usage_sources)
            string(FIND
                "${_vivid_stack_function}"
                "${_vivid_stack_usage_source}:"
                _vivid_stack_source_pos)
            if(NOT _vivid_stack_source_pos EQUAL -1)
                set(_vivid_stack_source_selected TRUE)
                break()
            endif()
        endforeach()
        if(NOT _vivid_stack_source_selected)
            continue()
        endif()
        math(EXPR _vivid_stack_entry_count "${_vivid_stack_entry_count} + 1")
        list(APPEND _vivid_stack_entries
            "${_vivid_stack_bytes}|${_vivid_stack_kind}|${_vivid_stack_function}")

        if(_vivid_stack_bytes GREATER _vivid_stack_max_bytes)
            set(_vivid_stack_max_bytes "${_vivid_stack_bytes}")
            set(_vivid_stack_max_function "${_vivid_stack_function}")
        endif()

        set(_vivid_stack_unbounded FALSE)
        if(_vivid_stack_kind MATCHES "dynamic" AND NOT _vivid_stack_kind MATCHES "bounded")
            set(_vivid_stack_unbounded TRUE)
        endif()
        if(_vivid_stack_unbounded OR _vivid_stack_bytes GREATER VIVID_STACK_USAGE_MAX_BYTES)
            list(APPEND _vivid_stack_violations
                "${_vivid_stack_bytes}|${_vivid_stack_kind}|${_vivid_stack_function}")
        endif()
    endforeach()
endforeach()

if(_vivid_stack_entry_count EQUAL 0)
    message(FATAL_ERROR "vivid-stack-usage: no Vivid stack entries found")
endif()

get_filename_component(_vivid_stack_usage_out_dir "${VIVID_STACK_USAGE_OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_vivid_stack_usage_out_dir}")
string(CONCAT _vivid_stack_manifest
    "# Generated Vivid stack usage evidence.\n"
    "profile_fingerprint=${VIVID_STACK_USAGE_PROFILE_FINGERPRINT}\n"
    "target_fingerprint=${VIVID_STACK_USAGE_TARGET_FINGERPRINT}\n"
    "entry_count=${_vivid_stack_entry_count}\n"
    "max_allowed_bytes=${VIVID_STACK_USAGE_MAX_BYTES}\n"
    "max_observed_bytes=${_vivid_stack_max_bytes}\n"
    "max_observed_function=${_vivid_stack_max_function}\n"
    "violation_count=0\n"
    "\n[entries]\n")
foreach(_vivid_stack_entry IN LISTS _vivid_stack_entries)
    string(APPEND _vivid_stack_manifest "${_vivid_stack_entry}\n")
endforeach()
list(LENGTH _vivid_stack_violations _vivid_stack_violation_count)
string(REPLACE "violation_count=0"
    "violation_count=${_vivid_stack_violation_count}"
    _vivid_stack_manifest "${_vivid_stack_manifest}")
file(WRITE "${VIVID_STACK_USAGE_OUT}" "${_vivid_stack_manifest}")

if(VIVID_STACK_USAGE_ENFORCE AND _vivid_stack_violations)
    list(GET _vivid_stack_violations 0 _vivid_stack_first_violation)
    message(FATAL_ERROR
        "vivid-stack-usage: limit=${VIVID_STACK_USAGE_MAX_BYTES} "
        "violations=${_vivid_stack_violation_count} first=${_vivid_stack_first_violation}")
endif()

message(STATUS
    "Vivid stack usage: entries=${_vivid_stack_entry_count} "
    "max=${_vivid_stack_max_bytes} limit=${VIVID_STACK_USAGE_MAX_BYTES} "
    "violations=${_vivid_stack_violation_count}")
