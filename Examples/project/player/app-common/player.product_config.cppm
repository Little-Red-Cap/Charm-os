module;
#include <cstddef>

export module player.product_config;

export namespace player::product_config {
#if defined(CHARM_PLAYER_LYRICS)
    inline constexpr bool lyrics_enabled = CHARM_PLAYER_LYRICS != 0;
#elif defined(CHARM_PLAYER_MCU) && CHARM_PLAYER_MCU
    inline constexpr bool lyrics_enabled = false;
#else
    inline constexpr bool lyrics_enabled = true;
#endif

    inline constexpr std::size_t path_text_capacity = 260;
    inline constexpr std::size_t primary_title_text_capacity = 192;
    inline constexpr std::size_t primary_subtitle_text_capacity = 64;
    inline constexpr std::size_t status_text_capacity = 128;
    inline constexpr std::size_t mode_text_capacity = 48;
    inline constexpr std::size_t list_hint_text_capacity = 128;
    inline constexpr std::size_t debug_text_capacity = 128;
    inline constexpr std::size_t info_text_capacity = 96;
    inline constexpr std::size_t track_format_text_capacity = 12;
    inline constexpr std::size_t home_stats_total_text_capacity = 32;
    inline constexpr std::size_t home_stats_plays_text_capacity = 16;
    inline constexpr std::size_t home_stats_average_text_capacity = 32;
    inline constexpr std::size_t mount_status_text_capacity = 128;
    inline constexpr std::size_t library_context_key_capacity = 192;
    inline constexpr std::size_t library_row_title_capacity = 192;
    inline constexpr std::size_t library_row_subtitle_capacity = 128;
    inline constexpr std::size_t library_row_tail_capacity = 32;
    inline constexpr std::size_t library_unique_album_text_capacity = 96;
    inline constexpr std::size_t library_segment_capacity = 8;
    inline constexpr std::size_t home_collage_slots = 6;
    inline constexpr std::size_t list_cover_cache_entries_max = 12;
#if defined(CHARM_PLAYER_LIST_COVER_CACHE_ENTRIES)
    inline constexpr int list_cover_cache_entries_config =
        CHARM_PLAYER_LIST_COVER_CACHE_ENTRIES;
    static_assert(list_cover_cache_entries_config >= 0,
                  "CHARM_PLAYER_LIST_COVER_CACHE_ENTRIES must not be negative");
    static_assert(static_cast<std::size_t>(list_cover_cache_entries_config)
                      <= list_cover_cache_entries_max,
                  "CHARM_PLAYER_LIST_COVER_CACHE_ENTRIES exceeds product max");
    inline constexpr std::size_t list_cover_cache_entries =
        static_cast<std::size_t>(list_cover_cache_entries_config);
#else
    inline constexpr std::size_t list_cover_cache_entries = list_cover_cache_entries_max;
#endif
    inline constexpr std::size_t listening_stats_history_weeks = 12;
    inline constexpr std::size_t listening_stats_io_bytes = 2048;
    inline constexpr std::size_t listening_stats_line_bytes = 192;
    inline constexpr std::size_t recent_track_history_entries = 12;
    inline constexpr std::size_t recent_track_history_io_bytes = 4096;
    inline constexpr std::size_t recent_track_history_line_bytes = 320;
    inline constexpr std::size_t track_label_text_capacity = 192;
    inline constexpr std::size_t track_title_text_capacity = 192;
    inline constexpr std::size_t track_subtitle_text_capacity = 32;
    inline constexpr std::size_t scan_status_text_capacity = 128;
    inline constexpr std::size_t max_scan_dirs = 64;

#if defined(CHARM_PLAYER_LYRICS_MAX_LINES)
    inline constexpr std::size_t lyrics_max_lines = CHARM_PLAYER_LYRICS_MAX_LINES;
#elif defined(CHARM_PLAYER_MCU) && CHARM_PLAYER_MCU
    inline constexpr std::size_t lyrics_max_lines = 64;
#else
    inline constexpr std::size_t lyrics_max_lines = 192;
#endif

#if defined(CHARM_PLAYER_LYRICS_LINE_TEXT_CAPACITY)
    inline constexpr std::size_t lyrics_line_text_capacity =
        CHARM_PLAYER_LYRICS_LINE_TEXT_CAPACITY;
#elif defined(CHARM_PLAYER_MCU) && CHARM_PLAYER_MCU
    inline constexpr std::size_t lyrics_line_text_capacity = 96;
#else
    inline constexpr std::size_t lyrics_line_text_capacity = 160;
#endif

    inline constexpr std::size_t lyrics_path_text_capacity = path_text_capacity;

#if defined(CHARM_PLAYER_LYRICS_RAW_READ_BYTES)
    inline constexpr std::size_t lyrics_raw_read_bytes = CHARM_PLAYER_LYRICS_RAW_READ_BYTES;
#elif defined(CHARM_PLAYER_MCU) && CHARM_PLAYER_MCU
    inline constexpr std::size_t lyrics_raw_read_bytes = 12288;
#else
    inline constexpr std::size_t lyrics_raw_read_bytes = 32768;
#endif

#if defined(CHARM_PLAYER_RESOURCE_FONT_PATH)
    inline constexpr const char* default_font_path = CHARM_PLAYER_RESOURCE_FONT_PATH;
#else
    inline constexpr const char* default_font_path = "/font/gflex_variable.ttf";
#endif

#if defined(CHARM_PLAYER_RESOURCE_FONT_FALLBACK_PATH)
    inline constexpr const char* default_font_fallback_path = CHARM_PLAYER_RESOURCE_FONT_FALLBACK_PATH;
#else
    inline constexpr const char* default_font_fallback_path = "";
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
