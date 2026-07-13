include_guard(GLOBAL)

include("${CHARM_ROOT}/cmake/CharmLeafComponent.cmake")

# Explicit non-audio Charm module closure consumed by canonical Player MD3.
set(PLAYER_CHARM_CLOSURE_MODULES
    "${CHARM_ROOT}/Modules/core/alg/alg_arc.cppm"
    "${CHARM_ROOT}/Modules/core/alg/alg_circle.cppm"
    "${CHARM_ROOT}/Modules/core/alg/alg_list_scroll.cppm"
    "${CHARM_ROOT}/Modules/core/alg/alg_round_rect.cppm"
    "${CHARM_ROOT}/Modules/core/alg/alg_scroll.cppm"
    "${CHARM_ROOT}/Modules/core/alg/alg_scroll_bounds.cppm"
    "${CHARM_ROOT}/Modules/core/alg/alg_scroll_thumb.cppm"
    "${CHARM_ROOT}/Modules/core/alg/alg_text_layout.cppm"
    "${CHARM_ROOT}/Modules/core/alg/alg_virtual_list.cppm"
    "${CHARM_ROOT}/Modules/core/service/fixed_vector.cppm"
    "${CHARM_ROOT}/Modules/core/service/signal.cppm"
    "${CHARM_ROOT}/Modules/core/service/state.cppm"
    "${CHARM_ROOT}/Modules/core/service/service_dirty_rects.cppm"
    "${CHARM_ROOT}/Modules/core/util/delegate.cppm"
    "${CHARM_ROOT}/Modules/gfx/font/font.cppm"
    "${CHARM_ROOT}/Modules/gfx/font/font_noto_ascii_12.cppm"
    "${CHARM_ROOT}/Modules/gfx/font/font_noto_ascii_16.cppm"
    "${CHARM_ROOT}/Modules/gfx/font/font_noto_sc_16.cppm"
    "${CHARM_ROOT}/Modules/gfx/font/font_provider_freetype.cppm"
    "${CHARM_ROOT}/Modules/gfx/font/font_provider_vfs.cppm"
    "${CHARM_ROOT}/Modules/gfx/font/typography.cppm"
    "${CHARM_ROOT}/Modules/io/block/block.device.cppm"
    "${CHARM_ROOT}/Modules/io/block/block.registry.cppm"
    "${CHARM_ROOT}/Modules/io/fs/fs_block.cppm"
    "${CHARM_ROOT}/Modules/io/fs/fs_block_file.cppm"
    "${CHARM_ROOT}/Modules/io/fs/fs_core.cppm"
    "${CHARM_ROOT}/Modules/io/fs/fs_errno.cppm"
    "${CHARM_ROOT}/Modules/io/fs/fs_fatfs.cppm"
    "${CHARM_ROOT}/Modules/io/fs/fs_mal.cppm"
    "${CHARM_ROOT}/Modules/io/fs/fs_mal_block.cppm"
    "${CHARM_ROOT}/Modules/io/fs/fs_mal_file.cppm"
    "${CHARM_ROOT}/Modules/io/fs/fs_path.cppm"
    "${CHARM_ROOT}/Modules/io/fs/fs_ramfs.cppm"
    "${CHARM_ROOT}/Modules/io/fs/fs_stream.cppm"
    "${CHARM_ROOT}/Modules/io/fs/fs_vfs.cppm"
    "${CHARM_ROOT}/Modules/io/hal/input.raw.cppm"
    "${CHARM_ROOT}/Modules/io/input/input.intent.cppm"
    "${CHARM_ROOT}/Modules/io/input/input.nav.cppm"
    "${CHARM_ROOT}/Modules/io/input/input.raw_event.cppm"
    "${CHARM_ROOT}/Modules/io/input/input.raw_sink.cppm"
    "${CHARM_ROOT}/Modules/io/reactor/io.reactor.cppm"
    "${CHARM_ROOT}/Modules/system/loop/system_run_loop.cppm"
    "${CHARM_ROOT}/Modules/ui/common/charm.core.event.cppm"
    "${CHARM_ROOT}/Modules/ui/common/ui.input_adapter.cppm"
    "${CHARM_ROOT}/Modules/ui/common/ui.render_backend.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/anchored_menu.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/config.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/geometry.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/handle.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/input_interaction.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/layer_runtime.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/list_card_header_layout.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/object.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/page_header_layout.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/page_layers.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/path_bar_layout.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/pill_layout.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/pill_surface.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/scene.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/scene_builder_support.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/scene_layer_support.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/seek_bar_style.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/soa_factory.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/soa_gui.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/soa_gui_basic_recorders.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/soa_gui_collection_recorders.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/soa_gui_feedback_recorders.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/soa_gui_style_support.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/soa_kernel.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/soa_layout.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/soa_payload.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/soa_registry.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/string.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/structured_view.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/style.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/style_sheet.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/text_style.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/theme_preset.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/title_block.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/top_bar_layout.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/virtual_list.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/core/widget_registry.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/font/font_package.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/gfx/canvas.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/gfx/color.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/gfx/draw_cmd.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/gfx/framebuffer.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/gfx/image.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/gfx/path.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/gfx/pixel_format.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/gfx/pixel_ops.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/gfx/render_core.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/gfx/render_style.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/gfx/svg.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/gfx/text_box.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/battery_gasgauge.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/busy_wheel.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/button.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/chart.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/cloudy_glass.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/console_box.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/crt_screen.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/dropdown.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/dynamic_nebula.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/foldable_panel.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/histogram.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/histogram_view.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/image.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/image_box.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/label.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/list_view.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/meter_pointer.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/perf_overlay.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/progress.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/progress_bar_drill.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/progress_bar_simple.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/scroll_container.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/scroll_dirty.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/scrollbar.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/segmented_control.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/slider.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/spectrum_view.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/spinning_wheel.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/switch.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/table_view.cppm"
    "${CHARM_ROOT}/Modules/ui/vivid/widgets/tree_view.cppm")

function(player_add_charm_closure target_name)
    charm_add_leaf_component(${target_name}
        LINK_LIBRARIES Charm::audio)
    player_configure_md3_vivid_target(${target_name})
    set_property(TARGET ${target_name} PROPERTY CHARM_VIVID_STACK_USAGE_ROOT
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target_name}.dir")
    set(_player_closure_modules ${PLAYER_CHARM_CLOSURE_MODULES})
    set(_player_closure_base_dirs "${CHARM_ROOT}")
    include("${CHARM_ROOT}/Modules/ui/vivid/vivid.cmake")
    vivid_collect_modules(
        ${target_name} _player_closure_modules _player_closure_base_dirs)
    list(APPEND _player_closure_modules
        "${CHARM_ROOT}/Modules/ui/vivid/gfx/color.cppm"
        "${CHARM_ROOT}/Modules/media/audio/audio_source_fs.cppm")
    list(REMOVE_DUPLICATES _player_closure_modules)
    set(_player_closure_base_dirs "${CHARM_ROOT}")
    charm_attach_leaf_modules(${target_name}
        BASE_DIRS ${_player_closure_base_dirs}
        FILES ${_player_closure_modules})
    if(NOT TARGET Charm::system)
        add_library(Charm-player-system INTERFACE)
        target_link_libraries(Charm-player-system INTERFACE ${target_name})
        add_library(Charm::system ALIAS Charm-player-system)
    endif()
endfunction()
