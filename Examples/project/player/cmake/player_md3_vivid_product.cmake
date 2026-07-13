include_guard(GLOBAL)

include("${CHARM_ROOT}/Modules/ui/vivid/cmake/product_profile_compiler.cmake")

set(CHARM_VIVID_FEATURESET PRODUCT CACHE STRING "Player MD3 Vivid featureset" FORCE)

set(_player_md3_vivid_roots
    charm.ui.vivid
    charm.gfx.svg
    charm.ui.scene.anchored_menu
    charm.ui.scene.list_card_header
    charm.ui.scene.page_header
    charm.ui.scene.page_layers
    charm.ui.scene.path_bar
    charm.ui.scene.pill
    charm.ui.scene.pill_surface
    charm.ui.scene.seek_bar_style
    charm.ui.scene.text_style
    charm.ui.scene.title_block
    charm.ui.scene.top_bar
    charm.ui.vivid.font_package)

vivid_define_product_profile(
    NAME player_md3
    ROOT_MODULES ${_player_md3_vivid_roots}
    WIDGET_KINDS
        Container
        Button
        IconButton
        Label
        Image
        ImageBox
        ListView
        ScrollBar
        ScrollContainer
        Progress
        BatteryGasGauge
        ProgressBarSimple
        ProgressBarDrill
        SegmentedControl
        Slider
        Switch
        Dropdown
        PerfOverlay
        BusyWheel
        Chart
        CloudyGlass
        ConsoleBox
        CrtScreen
        DynamicNebula
        FoldablePanel
        Histogram
        HistogramView
        MeterPointer
        SpectrumView
        SpinningWheel
    PAYLOAD_CAPACITIES
        Label=96
        Button=56
        Image=24
        ListView=4
        SegmentedControl=4
        Slider=12
        Switch=4
        Progress=10
        ScrollBar=5
        ScrollContainer=5
        TextList=4
        Spinner=4
    SOA_MAX_NODES 384
    SOA_TEXT_ARENA_BYTES 24576
    STYLE_CLASS_MAX 16
    STYLE_RULE_CAP 8
    STYLE_METRICS_POOL_CAP 16
    DRAW_CMD_MAX_COMMANDS 1024
    DRAW_CMD_TEXT_BYTES 4096
    DRAW_CMD_BLOB_BYTES 2048
    FLOAT_WIDGETS ON)

vivid_define_product_profile(
    NAME player_md3_debug
    EXTENDS player_md3
    ROOT_MODULES ${_player_md3_vivid_roots}
    WIDGET_KINDS TableView TreeView
    PAYLOAD_CAPACITIES TableView=4 TreeView=4)

if(CHARM_PLAYER_DEBUG_UI)
    set(CHARM_PLAYER_VIVID_PROFILE player_md3_debug)
else()
    set(CHARM_PLAYER_VIVID_PROFILE player_md3)
endif()
