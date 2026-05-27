if(NOT DEFINED STATIC_REFLECTION_GXX OR STATIC_REFLECTION_GXX STREQUAL "")
    message(FATAL_ERROR "STATIC_REFLECTION_GXX is not set")
endif()

if(NOT DEFINED STATIC_REFLECTION_SOURCE OR STATIC_REFLECTION_SOURCE STREQUAL "")
    message(FATAL_ERROR "STATIC_REFLECTION_SOURCE is not set")
endif()

execute_process(
    COMMAND "${STATIC_REFLECTION_GXX}"
        -std=c++26
        -freflection
        -x c++
        -fsyntax-only
        "${STATIC_REFLECTION_SOURCE}"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_stdout
    ERROR_VARIABLE probe_stderr
)

if(NOT probe_result EQUAL 0)
    message(FATAL_ERROR
        "Static reflection probe failed.\n"
        "Compiler: ${STATIC_REFLECTION_GXX}\n"
        "Source: ${STATIC_REFLECTION_SOURCE}\n"
        "stdout:\n${probe_stdout}\n"
        "stderr:\n${probe_stderr}\n")
endif()

message(STATUS "[static-reflection-probe] ok")
