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
