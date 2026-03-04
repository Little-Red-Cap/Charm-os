module;
#include "core/features.hpp"

export module charm.ui.vivid;

export import charm.core.config;
export import charm.core.event;
export import charm.core.factory;
export import charm.core.geometry;
export import charm.core.handle;
#if !defined(CHARM_VIVID_SOA_ONLY)
export import charm.core.container;
export import charm.core.gui;
export import charm.core.input_router;
export import charm.core.input_router_bridge;
export import charm.core.input_interaction;
export import ui.input_adapter;
export import charm.core.virtual_list;
export import charm.core.layout;
export import charm.core.object;
export import charm.core.pool;
export import charm.core.string;
#endif
export import charm.core.style;
export import charm.core.style_sheet;
export import charm.core.theme_preset;
#if !defined(CHARM_VIVID_SOA_ONLY)
export import charm.core.render_tree;
#endif
export import charm.core.soa_kernel;
export import charm.core.soa_layout;
export import charm.core.soa_gui;
export import charm.core.soa_router;

export import ui.render_backend;
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.draw_cmd;
export import charm.gfx.framebuffer;
export import charm.gfx.image;
export import charm.gfx.pixel_format;
export import charm.gfx.render;

export import charm.font;
export import charm.font.typography;
export import charm.font.font_noto_ascii_12;
export import charm.font.font_noto_sc_12;

#if !defined(CHARM_VIVID_SOA_ONLY)
#if CHARM_VIVID_ENABLE_WIDGET_Button
export import charm.widgets.button;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
export import charm.widgets.checkbox;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
export import charm.widgets.image;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
export import charm.widgets.image_box;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
export import charm.widgets.label;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
export import charm.widgets.list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
export import charm.widgets.list_view;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
export import charm.widgets.progress;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
export import charm.widgets.radio;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
export import charm.widgets.scroll_container;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
export import charm.widgets.scrollbar;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
export import charm.widgets.slider;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
export import charm.widgets.switcher;
#endif
export import charm.widgets.text;
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
export import charm.widgets.text_area;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
export import charm.widgets.text_input;
#endif
#endif
