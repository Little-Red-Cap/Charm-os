include_guard(GLOBAL)

# Player owns its Vivid dependency closure and capacities. Platform adapters
# consume this product profile; boards must not redefine the application model.
set(CHARM_VIVID_FEATURESET PRODUCT CACHE STRING "Player MD3 Vivid featureset" FORCE)

set(CHARM_VIVID_PRODUCT_CORE_MODULES
    anchored_menu
    config
    focus_scope
    geometry
    handle
    input_interaction
    layer_runtime
    list_card_header_layout
    object
    page_header_layout
    page_layers
    path_bar_layout
    pill_layout
    pill_surface
    scene
    scene_builder_support
    scene_evidence
    scene_layer_support
    scene_render_detail
    seek_bar_style
    soa_factory
    soa_gui
    soa_gui_basic_recorders
    soa_gui_collection_recorders
    soa_gui_feedback_recorders
    soa_gui_style_support
    soa_kernel
    soa_kernel_actions
    soa_kernel_behavior
    soa_kernel_class
    soa_kernel_input
    soa_kernel_input_core
    soa_kernel_layout_state
    soa_kernel_payload
    soa_kernel_payload_lists
    soa_kernel_payload_views
    soa_kernel_semantic
    soa_kernel_storage
    soa_kernel_types
    soa_layout
    soa_payload
    soa_registry
    soa_router
    string
    structured_view
    style
    style_evidence
    style_impact
    style_sheet
    text_style
    theme_preset
    title_block
    top_bar_layout
    virtual_list
    widget_registry)

set(CHARM_VIVID_PRODUCT_GFX_MODULES
    canvas
    draw_cmd
    draw_cmd_buffer
    draw_cmd_evidence
    draw_cmd_executor
    draw_cmd_schema
    framebuffer
    framebuffer_core
    image
    path
    pixel_format
    pixel_ops
    render_core
    render_style
    svg
    text_box)

set(CHARM_VIVID_PRODUCT_WIDGETS
    button
    label
    image
    image_box
    list_view
    scrollbar
    scroll_container
    scroll_dirty
    progress
    battery_gasgauge
    progress_bar_simple
    progress_bar_drill
    segmented_control
    slider
    switcher
    dropdown
    perf_overlay
    busy_wheel
    chart
    cloudy_glass
    console_box
    crt_screen
    dynamic_nebula
    foldable_panel
    histogram
    histogram_view
    meter_pointer
    spectrum_view
    spinning_wheel)

if (CHARM_PLAYER_DEBUG_UI)
    list(APPEND CHARM_VIVID_PRODUCT_WIDGETS table_view tree_view)
endif()

set(CHARM_VIVID_SOA_MAX_NODES 384 CACHE STRING "Player MD3 SoA max nodes" FORCE)
set(CHARM_VIVID_SOA_TEXT_ARENA_BYTES 24576 CACHE STRING "Player MD3 text arena bytes" FORCE)
set(CHARM_VIVID_STYLE_CLASS_MAX 16 CACHE STRING "Player MD3 style class capacity" FORCE)
set(CHARM_VIVID_STYLE_RULE_CAP 8 CACHE STRING "Player MD3 stylesheet rule capacity" FORCE)
set(CHARM_VIVID_STYLE_METRICS_POOL_CAP 16 CACHE STRING "Player MD3 style metrics capacity" FORCE)
set(CHARM_VIVID_RUNTIME_SCENE_INSTANCES 1 CACHE STRING "Player MD3 resident Scene count" FORCE)
set(CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES 6291456 CACHE STRING "Player MD3 Vivid resident RAM budget" FORCE)
set(CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES 524288 CACHE STRING "Player MD3 Vivid resident RAM minimum headroom" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_LABEL 96 CACHE STRING "Player MD3 Label capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_BUTTON 56 CACHE STRING "Player MD3 Button capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_IMAGE 24 CACHE STRING "Player MD3 Image capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_LIST_VIEW 4 CACHE STRING "Player MD3 ListView capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_SEGMENTED_CONTROL 4 CACHE STRING "Player MD3 SegmentedControl capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_SLIDER 12 CACHE STRING "Player MD3 Slider capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_SWITCH 4 CACHE STRING "Player MD3 Switch capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_PROGRESS 10 CACHE STRING "Player MD3 Progress capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_SCROLLBAR 5 CACHE STRING "Player MD3 ScrollBar capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_SCROLL_CONTAINER 5 CACHE STRING "Player MD3 ScrollContainer capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_TEXT_LIST 4 CACHE STRING "Player MD3 TextList capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_SPINNER 4 CACHE STRING "Player MD3 Spinner capacity" FORCE)

if (CHARM_PLAYER_DEBUG_UI)
    set(CHARM_VIVID_PAYLOAD_CAP_TABLE_VIEW 4 CACHE STRING "Player MD3 debug TableView capacity" FORCE)
    set(CHARM_VIVID_PAYLOAD_CAP_TREE_VIEW 4 CACHE STRING "Player MD3 debug TreeView capacity" FORCE)
else()
    set(CHARM_VIVID_PAYLOAD_CAP_TABLE_VIEW 0 CACHE STRING "Player MD3 TableView capacity" FORCE)
    set(CHARM_VIVID_PAYLOAD_CAP_TREE_VIEW 0 CACHE STRING "Player MD3 TreeView capacity" FORCE)
endif()
