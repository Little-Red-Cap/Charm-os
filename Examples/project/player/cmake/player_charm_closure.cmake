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
)

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
