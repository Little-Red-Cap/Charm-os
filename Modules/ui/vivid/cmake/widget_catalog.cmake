# Named source of truth for Vivid widget and payload metadata.

vivid_catalog_payload_pool(
    NAME Label
    MEMBER label_
    STATS_FIELD label
    CAP_KIND Label)
vivid_catalog_payload_pool(
    NAME Button
    MEMBER button_
    STATS_FIELD button
    CAP_KIND Button)
vivid_catalog_payload_pool(
    NAME Image
    MEMBER image_
    STATS_FIELD image
    CAP_KIND Image)
vivid_catalog_payload_pool(
    NAME TextInput
    MEMBER text_input_
    STATS_FIELD text_input
    CAP_KIND TextInput)
vivid_catalog_payload_pool(
    NAME TextArea
    MEMBER text_area_
    STATS_FIELD text_area
    CAP_KIND TextArea)
vivid_catalog_payload_pool(
    NAME NumberInput
    MEMBER number_input_
    STATS_FIELD number_input
    CAP_KIND NumberInput)
vivid_catalog_payload_pool(
    NAME SegmentedControl
    MEMBER segmented_
    STATS_FIELD segmented
    CAP_KIND SegmentedControl)
vivid_catalog_payload_pool(
    NAME Stepper
    MEMBER stepper_
    STATS_FIELD stepper
    CAP_KIND Stepper)
vivid_catalog_payload_pool(
    NAME ToggleGroup
    MEMBER toggle_group_
    STATS_FIELD toggle_group
    CAP_KIND ToggleGroup)
vivid_catalog_payload_pool(
    NAME Checkbox
    MEMBER checkbox_
    STATS_FIELD checkbox
    CAP_KIND Checkbox)
vivid_catalog_payload_pool(
    NAME Radio
    MEMBER radio_
    STATS_FIELD radio
    CAP_KIND Radio)
vivid_catalog_payload_pool(
    NAME ListItem
    MEMBER list_item_
    STATS_FIELD list_item
    CAP_KIND ListItem)
vivid_catalog_payload_pool(
    NAME TextList
    MEMBER text_list_
    STATS_FIELD text_list
    CAP_KIND TextList)
vivid_catalog_payload_pool(
    NAME ListView
    MEMBER list_view_
    STATS_FIELD list_view
    CAP_KIND ListView)
vivid_catalog_payload_pool(
    NAME TableView
    MEMBER table_view_
    STATS_FIELD table_view
    CAP_KIND TableView)
vivid_catalog_payload_pool(
    NAME TreeView
    MEMBER tree_view_
    STATS_FIELD tree_view
    CAP_KIND TreeView)
vivid_catalog_payload_pool(
    NAME NumberList
    MEMBER number_list_
    STATS_FIELD number_list
    CAP_KIND NumberList)
vivid_catalog_payload_pool(
    NAME Roller
    MEMBER roller_
    STATS_FIELD roller
    CAP_KIND Roller)
vivid_catalog_payload_pool(
    NAME Switch
    MEMBER switch_
    STATS_FIELD switcher
    CAP_KIND Switch)
vivid_catalog_payload_pool(
    NAME Slider
    MEMBER slider_
    STATS_FIELD slider
    CAP_KIND Slider)
vivid_catalog_payload_pool(
    NAME ScrollBar
    MEMBER scrollbar_
    STATS_FIELD scrollbar
    CAP_KIND ScrollBar)
vivid_catalog_payload_pool(
    NAME Progress
    MEMBER progress_
    STATS_FIELD progress
    CAP_KIND Progress)
vivid_catalog_payload_pool(
    NAME List
    MEMBER list_
    STATS_FIELD list
    CAP_KIND List)
vivid_catalog_payload_pool(
    NAME ScrollContainer
    MEMBER scroll_container_
    STATS_FIELD scroll_container
    CAP_KIND ScrollContainer)
vivid_catalog_payload_pool(
    NAME Spinner
    MEMBER spinner_
    STATS_FIELD spinner
    CAP_KIND Spinner)

