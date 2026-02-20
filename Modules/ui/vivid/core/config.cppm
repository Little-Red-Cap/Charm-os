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
export constexpr int screen_height = 960;
#endif

export constexpr PixelFormat screen_pixel_format = PixelFormat::RGB888;

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
