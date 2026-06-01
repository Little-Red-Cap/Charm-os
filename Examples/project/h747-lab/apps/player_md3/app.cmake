set(H747_LAB_APP_NAME player_md3)
if(NOT DEFINED CHARM_PLAYER_FILE_FONTS)
    set(CHARM_PLAYER_FILE_FONTS OFF CACHE BOOL "Enable Player MD3 file-font backend on H747")
endif()
if(NOT DEFINED CHARM_PLAYER_DEBUG_UI)
    set(CHARM_PLAYER_DEBUG_UI OFF CACHE BOOL "Enable Player MD3 debug UI modules on H747")
endif()
if(NOT DEFINED CHARM_PLAYER_LAYERED_TRANSITIONS)
    set(CHARM_PLAYER_LAYERED_TRANSITIONS OFF CACHE BOOL "Enable Player MD3 layered transitions on H747")
endif()
if(NOT DEFINED CHARM_PLAYER_COVER_THEME_EXTRACT)
    set(CHARM_PLAYER_COVER_THEME_EXTRACT OFF CACHE BOOL "Enable Player MD3 dynamic cover theme extraction on H747")
endif()
if(NOT DEFINED CHARM_PLAYER_LIST_COVER_CACHE_ENTRIES)
    set(CHARM_PLAYER_LIST_COVER_CACHE_ENTRIES 0 CACHE STRING "Player MD3 PRODUCT list cover cache entries on H747")
endif()
include("${H747_LAB_ROOT}/cmake/h747_lab_player_md3_manifest.cmake")
set(H747_LAB_APP_SOURCES
    "${H747_LAB_ROOT}/apps/player_md3/player_md3.cpp"
    "${H747_LAB_ROOT}/apps/player_md3/player_md3_runtime.cpp"
    "${H747_LAB_ROOT}/apps/player_md3/player_md3_console.cpp"
    "${H747_LAB_ROOT}/apps/player_md3/player_md3_input.cpp"
    "${H747_LAB_ROOT}/apps/player_md3/player_md3_memory.cpp"
    "${H747_LAB_ROOT}/apps/player_md3/player_md3_resource_probe.cpp"
    "${H747_LAB_ROOT}/apps/player_md3/player_md3_diag_render.cpp"
    "${H747_LAB_ROOT}/apps/player_md3/player_md3_diag_scene.cpp"
    "${H747_LAB_ROOT}/apps/player_md3/player_md3_diag_smoke.cpp"
    "${H747_LAB_ROOT}/apps/player_md3/player_md3_diag_status.cpp")
set(H747_LAB_APP_INCLUDE_DIRS
    "${H747_LAB_ROOT}/apps/player_md3"
    "${H747_LAB_ROOT}/apps/player"
    "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3")
h747_lab_collect_player_md3_modules(H747_LAB_APP_MODULE_SOURCES H747_LAB_APP_MODULE_BASE_DIRS)
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
    scene_layer_support
    scene_render_detail
    scene_evidence
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
    title_block
    theme_preset
    top_bar_layout
    virtual_list
    widget_registry)
