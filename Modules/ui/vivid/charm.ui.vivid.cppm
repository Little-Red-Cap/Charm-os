module;
#include "core/features.hpp"

export module charm.ui.vivid;

export import charm.core.config;
export import charm.core.event;
export import charm.core.factory;
export import charm.core.geometry;
export import charm.core.handle;
export import charm.core.style;
export import charm.core.style_sheet;
export import charm.core.theme_preset;

export import ui.render_backend;
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.draw_cmd;
export import charm.gfx.framebuffer;
export import charm.gfx.image;
export import charm.gfx.snapshot;
export import charm.gfx.pixel_format;
export import charm.gfx.render_style;

export import charm.font;
export import charm.font.typography;
// deprecated: preset font resources; avoid new deps, will be moved out later.
export import charm.font.font_noto_ascii_12;
export import charm.font.font_noto_sc_12;
