export module charm.core.config;

export import charm.gfx.pixel_format;

export
#ifdef CHARM_VIVID_SCREEN_WIDTH
constexpr int screen_width = CHARM_VIVID_SCREEN_WIDTH;
#else
constexpr int screen_width = 1280;
#endif

export
#ifdef CHARM_VIVID_SCREEN_HEIGHT
constexpr int screen_height = CHARM_VIVID_SCREEN_HEIGHT;
#else
constexpr int screen_height = 720;
#endif

export
constexpr PixelFormat screen_pixel_format = PixelFormat::RGB888;
