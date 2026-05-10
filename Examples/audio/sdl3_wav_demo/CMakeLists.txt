cmake_minimum_required(VERSION 4.0)
project(sdl3-wav-demo)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

add_executable(${target_name}
    main.cpp
)

target_compile_definitions(${target_name} PRIVATE
    CHARM_ENABLE_SDL3=1
)

target_sources(${target_name}
    PRIVATE
    FILE_SET modules TYPE CXX_MODULES
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/../../.."
    FILES
        ../../../Modules/media/charm.media.cppm
        ../../../Modules/media/charm.media.audio.cppm
        ../../../Modules/media/stream/stream_types.cppm
        ../../../Modules/media/stream/stream_source.cppm
        ../../../Modules/media/stream/stream_sink.cppm
        ../../../Modules/media/stream/stream_filter.cppm
        ../../../Modules/core/util/core.cppm
        ../../../Modules/core/util/expected.cppm
        ../../../Modules/core/util/error.cppm
        ../../../Modules/core/init/init.node.cppm
        ../../../Modules/core/alg/alg_fft.cppm
        ../../../Modules/core/service/ring_queue.cppm
        ../../../Modules/core/service/queue.cppm
        ../../../Modules/io/channel/io.channel.cppm
        ../../../Modules/io/out/out.core.cppm
        ../../../Modules/io/out/out.domain.cppm
        ../../../Modules/io/out/out.sink.cppm
        ../../../Modules/io/out/out.format.cppm
        ../../../Modules/io/out/out.ansi.cppm
        ../../../Modules/io/out/out.channel.cppm
        ../../../Modules/io/out/out.logger.cppm
        ../../../Modules/io/out/out.api.cppm
        ../../../Modules/init/init.binding.cppm
        ../../../Modules/system/clock/system_clock.cppm
        ../../../Modules/platform/win/time_source.cppm
        ../../../Modules/media/audio/audio_result.cppm
        ../../../Modules/media/audio/audio_format.cppm
        ../../../Modules/media/audio/audio_fifo.cppm
        ../../../Modules/media/audio/audio_eq.cppm
        ../../../Modules/media/audio/audio_frame_queue.cppm
        ../../../Modules/media/audio/audio_frame_writer.cppm
        ../../../Modules/media/audio/audio_dsp_graph.cppm
        ../../../Modules/media/audio/audio_decode_pipe.cppm
        ../../../Modules/media/audio/audio_data_plane.cppm
        ../../../Modules/media/audio/audio_pcm_buffer.cppm
        ../../../Modules/media/audio/audio_pump.cppm
        ../../../Modules/media/audio/audio_pull_sim.cppm
        ../../../Modules/media/audio/audio_source_file.cppm
        ../../../Modules/media/audio/audio_decoder_wav.cppm
        ../../../Modules/media/audio/audio_decoder_flac.cppm
        ../../../Modules/media/audio/audio_decoder_mp3.cppm
        ../../../Modules/media/audio/audio_channel_convert.cppm
        ../../../Modules/media/audio/audio_resampler_linear.cppm
        ../../../Modules/media/audio/audio_sink_common.cppm
        ../../../Modules/media/audio/audio_sink_sdl3.cppm
        ../../../Modules/media/audio/audio_tone.cppm
        ../../../Modules/media/audio/audio_player.cppm
)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
include("${CHARM_ROOT}/cmake/SDL3.cmake")
charm_link_sdl3(${target_name})

set(CHARM_DR_LIBS_DIR "${CHARM_ROOT}/Modules/thirdparty/dr_libs")
include("${CHARM_ROOT}/cmake/DRLibs.cmake")
charm_link_dr_libs(${target_name})
