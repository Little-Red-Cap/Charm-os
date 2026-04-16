cmake_minimum_required(VERSION 4.0)
project(sdl3-wav-demo)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

add_executable(${target_name}
    main.cpp
)

target_sources(${target_name}
    PRIVATE
    FILE_SET modules TYPE CXX_MODULES
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/../../.."
    FILES
        ../../../Modules/media/charm.media.cppm
        ../../../Modules/media/charm.media.audio.cppm
        ../../../Modules/core/util/core.cppm
        ../../../Modules/core/util/expected.cppm
        ../../../Modules/core/service/ring_queue.cppm
        ../../../Modules/core/service/queue.cppm
        ../../../Modules/media/audio/audio_result.cppm
        ../../../Modules/media/audio/audio_format.cppm
        ../../../Modules/media/audio/audio_fifo.cppm
        ../../../Modules/media/audio/audio_eq.cppm
        ../../../Modules/media/audio/audio_frame_queue.cppm
        ../../../Modules/media/audio/audio_dsp_graph.cppm
        ../../../Modules/media/audio/audio_decode_pipe.cppm
        ../../../Modules/media/audio/audio_data_plane.cppm
        ../../../Modules/media/audio/audio_pcm_buffer.cppm
        ../../../Modules/media/audio/audio_pump.cppm
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
if (EXISTS "${CHARM_DR_LIBS_DIR}/dr_mp3.h" AND EXISTS "${CHARM_DR_LIBS_DIR}/dr_flac.h")
    include("${CHARM_ROOT}/cmake/DRLibs.cmake")
    charm_link_dr_libs(${target_name})
endif()

include("${CHARM_ROOT}/cmake/ETL.cmake")
charm_link_etl(${target_name})

include("${CHARM_ROOT}/cmake/DRLibs.cmake")
charm_link_dr_libs(${target_name})
