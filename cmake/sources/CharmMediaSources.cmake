include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/CharmSourceHelpers.cmake")

function(charm_collect_media_sources out_modules out_base_dirs)
    charm_collect_cppm(_modules
        "${CHARM_SOURCE_ROOT}/Modules/media/*.cppm"
    )

    if (NOT CHARM_ENABLE_SDL3)
        list(REMOVE_ITEM _modules
            "${CHARM_SOURCE_ROOT}/Modules/media/audio/audio_sink_sdl3.cppm")
    endif()

    set(_base_dirs "${CHARM_SOURCE_ROOT}/Modules")
    if (CHARM_AUDIO_SINK_I2S)
        set(_i2s_module
            "${CHARM_SOURCE_ROOT}/Examples/project/player/stn32common/audio_sink_i2s.cppm")
        if (EXISTS "${_i2s_module}")
            list(APPEND _modules "${_i2s_module}")
            list(APPEND _base_dirs
                "${CHARM_SOURCE_ROOT}/Examples/project/player/stn32common")
        else()
            message(FATAL_ERROR
                "CHARM_AUDIO_SINK_I2S=ON but module is missing: ${_i2s_module}")
        endif()
    endif()

    list(REMOVE_DUPLICATES _base_dirs)
    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_base_dirs} "${_base_dirs}" PARENT_SCOPE)
endfunction()

function(charm_collect_audio_component_sources out_modules out_base_dirs)
    set(_modules
        "${CHARM_SOURCE_ROOT}/Modules/core/util/core.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/core/util/expected.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/core/util/error.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/core/init/init.node.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/init/init.binding.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/system/clock/system_clock.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/platform/win/time_source.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/io/channel/io.channel.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/core/service/ring_queue.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/core/service/queue.cppm"
        "${CHARM_SOURCE_ROOT}/Modules/core/alg/alg_fft.cppm"
    )

    charm_collect_cppm(_out_modules
        "${CHARM_SOURCE_ROOT}/Modules/io/out/*.cppm"
    )
    charm_collect_cppm(_stream_modules
        "${CHARM_SOURCE_ROOT}/Modules/media/stream/*.cppm"
    )
    charm_collect_cppm(_audio_modules
        "${CHARM_SOURCE_ROOT}/Modules/media/*.cppm"
    )

    list(REMOVE_ITEM _audio_modules
        "${CHARM_SOURCE_ROOT}/Modules/media/audio/audio_source_fs.cppm")
    if (NOT CHARM_ENABLE_SDL3)
        list(REMOVE_ITEM _audio_modules
            "${CHARM_SOURCE_ROOT}/Modules/media/audio/audio_sink_sdl3.cppm")
    endif()

    list(APPEND _modules
        ${_out_modules}
        ${_stream_modules}
        ${_audio_modules}
    )
    list(REMOVE_DUPLICATES _modules)

    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_base_dirs} "${CHARM_SOURCE_ROOT}/Modules" PARENT_SCOPE)
endfunction()