set(CHARM_VIVID_PRODUCT_GFX_MODULES
    canvas
    color
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
if(CHARM_PLAYER_DEBUG_UI)
    list(APPEND CHARM_VIVID_PRODUCT_WIDGETS
        table_view
        tree_view)
endif()
set(CHARM_VIVID_SOA_MAX_NODES 384 CACHE STRING "Player MD3 PRODUCT SoA max nodes" FORCE)
set(CHARM_VIVID_SOA_TEXT_ARENA_BYTES 24576 CACHE STRING "Player MD3 PRODUCT SoA text arena bytes" FORCE)
set(CHARM_VIVID_STYLE_CLASS_MAX 16 CACHE STRING "Player MD3 PRODUCT style class capacity" FORCE)
set(CHARM_VIVID_STYLE_RULE_CAP 8 CACHE STRING "Player MD3 PRODUCT stylesheet rule capacity" FORCE)
set(CHARM_VIVID_STYLE_METRICS_POOL_CAP 16 CACHE STRING "Player MD3 PRODUCT stylesheet metrics pool capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_LABEL 96 CACHE STRING "Player MD3 PRODUCT Label payload capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_BUTTON 56 CACHE STRING "Player MD3 PRODUCT Button payload capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_IMAGE 24 CACHE STRING "Player MD3 PRODUCT Image payload capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_LIST_VIEW 4 CACHE STRING "Player MD3 PRODUCT ListView payload capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_SEGMENTED_CONTROL 4 CACHE STRING "Player MD3 PRODUCT SegmentedControl payload capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_SLIDER 12 CACHE STRING "Player MD3 PRODUCT Slider payload capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_SWITCH 4 CACHE STRING "Player MD3 PRODUCT Switch payload capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_PROGRESS 10 CACHE STRING "Player MD3 PRODUCT Progress payload capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_SCROLLBAR 5 CACHE STRING "Player MD3 PRODUCT ScrollBar payload capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_SCROLL_CONTAINER 5 CACHE STRING "Player MD3 PRODUCT ScrollContainer payload capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_TEXT_LIST 4 CACHE STRING "Player MD3 PRODUCT TextList payload capacity" FORCE)
set(CHARM_VIVID_PAYLOAD_CAP_SPINNER 4 CACHE STRING "Player MD3 PRODUCT Spinner payload capacity" FORCE)
if(CHARM_PLAYER_DEBUG_UI)
    set(CHARM_VIVID_PAYLOAD_CAP_TABLE_VIEW 4 CACHE STRING "Player MD3 debug TableView payload capacity" FORCE)
    set(CHARM_VIVID_PAYLOAD_CAP_TREE_VIEW 4 CACHE STRING "Player MD3 debug TreeView payload capacity" FORCE)
else()
    set(CHARM_VIVID_PAYLOAD_CAP_TABLE_VIEW 0 CACHE STRING "Player MD3 default TableView payload capacity" FORCE)
    set(CHARM_VIVID_PAYLOAD_CAP_TREE_VIEW 0 CACHE STRING "Player MD3 default TreeView payload capacity" FORCE)
endif()
set(H747_LAB_APP_COMPILE_DEFINITIONS
    CHARM_PLAYER_HOST_UI=0
    CHARM_PLAYER_HOST_STORAGE=0
    CHARM_PLAYER_HOST_COVER_DECODE=0
    CHARM_PLAYER_HOST_FILE_FONTS=0
    CHARM_PLAYER_ENABLE_FATFS_STORAGE=1
    CHARM_PLAYER_COVER_DECODE=0
    CHARM_PLAYER_FILE_FONTS=$<BOOL:${CHARM_PLAYER_FILE_FONTS}>
    CHARM_PLAYER_MCU=1
    CHARM_PLAYER_MCU_STRICT=1
    CHARM_PLAYER_DEBUG_UI=$<BOOL:${CHARM_PLAYER_DEBUG_UI}>
    CHARM_PLAYER_LAYERED_TRANSITIONS=$<BOOL:${CHARM_PLAYER_LAYERED_TRANSITIONS}>
    CHARM_PLAYER_COVER_THEME_EXTRACT=$<BOOL:${CHARM_PLAYER_COVER_THEME_EXTRACT}>
    CHARM_PLAYER_LIST_COVER_CACHE_ENTRIES=${CHARM_PLAYER_LIST_COVER_CACHE_ENTRIES}
    CHARM_VIVID_UNSUPPORTED_WIDGET_DIAG=1
    CHARM_VIVID_MEMORY_PROFILE_SYMBOLS=1
    CHARM_ENABLE_UI_VIVID=1
    CHARM_AUDIO_USE_VFS=1
    CHARM_AUDIO_SINK_I2S=1
    CHARM_AUDIO_ENABLE_MP3=1
    CHARM_AUDIO_ENABLE_FLAC=1
    CHARM_PLAYER_RESOURCE_FONT_SMALL_PX=14
    CHARM_PLAYER_RESOURCE_FONT_NORMAL_PX=18
    CHARM_PLAYER_RESOURCE_FONT_LARGE_PX=76)
set(H747_LAB_VIVID_FEATURESET PRODUCT)
