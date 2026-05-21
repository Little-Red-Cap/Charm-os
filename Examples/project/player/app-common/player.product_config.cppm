module;

export module player.product_config;

export namespace player::product_config {
#if defined(CHARM_PLAYER_RESOURCE_FONT_PATH)
    inline constexpr const char* default_font_path = CHARM_PLAYER_RESOURCE_FONT_PATH;
#else
    inline constexpr const char* default_font_path = "/font/gflex_variable.ttf";
#endif

#if defined(CHARM_PLAYER_RESOURCE_FONT_SMALL_PX)
    inline constexpr int default_font_small_px = CHARM_PLAYER_RESOURCE_FONT_SMALL_PX;
#else
    inline constexpr int default_font_small_px = 14;
#endif

#if defined(CHARM_PLAYER_RESOURCE_FONT_NORMAL_PX)
    inline constexpr int default_font_normal_px = CHARM_PLAYER_RESOURCE_FONT_NORMAL_PX;
#else
    inline constexpr int default_font_normal_px = 18;
#endif

#if defined(CHARM_PLAYER_RESOURCE_FONT_LARGE_PX)
    inline constexpr int default_font_large_px = CHARM_PLAYER_RESOURCE_FONT_LARGE_PX;
#else
    inline constexpr int default_font_large_px = 76;
#endif

#if defined(CHARM_PLAYER_HOST_STORAGE_VHD_PATH)
    inline constexpr const char* host_default_vhd_path = CHARM_PLAYER_HOST_STORAGE_VHD_PATH;
#else
    inline constexpr const char* host_default_vhd_path = "";
#endif
}
