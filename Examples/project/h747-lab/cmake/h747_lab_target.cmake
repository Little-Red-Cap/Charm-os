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

function(h747_lab_collect_vivid_mcu_modules target_name out_modules out_base_dirs)
    set(CHARM_SOURCE_ROOT "${CHARM_ROOT}")
    set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
    set(CHARM_ENABLE_UI_VIVID ON CACHE BOOL "" FORCE)
    set(CHARM_ENABLE_FREETYPE OFF CACHE BOOL "" FORCE)
    set(CHARM_VIVID_FEATURESET "MCU_MIN" CACHE STRING "" FORCE)
    set(CHARM_VIVID_SCREEN_WIDTH "720" CACHE STRING "" FORCE)
    set(CHARM_VIVID_SCREEN_HEIGHT "1280" CACHE STRING "" FORCE)
    set(CHARM_VIVID_SCREEN_PIXEL_FORMAT "RGB888" CACHE STRING "" FORCE)
    set(CHARM_VIVID_LAYER_CACHE_SLOTS "1" CACHE STRING "" FORCE)
    set(CHARM_VIVID_LAYER_CACHE_WIDTH "720" CACHE STRING "" FORCE)
    set(CHARM_VIVID_LAYER_CACHE_HEIGHT "1280" CACHE STRING "" FORCE)
    set(CHARM_VIVID_ENABLE_FLOAT_WIDGETS OFF CACHE BOOL "" FORCE)
    set(CHARM_VIVID_SOA_MAX_NODES "192" CACHE STRING "" FORCE)

    include("${CHARM_ROOT}/Modules/ui/vivid/vivid.cmake")

    set(_modules
        "${CHARM_ROOT}/Modules/gfx/font/font.cppm"
        "${CHARM_ROOT}/Modules/gfx/font/typography.cppm"
        "${CHARM_ROOT}/Modules/gfx/font/font_defaults_noto.cppm"
        "${CHARM_ROOT}/Modules/gfx/font/font_noto_ascii_12.cppm"
        "${CHARM_ROOT}/Modules/gfx/font/font_noto_sc_12.cppm"
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
        "${CHARM_ROOT}/Modules/core/alg/alg_round_rect.cppm"
        "${CHARM_ROOT}/Modules/core/alg/alg_text_layout.cppm"
        "${CHARM_ROOT}/Modules/core/service/service_dirty_rects.cppm"
        "${CHARM_ROOT}/Modules/core/util/core.cppm"
        "${CHARM_ROOT}/Modules/ui/common/ui.render_backend.cppm")

    set(_base_dirs "${CHARM_ROOT}/Modules")
    vivid_collect_modules(${target_name} _modules _base_dirs)

    list(REMOVE_DUPLICATES _modules)
    list(REMOVE_DUPLICATES _base_dirs)
    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_base_dirs} "${_base_dirs}" PARENT_SCOPE)
endfunction()

function(h747_lab_select_linker_script out_script target_name app_name)
    if(app_name STREQUAL "posix_lab")
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
            "${target_name}: non-POSIX H747 Lab target would still reserve the ELF load region. "
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

    add_executable(${H747_LAB_FW_TARGET}
        ${H747_LAB_PLATFORM_SOURCES}
        ${H747_LAB_BOARD_SOURCES}
        ${H747_LAB_RUNTIME_SOURCES}
        ${_service_sources}
        ${H747_LAB_FW_APP_SOURCES}
        ${_generated_app_sources}
        "${_profile_source}"
    )

    if(TARGET ${H747_LAB_FW_TARGET}_elf_samples)
        add_dependencies(${H747_LAB_FW_TARGET} ${H747_LAB_FW_TARGET}_elf_samples)
    endif()

    if(H747_LAB_FW_APP STREQUAL "player_md3")
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
endfunction()
