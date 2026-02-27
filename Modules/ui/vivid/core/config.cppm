export module charm.core.config;

export import charm.gfx.pixel_format;

#ifdef CHARM_VIVID_SCREEN_WIDTH
export constexpr int screen_width = CHARM_VIVID_SCREEN_WIDTH;
#else
export constexpr int screen_width = 480;
#endif

#ifdef CHARM_VIVID_SCREEN_HEIGHT
export constexpr int screen_height = CHARM_VIVID_SCREEN_HEIGHT;
#else
export constexpr int screen_height = 800;
#endif

#ifdef CHARM_VIVID_SCREEN_PIXEL_FORMAT_RGB565
export constexpr PixelFormat screen_pixel_format = PixelFormat::RGB565;
#else
export constexpr PixelFormat screen_pixel_format = PixelFormat::RGB888;
#endif

#ifdef CHARM_VIVID_LAYER_CACHE_SLOTS
export constexpr int layer_cache_slots = CHARM_VIVID_LAYER_CACHE_SLOTS;
#else
export constexpr int layer_cache_slots = 1;
#endif

#ifdef CHARM_VIVID_LAYER_CACHE_WIDTH
export constexpr int layer_cache_width = CHARM_VIVID_LAYER_CACHE_WIDTH;
#else
// Default to half-screen cache cap for subtree caching eligibility.
// Note: the cache buffer is still full-screen until the canvas is generalized.
export constexpr int layer_cache_width = screen_width / 2;
#endif

#ifdef CHARM_VIVID_LAYER_CACHE_HEIGHT
export constexpr int layer_cache_height = CHARM_VIVID_LAYER_CACHE_HEIGHT;
#else
// Default to half-screen cache cap for subtree caching eligibility.
// Note: the cache buffer is still full-screen until the canvas is generalized.
export constexpr int layer_cache_height = screen_height / 2;
#endif

#ifndef CHARM_VIVID_ENABLE_FLOAT_WIDGETS
#define CHARM_VIVID_ENABLE_FLOAT_WIDGETS 1
#endif

export constexpr bool enable_float_widgets = (CHARM_VIVID_ENABLE_FLOAT_WIDGETS != 0);