vivid_catalog_widget(
    ID 1
    KIND Container
    SCENE_SUPPORT Supported
    RUNTIME_ONLY
    CPP_TYPE Container
    THEME_BASE None
    FACTORY container
    FACTORY_POOL containers_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 2
    KIND ScrollContainer
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.scroll_container
    CPP_TYPE ScrollContainer
    THEME_BASE ScrollContainer
    FACTORY scroll_container
    FACTORY_POOL scrolls_
    FACTORY_CREATE None
    PAYLOAD_POOL ScrollContainer
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET SelfIfScrollableElseAncestor
    DRAG_BEHAVIOR ScrollDrag
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    SCROLL_ENABLED
    EXTRA_ENABLED
)
vivid_catalog_widget(
    ID 3
    KIND Dial
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.dial
    CPP_TYPE Dial
    THEME_BASE Dial
    FACTORY dial
    FACTORY_POOL dials_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 4
    KIND Arc
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.arc
    CPP_TYPE Arc
    THEME_BASE Arc
    FACTORY arc
    FACTORY_POOL arcs_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 5
    KIND Image
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.image
    CPP_TYPE Image
    THEME_BASE Image
    FACTORY image
    FACTORY_POOL images_
    FACTORY_CREATE None
    PAYLOAD_POOL Image
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 6
    KIND Label
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.label
    CPP_TYPE Label
    THEME_BASE Label
    FACTORY label
    FACTORY_POOL labels_
    FACTORY_CREATE Text
    PAYLOAD_POOL Label
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 7
    KIND Button
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.button
    CPP_TYPE Button
    THEME_BASE Button
    FACTORY button
    FACTORY_POOL buttons_
    FACTORY_CREATE Text
    PAYLOAD_POOL Button
    STYLE Interactive
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 8
    KIND IconButton
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.button
    CPP_TYPE Button
    THEME_BASE IconButton
    FACTORY icon_button
    FACTORY_POOL buttons_
    FACTORY_CREATE None
    PAYLOAD_POOL Button
    STYLE Interactive
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 9
    KIND Checkbox
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.checkbox
    CPP_TYPE Checkbox
    THEME_BASE Checkbox
    FACTORY checkbox
    FACTORY_POOL checkboxes_
    FACTORY_CREATE Text
    PAYLOAD_POOL Checkbox
    STYLE Readonly
    CLICK Toggle
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    FOCUSABLE
    CLICK_ENABLED
)
vivid_catalog_widget(
    ID 10
    KIND Led
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.led
    CPP_TYPE Led
    THEME_BASE Led
    FACTORY led
    FACTORY_POOL leds_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 11
    KIND Slider
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.slider
    CPP_TYPE Slider
    THEME_BASE Slider
    FACTORY slider
    FACTORY_POOL sliders_
    FACTORY_CREATE None
    PAYLOAD_POOL Slider
    STYLE PressOnly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY UpdateValueFromPos
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    DRAG_ENABLED
)
vivid_catalog_widget(
    ID 12
    KIND Switch
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.switcher
    CPP_TYPE Switch
    THEME_BASE Switch
    FACTORY switch
    FACTORY_POOL switches_
    FACTORY_CREATE None
    PAYLOAD_POOL Switch
    STYLE Readonly
    CLICK Toggle
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    CLICK_ENABLED
)
vivid_catalog_widget(
    ID 13
    KIND Progress
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.progress
    CPP_TYPE Progress
    THEME_BASE Progress
    FACTORY progress
    FACTORY_POOL progresses_
    FACTORY_CREATE None
    PAYLOAD_POOL Progress
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 14
    KIND List
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.list
    CPP_TYPE List
    THEME_BASE List
    FACTORY list
    FACTORY_POOL lists_
    FACTORY_CREATE None
    PAYLOAD_POOL List
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET SelfIfScrollableElseAncestor
    DRAG_BEHAVIOR ScrollDrag
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    SCROLL_ENABLED
    EXTRA_ENABLED
)
vivid_catalog_widget(
    ID 15
    KIND ListItem
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.list
    CPP_TYPE ListItem
    THEME_BASE ListItem
    FACTORY list_item
    FACTORY_POOL list_items_
    FACTORY_CREATE Text
    PAYLOAD_POOL ListItem
    STYLE Readonly
    CLICK ListItemGroup
    CLICK_INDEX None
    GROUP_KIND ListItem
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    CLICK_ENABLED
)
vivid_catalog_widget(
    ID 16
    KIND ListView
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.list_view
    CPP_TYPE ListView
    THEME_BASE ListView
    FACTORY list_view
    FACTORY_POOL list_views_
    FACTORY_CREATE None
    PAYLOAD_POOL ListView
    STYLE Readonly
    CLICK ListView
    CLICK_INDEX ListViewY
    GROUP_KIND None
    WHEEL_TARGET SelfIfScrollableElseAncestor
    DRAG_BEHAVIOR ScrollDrag
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    CLICK_ENABLED
    SCROLL_ENABLED
    EXTRA_ENABLED
)
vivid_catalog_widget(
    ID 17
    KIND IconList
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.icon_list
    CPP_TYPE IconList
    THEME_BASE IconList
    FACTORY icon_list
    FACTORY_POOL icon_lists_
    FACTORY_CREATE None
    PAYLOAD_POOL ListView
    STYLE Readonly
    CLICK ListView
    CLICK_INDEX ListViewY
    GROUP_KIND None
    WHEEL_TARGET SelfIfScrollableElseAncestor
    DRAG_BEHAVIOR ScrollDrag
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    CLICK_ENABLED
    SCROLL_ENABLED
    EXTRA_ENABLED
)
vivid_catalog_widget(
    ID 18
    KIND TextTrackingList
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.text_tracking_list
    CPP_TYPE TextTrackingList
    THEME_BASE TextTrackingList
    FACTORY text_tracking_list
    FACTORY_POOL text_tracking_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 19
    KIND TextList
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.text_list
    CPP_TYPE TextList
    THEME_BASE TextList
    FACTORY text_list
    FACTORY_POOL text_lists_
    FACTORY_CREATE None
    PAYLOAD_POOL TextList
    STYLE Readonly
    CLICK TextList
    CLICK_INDEX TextListY
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    CLICK_ENABLED
)
vivid_catalog_widget(
    ID 20
    KIND ModalDialog
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.modal_dialog
    CPP_TYPE ModalDialog
    THEME_BASE ModalDialog
    FACTORY modal_dialog
    FACTORY_POOL modals_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 21
    KIND ProgressBarSimple
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.progress_bar_simple
    CPP_TYPE ProgressBarSimple
    THEME_BASE ProgressBarSimple
    FACTORY progress_bar_simple
    FACTORY_POOL progress_simple_
    FACTORY_CREATE None
    PAYLOAD_POOL Progress
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 22
    KIND DynamicNebula
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.dynamic_nebula
    CPP_TYPE DynamicNebula
    THEME_BASE DynamicNebula
    FACTORY dynamic_nebula
    FACTORY_POOL nebula_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 23
    KIND CrtScreen
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.crt_screen
    CPP_TYPE CrtScreen
    THEME_BASE CrtScreen
    FACTORY crt_screen
    FACTORY_POOL crt_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 24
    KIND ScrollBar
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.scrollbar
    CPP_TYPE ScrollBar
    THEME_BASE ScrollBar
    FACTORY scroll_bar
    FACTORY_POOL scroll_bars_
    FACTORY_CREATE None
    PAYLOAD_POOL ScrollBar
    STYLE PressOnly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY ScrollBarTrack
    WHEEL_TARGET_ONLY BoundTarget
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    DRAG_ENABLED
    WHEEL_ENABLED
)
vivid_catalog_widget(
    ID 25
    KIND SegmentedControl
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.segmented_control
    CPP_TYPE SegmentedControl
    THEME_BASE SegmentedControl
    FACTORY segmented_control
    FACTORY_POOL segments_
    FACTORY_CREATE None
    PAYLOAD_POOL SegmentedControl
    STYLE Readonly
    CLICK SegmentedControl
    CLICK_INDEX SegmentedX
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    FOCUSABLE
    CLICK_ENABLED
)
vivid_catalog_widget(
    ID 26
    KIND TextArea
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.text_area
    CPP_TYPE TextArea
    THEME_BASE TextArea
    FACTORY text_area
    FACTORY_POOL text_areas_
    FACTORY_CREATE Text
    PAYLOAD_POOL TextArea
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 27
    KIND TextInput
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.text_input
    CPP_TYPE TextInput
    THEME_BASE TextInput
    FACTORY text_input
    FACTORY_POOL text_inputs_
    FACTORY_CREATE None
    PAYLOAD_POOL TextInput
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 28
    KIND NumberInput
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.number_input
    CPP_TYPE NumberInput
    THEME_BASE NumberInput
    FACTORY number_input
    FACTORY_POOL number_inputs_
    FACTORY_CREATE None
    PAYLOAD_POOL NumberInput
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 29
    KIND ToggleGroup
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.toggle_group
    CPP_TYPE ToggleGroup
    THEME_BASE ToggleGroup
    FACTORY toggle_group
    FACTORY_POOL toggles_
    FACTORY_CREATE None
    PAYLOAD_POOL ToggleGroup
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 30
    KIND TableView
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.table_view
    CPP_TYPE TableView
    THEME_BASE TableView
    FACTORY table_view
    FACTORY_POOL tables_
    FACTORY_CREATE None
    PAYLOAD_POOL TableView
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET SelfIfScrollableElseAncestor
    DRAG_BEHAVIOR ScrollDrag
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Both
    WHEEL_AXIS PreferVertical
    SCROLL_ENABLED
    EXTRA_ENABLED
)
vivid_catalog_widget(
    ID 31
    KIND TreeView
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.tree_view
    CPP_TYPE TreeView
    THEME_BASE TreeView
    FACTORY tree_view
    FACTORY_POOL trees_
    FACTORY_CREATE None
    PAYLOAD_POOL TreeView
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET SelfIfScrollableElseAncestor
    DRAG_BEHAVIOR ScrollDrag
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    SCROLL_ENABLED
    EXTRA_ENABLED
)
vivid_catalog_widget(
    ID 32
    KIND Dropdown
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.dropdown
    CPP_TYPE Dropdown
    THEME_BASE Dropdown
    FACTORY dropdown
    FACTORY_POOL dropdowns_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 33
    KIND TabView
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.tabview
    CPP_TYPE TabView
    THEME_BASE TabView
    FACTORY tabview
    FACTORY_POOL tabviews_
    FACTORY_CREATE None
    PAYLOAD_POOL SegmentedControl
    STYLE Readonly
    CLICK SegmentedControl
    CLICK_INDEX SegmentedX
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    FOCUSABLE
    CLICK_ENABLED
)
vivid_catalog_widget(
    ID 34
    KIND Roller
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.roller
    CPP_TYPE Roller
    THEME_BASE Roller
    FACTORY roller
    FACTORY_POOL rollers_
    FACTORY_CREATE None
    PAYLOAD_POOL Roller
    STYLE PressOnly
    CLICK Roller
    CLICK_INDEX RollerY
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    CLICK_ENABLED
)
vivid_catalog_widget(
    ID 35
    KIND Spinner
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.spinner
    CPP_TYPE Spinner
    THEME_BASE Spinner
    FACTORY spinner
    FACTORY_POOL spinners_
    FACTORY_CREATE None
    PAYLOAD_POOL Spinner
    STYLE PressOnly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 36
    KIND Bar
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.bar
    CPP_TYPE Bar
    THEME_BASE Bar
    FACTORY bar
    FACTORY_POOL bars_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 37
    KIND PopupLayer
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.popup_layer
    CPP_TYPE PopupLayer
    THEME_BASE PopupLayer
    FACTORY popup_layer
    FACTORY_POOL popups_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 38
    KIND MessageBox
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.message_box
    CPP_TYPE MessageBox
    THEME_BASE MessageBox
    FACTORY message_box
    FACTORY_POOL message_boxes_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 39
    KIND Menu
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.menu
    CPP_TYPE Menu
    THEME_BASE Menu
    FACTORY menu
    FACTORY_POOL menus_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 40
    KIND MenuItem
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.menu_item
    CPP_TYPE MenuItem
    THEME_BASE MenuItem
    FACTORY menu_item
    FACTORY_POOL menu_items_
    FACTORY_CREATE Text
    PAYLOAD_POOL ListItem
    STYLE Readonly
    CLICK ListItemGroup
    CLICK_INDEX None
    GROUP_KIND MenuItem
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    CLICK_ENABLED
)
vivid_catalog_widget(
    ID 41
    KIND Radio
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.radio
    CPP_TYPE Radio
    THEME_BASE Radio
    FACTORY radio
    FACTORY_POOL radios_
    FACTORY_CREATE Text
    PAYLOAD_POOL Radio
    STYLE Readonly
    CLICK RadioGroup
    CLICK_INDEX None
    GROUP_KIND Radio
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    FOCUSABLE
    CLICK_ENABLED
)
vivid_catalog_widget(
    ID 42
    KIND RadioGroup
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.radio_group
    CPP_TYPE RadioGroup
    THEME_BASE RadioGroup
    FACTORY radio_group
    FACTORY_POOL radio_groups_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 43
    KIND Chart
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.chart
    CPP_TYPE Chart
    THEME_BASE Chart
    FACTORY chart
    FACTORY_POOL charts_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 44
    KIND Waveform
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.waveform
    CPP_TYPE Waveform
    THEME_BASE Waveform
    FACTORY waveform
    FACTORY_POOL waveforms_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 45
    KIND Gauge
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.gauge
    CPP_TYPE Gauge
    THEME_BASE Gauge
    FACTORY gauge
    FACTORY_POOL gauges_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 46
    KIND PrimitivesCanvas
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.primitives_canvas
    CPP_TYPE PrimitivesCanvas
    THEME_BASE PrimitivesCanvas
    FACTORY primitives_canvas
    FACTORY_POOL prim_canvas_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 47
    KIND PerfOverlay
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.perf_overlay
    CPP_TYPE PerfOverlay
    THEME_BASE PerfOverlay
    FACTORY perf_overlay
    FACTORY_POOL perf_overlays_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 48
    KIND Stepper
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.stepper
    CPP_TYPE Stepper
    THEME_BASE Stepper
    FACTORY stepper
    FACTORY_POOL steppers_
    FACTORY_CREATE None
    PAYLOAD_POOL Stepper
    STYLE Interactive
    CLICK Stepper
    CLICK_INDEX StepperX
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    CLICK_ENABLED
)
vivid_catalog_widget(
    ID 49
    KIND Timeline
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.timeline
    CPP_TYPE Timeline
    THEME_BASE Timeline
    FACTORY timeline
    FACTORY_POOL timelines_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 50
    KIND RichText
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.rich_text
    CPP_TYPE RichText
    THEME_BASE RichText
    FACTORY rich_text
    FACTORY_POOL rich_texts_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 51
    KIND CodeBlock
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.code_block
    CPP_TYPE CodeBlock
    THEME_BASE CodeBlock
    FACTORY code_block
    FACTORY_POOL code_blocks_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 52
    KIND ProgressWheel
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.progress_wheel
    CPP_TYPE ProgressWheel
    THEME_BASE ProgressWheel
    FACTORY progress_wheel
    FACTORY_POOL progress_wheels_
    FACTORY_CREATE None
    PAYLOAD_POOL Progress
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 53
    KIND WaveformView
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.waveform_view
    CPP_TYPE WaveformView
    THEME_BASE WaveformView
    FACTORY waveform_view
    FACTORY_POOL waveform_views_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 54
    KIND BatteryGauge
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.battery_gauge
    CPP_TYPE BatteryGauge
    THEME_BASE BatteryGauge
    FACTORY battery_gauge
    FACTORY_POOL batteries_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 55
    KIND HistogramView
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.histogram_view
    CPP_TYPE HistogramView
    THEME_BASE HistogramView
    FACTORY histogram_view
    FACTORY_POOL histograms_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 56
    KIND RingIndication
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.ring_indication
    CPP_TYPE RingIndication
    THEME_BASE RingIndication
    FACTORY ring_indication
    FACTORY_POOL rings_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 57
    KIND TextBox
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.text_box
    CPP_TYPE TextBox
    THEME_BASE TextBox
    FACTORY text_box
    FACTORY_POOL text_boxes_
    FACTORY_CREATE Text
    PAYLOAD_POOL Label
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 58
    KIND FoldablePanel
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.foldable_panel
    CPP_TYPE FoldablePanel
    THEME_BASE FoldablePanel
    FACTORY foldable_panel
    FACTORY_POOL fold_panels_
    FACTORY_CREATE Text
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 59
    KIND ProgressFlowing
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.progress_flowing
    CPP_TYPE ProgressFlowing
    THEME_BASE ProgressFlowing
    FACTORY progress_flowing
    FACTORY_POOL progress_flow_
    FACTORY_CREATE None
    PAYLOAD_POOL Progress
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 60
    KIND CloudyGlass
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.cloudy_glass
    CPP_TYPE CloudyGlass
    THEME_BASE CloudyGlass
    FACTORY cloudy_glass
    FACTORY_POOL glass_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 61
    KIND NumberList
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.number_list
    CPP_TYPE NumberList
    THEME_BASE NumberList
    FACTORY number_list
    FACTORY_POOL number_lists_
    FACTORY_CREATE None
    PAYLOAD_POOL NumberList
    STYLE PressOnly
    CLICK NumberList
    CLICK_INDEX NumberListY
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
    CLICK_ENABLED
)
vivid_catalog_widget(
    ID 62
    KIND ProgressBarRound
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.progress_bar_round
    CPP_TYPE ProgressBarRound
    THEME_BASE ProgressBarRound
    FACTORY progress_bar_round
    FACTORY_POOL progress_round_
    FACTORY_CREATE None
    PAYLOAD_POOL Progress
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 63
    KIND SpinZoomWidget
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.spin_zoom_widget
    CPP_TYPE SpinZoomWidget
    THEME_BASE SpinZoomWidget
    FACTORY spin_zoom_widget
    FACTORY_POOL spin_zoom_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE PressOnly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 64
    KIND SpinningWheel
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.spinning_wheel
    CPP_TYPE SpinningWheel
    THEME_BASE SpinningWheel
    FACTORY spinning_wheel
    FACTORY_POOL spinning_wheel_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 65
    KIND ImageBox
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.image_box
    CPP_TYPE ImageBox
    THEME_BASE ImageBox
    FACTORY image_box
    FACTORY_POOL image_boxes_
    FACTORY_CREATE None
    PAYLOAD_POOL Image
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 66
    KIND MeterPointer
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.meter_pointer
    CPP_TYPE MeterPointer
    THEME_BASE MeterPointer
    FACTORY meter_pointer
    FACTORY_POOL meters_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 67
    KIND ProgressBarDrill
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.progress_bar_drill
    CPP_TYPE ProgressBarDrill
    THEME_BASE ProgressBarDrill
    FACTORY progress_bar_drill
    FACTORY_POOL progress_drill_
    FACTORY_CREATE None
    PAYLOAD_POOL Progress
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 68
    KIND SpectrumView
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.spectrum_view
    CPP_TYPE SpectrumView
    THEME_BASE SpectrumView
    FACTORY spectrum_view
    FACTORY_POOL spectrum_views_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 69
    KIND BusyWheel
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.busy_wheel
    CPP_TYPE BusyWheel
    THEME_BASE BusyWheel
    FACTORY busy_wheel
    FACTORY_POOL busy_wheels_
    FACTORY_CREATE None
    PAYLOAD_POOL Spinner
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 70
    KIND ConsoleBox
    SCENE_SUPPORT Supported
    OBJECT_MODULE charm.widgets.console_box
    CPP_TYPE ConsoleBox
    THEME_BASE ConsoleBox
    FACTORY console_box
    FACTORY_POOL console_boxes_
    FACTORY_CREATE None
    PAYLOAD_POOL TextList
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 71
    KIND BatteryGasGauge
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.battery_gasgauge
    CPP_TYPE BatteryGasGauge
    THEME_BASE BatteryGasGauge
    FACTORY battery_gasgauge
    FACTORY_POOL battery_gasgauge_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
vivid_catalog_widget(
    ID 72
    KIND Histogram
    SCENE_SUPPORT Unsupported
    OBJECT_MODULE charm.widgets.histogram
    CPP_TYPE Histogram
    THEME_BASE Histogram
    FACTORY histogram
    FACTORY_POOL histogram_
    FACTORY_CREATE None
    PAYLOAD_POOL None
    STYLE Readonly
    CLICK None
    CLICK_INDEX None
    GROUP_KIND None
    WHEEL_TARGET NearestAncestor
    DRAG_BEHAVIOR None
    DRAG_BEHAVIOR_ONLY None
    WHEEL_TARGET_ONLY None
    SCROLL_AXIS Vertical
    WHEEL_AXIS PreferVertical
)
