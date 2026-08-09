# Product roots are existing public Vivid surfaces. Unlisted modules are internal.
foreach(_module IN ITEMS
        charm.ui.vivid
        charm.ui.scene
        charm.ui.scene.anchored_menu
        charm.ui.scene.focus_scope
        charm.ui.scene.list_card_header
        charm.ui.scene.motion_runtime
        charm.ui.scene.page_header
        charm.ui.scene.page_layers
        charm.ui.scene.page_transition
        charm.ui.scene.path_bar
        charm.ui.scene.pill
        charm.ui.scene.pill_surface
        charm.ui.scene.scene_evidence
        charm.ui.scene.seek_bar_style
        charm.ui.scene.text_style
        charm.ui.scene.title_block
        charm.ui.scene.top_bar
        charm.gfx.canvas
        charm.gfx.color
        charm.gfx.display_policy
        charm.gfx.framebuffer
        charm.gfx.image
        charm.gfx.path
        charm.gfx.pixel_format
        charm.gfx.render_style
        charm.gfx.svg
        charm.gfx.text_box
        charm.ui.vivid.font_package
        charm.ui.vivid.font_runtime
        charm.ui.vivid.perf_overlay_runtime)
    vivid_module_policy(NAME "${_module}" ACCESS PRODUCT_ROOT)
endforeach()

foreach(_module IN ITEMS
        charm.gfx.host_tools
        charm.gfx.snapshot)
    vivid_module_policy(NAME "${_module}" ACCESS HOST_ONLY)
endforeach()

foreach(_module IN ITEMS
        charm.ui.vivid_internal
        charm.core.soa_factory
        charm.core.soa_gui
        charm.core.soa_payload
        charm.core.soa_kernel
        charm.gfx.draw_cmd
        charm.ui.scene.builder_support
        charm.ui.scene.layer_support
        charm.ui.scene:snapshot_store
        charm.ui.scene:render_detail)
    vivid_module_policy(NAME "${_module}" ACCESS INTERNAL)
endforeach()
