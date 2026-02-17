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
        "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    FILES
        ../../Modules/audio_result.cppm
        ../../Modules/audio_format.cppm
        ../../Modules/audio_fifo.cppm
        ../../Modules/audio_source_file.cppm
        ../../Modules/audio_decoder_wav.cppm
        ../../Modules/audio_decoder_flac.cppm
        ../../Modules/audio_decoder_mp3.cppm
        ../../Modules/audio_channel_convert.cppm
        ../../Modules/audio_resampler_linear.cppm
        ../../Modules/audio_sink_sdl3.cppm
        ../../Modules/audio_player.cppm
)

find_package(SDL3 CONFIG REQUIRED)

target_link_libraries(${target_name}
    PRIVATE
        SDL3::SDL3
)
