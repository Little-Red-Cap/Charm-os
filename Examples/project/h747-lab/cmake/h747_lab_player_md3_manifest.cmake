include_guard(GLOBAL)

function(h747_lab_collect_player_md3_modules out_modules out_base_dirs)
    set(_modules
        "${CHARM_ROOT}/Modules/core/alg/alg_fft.cppm"
        "${CHARM_ROOT}/Modules/core/service/queue.cppm"
        "${CHARM_ROOT}/Modules/core/service/ring_queue.cppm"
        "${CHARM_ROOT}/Modules/media/stream/stream_types.cppm"
        "${CHARM_ROOT}/Modules/media/stream/stream_source.cppm"
        "${CHARM_ROOT}/Modules/media/stream/stream_sink.cppm"
        "${CHARM_ROOT}/Modules/media/stream/stream_filter.cppm"
        "${CHARM_ROOT}/Modules/media/stream/stream_pipeline.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_result.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_format.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_data_plane.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_fifo.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_frame_queue.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_pcm_buffer.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_frame_writer.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_channel_convert.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_resampler_linear.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_sink_common.cppm"
        "${CHARM_ROOT}/Examples/project/player/stn32common/audio_sink_i2s.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_spectrum.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_source_fs.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_decoder_wav.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_decoder_mp3.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_decoder_flac.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_decode_pipe.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_dsp_graph.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_eq.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_pump.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_player.cppm"
        "${CHARM_ROOT}/Modules/ui/vivid/widgets/battery_gasgauge.cppm"
        "${CHARM_ROOT}/Modules/io/out/out.ansi.cppm"
        "${CHARM_ROOT}/Modules/io/out/out.api.cppm"
        "${CHARM_ROOT}/Modules/io/out/out.channel.cppm"
        "${CHARM_ROOT}/Modules/io/out/out.domain.cppm"
        "${CHARM_ROOT}/Modules/io/out/out.logger.cppm"
        "${CHARM_ROOT}/Modules/io/fs/fs_block_file.cppm"
        "${CHARM_ROOT}/Modules/io/fs/fs_mal_block.cppm"
        "${CHARM_ROOT}/Modules/io/fs/fs_mal_file.cppm"
        "${CHARM_ROOT}/Modules/io/fs/fs_mal.cppm"
        "${CHARM_ROOT}/Modules/io/fs/fs_fatfs.cppm"
        "${CHARM_ROOT}/Modules/io/hal/input.raw.cppm"
        "${CHARM_ROOT}/Modules/io/input/input.intent.cppm"
        "${CHARM_ROOT}/Modules/io/input/input.nav.cppm"
        "${CHARM_ROOT}/Modules/io/input/input.raw_event.cppm"
        "${CHARM_ROOT}/Modules/ui/common/ui.input_adapter.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.app_config.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.app.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.cover_resource.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.display.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.fixed_string.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.font_cache.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.font_resource.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.font_resource_apply.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.fs_utils.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.host_features.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.input.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.mcu_policy.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.media_scan.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.platform.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.playback.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.product_config.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.runtime.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.runtime_shell.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.stats_history.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.storage.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.time_utils.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.track_probe.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.controller.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.cover.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.cover_theme.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.ui.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.ui_builder.cppm")

    if(CHARM_PLAYER_FILE_FONTS)
        list(APPEND _modules
            "${CHARM_ROOT}/Modules/gfx/font/font_provider_vfs.cppm"
            "${CHARM_ROOT}/Modules/gfx/font/font_provider_freetype.cppm"
            "${CHARM_ROOT}/Modules/ui/vivid/font/font_package.cppm")
    endif()

    if(CHARM_PLAYER_DEBUG_UI)
        list(APPEND _modules
            "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.ui_debug.cppm")
    endif()

    set(_base_dirs
        "${CHARM_ROOT}/Examples/project/player/app-common"
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3"
        "${CHARM_ROOT}/Examples/project/player/stn32common")

    list(REMOVE_DUPLICATES _modules)
    list(REMOVE_DUPLICATES _base_dirs)
    set(${out_modules} "${_modules}" PARENT_SCOPE)
    set(${out_base_dirs} "${_base_dirs}" PARENT_SCOPE)
endfunction()
