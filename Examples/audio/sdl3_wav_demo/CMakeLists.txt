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
        ../../../Modules/media/audio/audio_result.cppm
        ../../../Modules/media/audio/audio_format.cppm
        ../../../Modules/media/audio/audio_fifo.cppm
        ../../../Modules/media/audio/audio_source_file.cppm
        ../../../Modules/media/audio/audio_decoder_wav.cppm
        ../../../Modules/media/audio/audio_decoder_flac.cppm
        ../../../Modules/media/audio/audio_decoder_mp3.cppm
        ../../../Modules/media/audio/audio_channel_convert.cppm
        ../../../Modules/media/audio/audio_resampler_linear.cppm
        ../../../Modules/media/audio/audio_sink_sdl3.cppm
        ../../../Modules/media/audio/audio_player.cppm
)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
include("${CHARM_ROOT}/cmake/SDL3.cmake")
charm_link_sdl3(${target_name})
