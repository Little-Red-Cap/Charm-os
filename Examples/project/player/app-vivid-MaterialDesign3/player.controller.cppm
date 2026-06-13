module;
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef CHARM_PLAYER_LYRICS
#define CHARM_PLAYER_LYRICS 1
#endif

export module player.controller;

import player.fixed_string;
import player.mcu_policy;
import player.app_config;
import audio.eq;
import audio.player;
import audio.result;
import charm.core.event;
import charm.core.geometry;
import charm.core.handle;
import charm.gfx.color;
import charm.gfx.image;
import charm.ui.scene.text_style;
import charm.ui.scene.anchored_menu;
import charm.ui.scene.pill_surface;
import charm.ui.scene.seek_bar_style;
import charm.font.typography;
import charm.system.clock;
import player.playback;
import player.input;
import player.fs_utils;
import player.media_library;
import player.media_scan;
import player.product_config;
import player.scene_runtime;
import player.stats_history;
import player.storage;
import player.time_utils;
import player.track_probe;
import player.ui;
import player.cover;
import player.cover_theme;
import player.font_cache;
#if CHARM_PLAYER_LYRICS
import player.lyrics;
#endif
import charm.widgets.perf_overlay;
import charm.core.style_sheet;
import service.fixed_vector;
#if defined(CHARM_AUDIO_USE_VFS)
import audio.source.fs;
#else
import audio.source.file;
#endif

export namespace player {
    using namespace player::fs_utils;
    using namespace player::ui;

    enum class UiKey {
        Up,
        Down,
        Left,
        Enter,
        PlayToggle,
        Next,
        Prev,
        Mode,
    };

    enum class CoverStrategy : std::uint8_t {
        embedded_first,
        folder_first,
        embedded_only,
        folder_only,
    };

    enum class PlayerPage : std::uint8_t {
        Probe,
        Home,
        NowPlaying,
        Library,
    };

    enum class LibraryTab : std::uint8_t {
        Songs,
        Albums,
        Artists,
    };

    enum class ListSort : std::uint8_t {
        NameAsc,
        NameDesc,
    };

    enum class ProgressInteractionState : std::uint8_t {
        Idle,
        DraggingPreview,
        SeekCommitted,
        Unavailable,
    };

    struct UiHandles {
        WidgetHandle page_probe{};
        WidgetHandle probe_backdrop{};
        WidgetHandle probe_panel{};
        WidgetHandle probe_title{};
        WidgetHandle probe_hint{};
        WidgetHandle probe_hero_top{};
        WidgetHandle probe_hero_bottom{};
        WidgetHandle probe_hero_subtitle{};
        WidgetHandle probe_ref_title{};
        WidgetHandle probe_ref_meta{};
        WidgetHandle page_home{};
        WidgetHandle page_now_playing{};
        WidgetHandle page_library{};
        WidgetHandle root{};
        WidgetHandle home_scroll{};
        WidgetHandle home_backdrop{};
        WidgetHandle home_title_top{};
        WidgetHandle home_title_bottom{};
        WidgetHandle home_subtitle{};
        WidgetHandle home_play{};
        WidgetHandle home_daily_mix_card{};
        WidgetHandle home_daily_mix_header_band{};
        WidgetHandle home_daily_mix_title{};
        WidgetHandle home_daily_mix_subtitle{};
        WidgetHandle home_daily_mix_body{};
        WidgetHandle home_daily_mix_chip{};
        WidgetHandle home_recently_played_card{};
        WidgetHandle home_recently_played_title{};
        WidgetHandle home_recently_played_body{};
        WidgetHandle home_recently_played_chip{};
        WidgetHandle home_recently_played_action{};
        WidgetHandle home_stats_probe_card{};
        WidgetHandle home_stats_header_band{};
        WidgetHandle home_stats_title{};
        WidgetHandle home_stats_subtitle{};
        WidgetHandle home_stats_plays_label{};
        WidgetHandle home_stats_avg_label{};
        WidgetHandle home_stats_action{};
        WidgetHandle home_cover_big{};
        WidgetHandle home_cover_left{};
        WidgetHandle home_cover_right{};
        WidgetHandle home_cover_small{};
        WidgetHandle home_cover_bottom_left{};
        WidgetHandle home_cover_bottom_right{};
        WidgetHandle home_stats_total{};
        WidgetHandle home_stats_plays{};
        WidgetHandle home_stats_avg{};
        std::array<WidgetHandle, 7> home_stats_bars{};
        std::array<Rect, 7> home_stats_bar_slots{};
        std::array<WidgetHandle, 3> home_daily_mix_preview_plates{};
        std::array<WidgetHandle, 3> home_daily_mix_covers{};
        std::array<WidgetHandle, 3> home_recently_played_preview_plates{};
        std::array<WidgetHandle, 3> home_recently_played_covers{};
        WidgetHandle now_back{};
        WidgetHandle now_more{};
#if CHARM_PLAYER_LYRICS
        WidgetHandle now_lyrics{};
#endif
        WidgetHandle now_backdrop{};
        WidgetHandle now_top_bar{};
        WidgetHandle now_cover_plate{};
#if CHARM_PLAYER_LYRICS
        WidgetHandle now_lyrics_panel{};
        WidgetHandle now_lyrics_prev{};
        WidgetHandle now_lyrics_current{};
        WidgetHandle now_lyrics_next{};
        WidgetHandle now_lyrics_status{};
#endif
        WidgetHandle now_text_group{};
        WidgetHandle now_progress_group{};
        WidgetHandle now_aux_group{};
        WidgetHandle cover{};
        WidgetHandle title{};
        WidgetHandle subtitle{};
        WidgetHandle status{};
        WidgetHandle progress_visual{};
        WidgetHandle progress{};
        WidgetHandle time_left{};
        WidgetHandle time_right{};
        WidgetHandle info_tag{};
        WidgetHandle spectrum{};
        WidgetHandle eq_panel{};
        WidgetHandle eq_title{};
        std::array<WidgetHandle, kEqBands> eq_labels{};
        std::array<WidgetHandle, kEqBands> eq_sliders{};
        std::array<WidgetHandle, kEqBands> eq_values{};
        WidgetHandle volume_label{};
        WidgetHandle volume_slider{};
        WidgetHandle volume_value{};
        WidgetHandle dc_label{};
        WidgetHandle dc_switch{};
        WidgetHandle clip_label{};
        WidgetHandle clip_switch{};
        WidgetHandle clip_slider{};
        WidgetHandle clip_value{};
        WidgetHandle list{};
        WidgetHandle library_hero_title{};
        WidgetHandle list_tab_songs{};
        WidgetHandle list_tab_albums{};
        WidgetHandle list_tab_artist{};
        WidgetHandle list_tab_indicator{};
        WidgetHandle list_shuffle{};
        WidgetHandle list_title{};
        WidgetHandle list_path_bg{};
        WidgetHandle list_path{};
        WidgetHandle list_path_hit{};
        WidgetHandle list_sort{};
        WidgetHandle list_hint{};
        WidgetHandle list_scroll{};
        WidgetHandle list_context_root{};
        WidgetHandle list_context_cover{};
        WidgetHandle list_context_cover_scrim{};
        WidgetHandle list_context_cover_placeholder{};
        WidgetHandle list_context_title{};
        WidgetHandle list_context_subtitle{};
        WidgetHandle list_action_scrim{};
        WidgetHandle list_action_card{};
        WidgetHandle list_action_title{};
        std::array<WidgetHandle, 3> list_action_items{};
        WidgetHandle list_info_scrim{};
        WidgetHandle list_info_card{};
        WidgetHandle list_info_cover_plate{};
        WidgetHandle list_info_cover{};
        WidgetHandle list_info_eyebrow{};
        WidgetHandle list_info_title{};
        WidgetHandle list_info_subtitle{};
        WidgetHandle list_info_meta{};
        WidgetHandle list_info_path_plate{};
        WidgetHandle list_info_path_title{};
        WidgetHandle list_info_path{};
        WidgetHandle list_info_path_detail{};
        WidgetHandle list_info_hint{};
        WidgetHandle list_info_close{};
        WidgetHandle mode_hint{};
        WidgetHandle btn_prev{};
        WidgetHandle btn_pause{};
        WidgetHandle btn_next{};
        WidgetHandle btn_mode{};
        WidgetHandle controls{};
        WidgetHandle bottom_bar{};
        WidgetHandle bottom_hit{};
        WidgetHandle bottom_cover{};
        WidgetHandle bottom_title{};
        WidgetHandle bottom_subtitle{};
        WidgetHandle bottom_play{};
        WidgetHandle bottom_next{};
        WidgetHandle transition_root{};
        WidgetHandle transition_shell{};
        WidgetHandle transition_hit{};
        WidgetHandle transition_cover{};
        WidgetHandle transition_title{};
        WidgetHandle transition_subtitle{};
        WidgetHandle transition_play{};
        WidgetHandle nav_bar{};
        WidgetHandle nav_home_indicator{};
        WidgetHandle nav_search_indicator{};
        WidgetHandle nav_library_indicator{};
        WidgetHandle nav_home{};
        WidgetHandle nav_search{};
        WidgetHandle nav_library{};
        WidgetHandle nav_home_icon{};
        WidgetHandle nav_search_icon{};
        WidgetHandle nav_library_icon{};
        WidgetHandle nav_home_label{};
        WidgetHandle nav_search_label{};
        WidgetHandle nav_library_label{};
        WidgetHandle cover_debug{};
        WidgetHandle debug_text{};
    };

    struct PlayerController {
        struct LibraryRowRecipe {
            int row_index{-1};
            int track_index{-1};
            bool group_row{false};
            bool current_row{false};
            bool selected_row{false};
            bool show_tail_action{false};
            bool text_resolved{false};
            const char* menu_title_cstr{"Track"};
            std::string_view title_text{};
            std::string_view subtitle_text{};
            std::string_view tail_text{};
            const char* title_cstr{""};
            const char* subtitle_cstr{""};
            const char* tail_cstr{""};
            std::string_view detail_primary{};
            std::string_view detail_secondary{};
            std::string_view track_path{};
            std::string_view cover_path{};
            ::ui::scene::ImageId tail_icon{};
            ::ui::scene::ImageId tail_action_icon{};
            ::ui::scene::ImageId fallback_icon{};
            bool prefer_cover{false};

            std::string_view title() const noexcept {
                return title_text;
            }

            std::string_view subtitle() const noexcept {
                return subtitle_text;
            }

            std::string_view tail() const noexcept {
                return tail_text;
            }

            const char* title_c_str() const noexcept {
                return title_cstr ? title_cstr : "";
            }

            const char* subtitle_c_str() const noexcept {
                return subtitle_cstr ? subtitle_cstr : "";
            }

            const char* tail_c_str() const noexcept {
                return tail_cstr ? tail_cstr : "";
            }
        };

        PlaybackEngine playback{};
        PlayerSceneRuntime scene_runtime{};
        ::ui::scene::SceneAccess access{};
        UiHandles handles{};
        PlayerIconIds icons{};
        int last_time_sec{-1};
        int last_time_total_sec{-1};
        StorageView storage{};
        int track_index{0};
        FixedString<192> title_text{};
        FixedString<64> subtitle_text{};
        FixedString<260> cover_path{};
        FixedString<260> cover_embedded_path{};
        FixedString<260> cover_folder_path{};
        FixedString<260> cover_failed_embedded_path{};
        FixedString<260> cover_failed_folder_path{};
        FixedString<260> cover_tint_path{};
        FixedString<12> track_format_text{};
        FixedString<128> last_status_text{};
        FixedString<48> last_mode_text{};
        FixedString<192> last_list_title_text{};
        FixedString<128> last_list_hint_text{};
        FixedString<128> last_debug_text{};
        FixedString<96> last_info_text{};
        FixedString<32> last_home_stats_total_text{};
        FixedString<16> last_home_stats_plays_text{};
        FixedString<32> last_home_stats_avg_text{};
#if CHARM_PLAYER_LYRICS
        FixedString<product_config::lyrics_line_text_capacity> lyrics_prev_text{};
        FixedString<product_config::lyrics_line_text_capacity> lyrics_current_text{};
        FixedString<product_config::lyrics_line_text_capacity> lyrics_next_text{};
        FixedString<96> lyrics_status_text{};
#endif
        std::uint64_t track_size_bytes{0};
        ResolvedCover current_cover{};
        cover_theme::CoverTheme cover_theme{};
        cover_theme::NowPlayingColorRoles now_playing_roles{
            cover_theme::derive_now_playing_color_roles(cover_theme::CoverTheme{})
        };
        bool cover_ready{false};
        CoverStrategy cover_strategy{CoverStrategy::embedded_first};
        cover_theme::CoverThemeMode cover_theme_mode{cover_theme::CoverThemeMode::primary_container};
        cover_theme::CoverThemePaletteStyle cover_palette_style{
            cover_theme::CoverThemePaletteStyle::tonal_spot
        };
        bool fs_ready{false};
        bool track_preloaded{false};
#if CHARM_PLAYER_LYRICS
        bool lyrics_visible{false};
        LyricsLoadResult lyrics_result{};
        int last_lyrics_position_ms{-1};
        int last_lyrics_index{-2};
#endif
        int preloaded_duration_sec{0};
        int play_mode{0};
        int last_play_button_state{-1};
        int last_list_count{-1};
        bool ignore_list_select{false};
        int last_list_selected{-1};
        int list_action_menu_index{-1};
        int list_action_menu_focus_index{-1};
        int list_info_popup_track_index{-1};
        LibraryTab library_tab{LibraryTab::Songs};
        FixedString<192> library_context_key{};
        ListSort list_sort{ListSort::NameAsc};
        bool list_shuffle_enabled{false};
        std::uint32_t list_shuffle_seed{0};
        struct LibraryVisualState {
            LibraryTab tab{LibraryTab::Songs};
            bool has_context{false};
            bool path_active{false};
            bool shuffle_enabled{false};
            bool sort_desc{false};
            Rect active_tab_rect{};
            std::uint32_t generation{0};
            bool valid{false};
        } library_visual_state{};
        std::uint32_t library_visual_generation{1};
        service::FixedVector<int, kMaxTracks> list_order{};
        service::FixedVector<int, kMaxTracks> playback_queue_order{};
        int playback_queue_position{-1};
        std::uint32_t playback_queue_generation{1};
        service::FixedVector<int, kMaxTracks> track_duration_cache_sec{};
        std::size_t list_duration_probe_cursor{0};
        std::uint64_t last_list_duration_probe_ms{0};
        mutable FixedString<product_config::library_row_title_capacity> list_row_title_scratch{};
        mutable FixedString<product_config::library_row_subtitle_capacity> list_row_subtitle_scratch{};
        mutable FixedString<product_config::library_row_tail_capacity> list_row_tail_scratch{};
        mutable FixedString<product_config::path_text_capacity> list_cover_path_scratch{};
        struct ListCoverCacheEntry {
            FixedString<product_config::path_text_capacity> path{};
            ResolvedCover cover{};
            bool missing{false};
        };
        static constexpr std::size_t kHomeCollageSlots = 6;
        static constexpr std::size_t kListCoverCache =
            product_config::list_cover_cache_entries;
        std::array<ListCoverCacheEntry, kListCoverCache> list_cover_cache{};
        std::size_t list_cover_next{0};
        std::array<int, kHomeCollageSlots> last_home_collage_track_indices{{-2, -2, -2, -2, -2, -2}};
        bool progress_dragging{false};
        int progress_drag_value{0};
        int progress_drag_sec{0};
        int progress_committed_sec{0};
        ProgressInteractionState progress_interaction_state{ProgressInteractionState::Unavailable};
        std::uint8_t progress_committed_hold_frames{0};
        struct WeeklyListeningStats {
            int week_key{0};
            std::array<int, 7> seconds{};
            int total_plays{0};
            std::uint64_t last_tick_ms{0};
            std::uint64_t last_persist_ms{0};
            bool active{false};
            bool dirty{true};
            bool persist_dirty{true};
        } weekly_listening_stats{};
        ListeningStatsHistory weekly_listening_history{};
        FixedString<260> font_ttf_path{};
        FixedString<260> font_fallback_ttf_path{};
        int font_small_px{0};
        int font_normal_px{0};
        int font_large_px{0};
        bool font_retry_done{false};

        static bool is_audio_extension(std::string_view ext) noexcept {
            if (ext.empty()) return false;
            auto lower = [&](char c) noexcept -> char {
                return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            };
            char buf[8]{};
            const std::size_t n = std::min(ext.size(), sizeof(buf) - 1);
            for (std::size_t i = 0; i < n; ++i) buf[i] = lower(ext[i]);
            std::string_view v{buf, n};
            return v == "flac" || v == "mp3" || v == "wav" || v == "aac"
                || v == "m4a" || v == "ogg" || v == "opus" || v == "ape"
                || v == "alac" || v == "wv";
        }

        static bool is_audio_path(std::string_view path) noexcept {
            const auto dot = path.find_last_of('.');
            if (dot == std::string_view::npos || dot + 1 >= path.size()) return false;
            return is_audio_extension(path.substr(dot + 1));
        }

        static void strip_audio_extension(FixedString<192>& text) {
            std::string_view v = text.view();
            const auto dot = v.find_last_of('.');
            if (dot == std::string_view::npos) return;
            const auto slash = v.find_last_of("/\\");
            if (slash != std::string_view::npos && dot < slash) return;
            const auto ext = v.substr(dot + 1);
            if (!is_audio_extension(ext)) return;
            text.assign(v.substr(0, dot));
        }

        cover_theme::CoverTheme derive_cover_theme(const ResolvedCover& cover) noexcept {
            cover_theme::CoverThemeConfig config{};
            config.mode = cover_theme_mode;
            config.is_dark = true;
            config.palette_style = cover_palette_style;
            config.fallback = kUiBackdropBase;
            return cover_theme::compute_cover_theme_from_resolved(cover, config);
        }

        void sync_now_playing_color_roles() noexcept {
            now_playing_roles = cover_theme::derive_now_playing_color_roles(cover_theme);
        }

        void refresh_cover_theme_dependent_visuals() noexcept {
            sync_now_playing_color_roles();
            refresh_now_playing_visual_content();
            refresh_home_visual_if_active();
            refresh_library_visual_if_active(true);
        }

        void set_cover_theme_tint_source(std::string_view path) noexcept {
            if (path.empty()) {
                cover_tint_path.clear();
                return;
            }
            cover_tint_path.assign(path);
        }

        bool cover_theme_tint_source_matches(std::string_view path) const noexcept {
            return !path.empty() && cover_tint_path.view() == path;
        }

        void apply_cover_theme_from_current_image(std::string_view tint_path = {}) noexcept {
            cover_theme = derive_cover_theme(current_cover);
            set_cover_theme_tint_source(tint_path);
            refresh_cover_theme_dependent_visuals();
        }

        static const char* palette_style_name(cover_theme::CoverThemePaletteStyle style) noexcept {
            switch (style) {
            case cover_theme::CoverThemePaletteStyle::vibrant: return "vibrant";
            case cover_theme::CoverThemePaletteStyle::expressive: return "expressive";
            case cover_theme::CoverThemePaletteStyle::fruit_salad: return "fruit_salad";
            case cover_theme::CoverThemePaletteStyle::tonal_spot:
            default:
                return "tonal_spot";
            }
        }

        static rgba with_alpha(const rgba& c, std::uint8_t a) noexcept {
            return {c.r, c.g, c.b, a};
        }

        static rgba blend_on(const rgba& src, const rgba& bg, std::uint8_t alpha) noexcept {
            return rgba{src.r, src.g, src.b, alpha}.blend_over(bg);
        }

        static StylePatch make_now_backdrop_patch(const rgba& backdrop, const rgba&) noexcept {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = backdrop;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 0;
            return patch;
        }

        static StylePatch make_now_cover_plate_patch(const rgba& bg, int corner_radius, bool has_cover) noexcept {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_border_width = true,
                .apply_corner_radius = true,
                .bg_color = has_cover ? rgba{bg.r, bg.g, bg.b, 0} : bg,
                .border_color = has_cover ? rgba{0, 0, 0, 0}
                                          : rgba{kUiTitle.r, kUiTitle.g, kUiTitle.b, 24},
                .border_width = has_cover ? 0 : 1,
                .corner_radius = corner_radius,
            });
            return patch;
        }

        static StylePatch make_now_surface_role_patch(const player::cover_theme::SurfaceRole& role,
                                                      int corner_radius,
                                                      int padding,
                                                      bool prominent) noexcept {
            StylePatch patch = ::ui::scene::make_surface_recolor_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_font_color = true,
                .bg_color = role.bg,
                .border_color = role.border,
                .font_color = role.fg,
            });
            patch.has_padding = true;
            patch.padding = padding;
            patch.has_corner_radius = true;
            patch.corner_radius = corner_radius;
            (void)prominent;
            return patch;
        }

#if defined(CHARM_PLAYER_COVER_DEBUG)
        static void debug_cover_theme(const cover_theme::CoverTheme& theme, std::string_view path) {
            std::printf("[cover] theme path=%.*s backdrop=%u,%u,%u primary=%u,%u,%u surface=%u,%u,%u\n",
                        static_cast<int>(path.size()), path.data(),
                        theme.backdrop.r, theme.backdrop.g, theme.backdrop.b,
                        theme.primary.r, theme.primary.g, theme.primary.b,
                        theme.surface.r, theme.surface.g, theme.surface.b);
        }

        void update_cover_debug_label() {
            if (!access.valid()) return;
            if (dbg_color_avg == static_cast<std::size_t>(-1)) {
                dbg_color_avg = perf_overlay_debug_channel("color.avg");
                dbg_color_seed = perf_overlay_debug_channel("color.seed");
                dbg_color_backdrop = perf_overlay_debug_channel("color.backdrop");
                dbg_color_primary = perf_overlay_debug_channel("color.primary");
            }
            char buf[192]{};
            std::snprintf(buf, sizeof(buf),
                          "seed %u,%u,%u back %u,%u,%u prim %u,%u,%u",
                          cover_theme.seed_raw.r, cover_theme.seed_raw.g, cover_theme.seed_raw.b,
                          cover_theme.backdrop.r, cover_theme.backdrop.g, cover_theme.backdrop.b,
                          cover_theme.primary.r, cover_theme.primary.g, cover_theme.primary.b);
            if (handles.cover_debug) {
                set_label_slot(handles.cover_debug, text_slots.cover_debug, buf);
            }

            char line3[96]{};
            std::snprintf(line3, sizeof(line3),
                          "avg %u,%u,%u",
                          cover_theme.avg_raw.r, cover_theme.avg_raw.g, cover_theme.avg_raw.b);
            set_perf_overlay_debug_channel(dbg_color_avg, line3);
            std::snprintf(line3, sizeof(line3),
                          "seed %u,%u,%u",
                          cover_theme.seed_raw.r, cover_theme.seed_raw.g, cover_theme.seed_raw.b);
            set_perf_overlay_debug_channel(dbg_color_seed, line3);
            std::snprintf(line3, sizeof(line3),
                          "back %u,%u,%u",
                          cover_theme.backdrop.r, cover_theme.backdrop.g, cover_theme.backdrop.b);
            set_perf_overlay_debug_channel(dbg_color_backdrop, line3);
            std::snprintf(line3, sizeof(line3),
                          "prim %u,%u,%u",
                          cover_theme.primary.r, cover_theme.primary.g, cover_theme.primary.b);
            set_perf_overlay_debug_channel(dbg_color_primary, line3);
        }
#endif

        #include "player.controller.font.inc"

        void cycle_cover_theme() noexcept {
            switch (cover_theme_mode) {
            case cover_theme::CoverThemeMode::primary_container:
                cover_theme_mode = cover_theme::CoverThemeMode::surface_container_high;
                break;
            case cover_theme::CoverThemeMode::surface_container_high:
                cover_theme_mode = cover_theme::CoverThemeMode::seed_backdrop;
                break;
            case cover_theme::CoverThemeMode::seed_backdrop:
            default:
                cover_theme_mode = cover_theme::CoverThemeMode::primary_container;
                break;
            }
            if (!current_cover.key.empty()) {
                apply_cover_theme_from_current_image(current_cover.key.view());
            }
        }

        void cycle_palette_style() noexcept {
            switch (cover_palette_style) {
            case cover_theme::CoverThemePaletteStyle::tonal_spot:
                cover_palette_style = cover_theme::CoverThemePaletteStyle::vibrant;
                break;
            case cover_theme::CoverThemePaletteStyle::vibrant:
                cover_palette_style = cover_theme::CoverThemePaletteStyle::expressive;
                break;
            case cover_theme::CoverThemePaletteStyle::expressive:
                cover_palette_style = cover_theme::CoverThemePaletteStyle::fruit_salad;
                break;
            case cover_theme::CoverThemePaletteStyle::fruit_salad:
            default:
                cover_palette_style = cover_theme::CoverThemePaletteStyle::tonal_spot;
                break;
            }
            if (!current_cover.key.empty()) {
                apply_cover_theme_from_current_image(current_cover.key.view());
            }
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] palette=%s\n", palette_style_name(cover_palette_style));
#endif
        }

        PlayerPage current_page{PlayerPage::Home};
        PlayerPage start_page{PlayerPage::Home};
        PlayerPage default_page{PlayerPage::Home};
        PlayerPage return_page{PlayerPage::Home};
        static constexpr std::array<PlayerPage, 4> kManagedPages{
            PlayerPage::Probe,
            PlayerPage::Home,
            PlayerPage::NowPlaying,
            PlayerPage::Library,
        };
        struct PageRefreshHookContext {
            PlayerController* self{};
            PlayerPage page{PlayerPage::Probe};
        };
        struct PageBinding {
            PlayerPage page{PlayerPage::Probe};
            PlayerPageLayer* layer{};
            WidgetHandle* root{};
            PageRefreshHookContext* show_ctx{};

            WidgetHandle root_handle() const noexcept {
                return root ? *root : WidgetHandle{};
            }
        };
        PlayerPageLayer page_probe_layer{};
        PlayerPageLayer page_now_layer{};
        PlayerPageLayer page_library_layer{};
        PlayerPageLayer page_home_layer{};
        PageRefreshHookContext page_home_show_ctx{};
        PageRefreshHookContext page_now_show_ctx{};
        PageRefreshHookContext page_library_show_ctx{};
        static constexpr std::uint64_t kNowPlayingExpandDurationMs = 360;
        enum class NowPlayingTransitionDirection : std::uint8_t {
            Expand,
            Collapse,
        };
        enum class PageTransitionState : std::uint8_t {
            Idle,
            CapturingSource,
            PreparingDestination,
            CapturingDestination,
            Composing,
            Finishing,
            Aborting,
        };
        struct NowPlayingTransitionEndpoints {
            PlayerPage source_page{PlayerPage::Home};
            PlayerPage destination_page{PlayerPage::NowPlaying};
            bool expanding{false};
        };
        struct NowPlayingExpandTransition {
            bool active{false};
            bool reveal_started{false};
            bool destination_refreshed{false};
#if CHARM_PLAYER_LAYERED_TRANSITIONS
            bool destination_capture_ready{false};
#endif
            NowPlayingTransitionEndpoints route{};
            ::ui::scene::SnapshotHandle source_snapshot{};
            ::ui::scene::SnapshotHandle destination_snapshot{};
            std::uint64_t start_ms{0};
            Rect shell_from{};
            Rect shell_to{};
            Rect cover_from{};
            Rect cover_to{};
            Rect title_from{};
            Rect title_to{};
            Rect subtitle_from{};
            Rect subtitle_to{};
            Rect play_from{};
            Rect play_to{};
            Rect now_top_bar_rect{};
            Rect now_progress_rect{};
            Rect now_controls_rect{};
            Rect now_aux_rect{};
        } now_playing_transition{};
        audio::EqConfig eq_config{};
        std::array<int, kEqBands> eq_values{};
        std::array<int, kEqBands> last_eq_values{};
        int last_volume_value{-1};
        int last_dc_enabled{-1};
        int last_clip_enabled{-1};
        int last_clip_threshold{-1};
        struct TextSlots {
            ::ui::scene::TextSlotId title{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId subtitle{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId status{::ui::scene::kInvalidTextSlot};
#if CHARM_PLAYER_LYRICS
            ::ui::scene::TextSlotId lyrics_prev{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId lyrics_current{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId lyrics_next{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId lyrics_status{::ui::scene::kInvalidTextSlot};
#endif
            ::ui::scene::TextSlotId time_left{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId time_right{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId info_tag{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId mode_hint{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId list_title{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId list_path{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId list_sort{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId list_shuffle{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId list_hint{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId btn_pause{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId bottom_title{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId bottom_subtitle{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId cover_debug{::ui::scene::kInvalidTextSlot};
            std::array<::ui::scene::TextSlotId, kEqBands> eq_values{
                ::ui::scene::kInvalidTextSlot,
                ::ui::scene::kInvalidTextSlot,
                ::ui::scene::kInvalidTextSlot,
                ::ui::scene::kInvalidTextSlot,
                ::ui::scene::kInvalidTextSlot,
            };
            ::ui::scene::TextSlotId volume_value{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId clip_value{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId debug_text{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId home_stats_total{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId home_stats_plays{::ui::scene::kInvalidTextSlot};
            ::ui::scene::TextSlotId home_stats_avg{::ui::scene::kInvalidTextSlot};
        } text_slots{};
        std::uint32_t layer_transition_capture_count{0};
        std::uint32_t layer_transition_release_count{0};
        std::uint32_t layer_transition_capture_fail_count{0};
        std::uint32_t layer_transition_capture_no_slot_count{0};
        std::uint32_t layer_transition_capture_record_fail_count{0};
        std::uint32_t layer_transition_capture_store_fail_count{0};
        std::uint32_t layer_transition_compose_count{0};
        std::uint32_t layer_transition_compose_fail_count{0};
        std::uint32_t layer_transition_composite_pixels{0};
        std::uint32_t layer_transition_pixel_compose_count{0};
        std::uint32_t layer_transition_pixel_compose_pixels{0};
        std::uint32_t layer_transition_destination_capture_count{0};
        std::uint32_t layer_transition_destination_compose_count{0};
        std::uint32_t layer_transition_destination_compose_pixels{0};
#if CHARM_PLAYER_LAYERED_TRANSITIONS
        std::uint32_t layer_transition_capture_source_us{0};
        std::uint32_t layer_transition_prepare_destination_us{0};
        std::uint32_t layer_transition_capture_destination_us{0};
        std::uint32_t layer_transition_first_compose_us{0};
        std::uint32_t layer_transition_destination_capture_defer_frames{0};
        std::uint64_t layer_transition_destination_capture_start_us{0};
#endif
        PageTransitionState page_transition_state{PageTransitionState::Idle};
        std::uint32_t layer_transition_abort_count{0};
#if CHARM_PLAYER_LAYERED_TRANSITIONS
        bool layer_transition_first_compose_recorded{false};
#endif
        bool layer_profile_budget_drill_enabled{false};
        std::uint32_t layer_static_cut_count{0};
        std::uint32_t layer_admission_static_cut_count{0};
        ::ui::scene::LayerAdmission last_layer_admission{
            ::ui::scene::LayerAdmission::PixelDouble};
        ::ui::scene::LayerProfile requested_layer_profile{::ui::scene::LayerProfile::Rich};
        ::ui::scene::LayerProfile effective_layer_profile{::ui::scene::LayerProfile::Rich};
        ::ui::scene::LayerFallbackReason layer_transition_fallback_reason{
            ::ui::scene::LayerFallbackReason::None};
        std::uint32_t layer_transition_budget_fail_count{0};
        std::uint32_t layer_transition_alpha_blend_pixels{0};
        std::uint32_t layer_transition_last_layer_bytes{0};
        std::uint32_t layer_transition_last_layer_bytes_budget{0};
        std::uint32_t layer_transition_last_composite_pixels{0};
        std::uint32_t layer_transition_last_composite_pixels_budget{0};
        bool layer_transition_last_budget_ok{true};
        ::ui::scene::LayerCaptureStatus last_layer_transition_capture_status{
            ::ui::scene::LayerCaptureStatus::Ok};
        FixedString<128> mount_status{};
        std::uint32_t rng_state{0};
        std::uint64_t last_debug_tick_ms{0};
        bool last_running{false};
        std::size_t dbg_color_avg{static_cast<std::size_t>(-1)};
        std::size_t dbg_color_seed{static_cast<std::size_t>(-1)};
        std::size_t dbg_color_backdrop{static_cast<std::size_t>(-1)};
        std::size_t dbg_color_primary{static_cast<std::size_t>(-1)};
        std::size_t dbg_style_button{static_cast<std::size_t>(-1)};
        std::size_t dbg_style_progress{static_cast<std::size_t>(-1)};

        static bool is_flac_path(std::string_view path) noexcept {
            const auto dot = path.find_last_of('.');
            if (dot == std::string_view::npos || dot + 1 >= path.size()) return false;
            std::string_view ext = path.substr(dot + 1);
            if (ext.size() != 4) return false;
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[2])));
            const char d = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[3])));
            return a == 'f' && b == 'l' && c == 'a' && d == 'c';
        }

        static std::string_view format_from_path(std::string_view path) noexcept {
            const auto dot = path.find_last_of('.');
            if (dot == std::string_view::npos || dot + 1 >= path.size()) return {};
            const auto ext = path.substr(dot + 1);
            if (ext.size() == 3) {
                const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
                const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
                const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[2])));
                if (a == 'm' && b == 'p' && c == '3') return "MP3";
                if (a == 'w' && b == 'a' && c == 'v') return "WAV";
                if (a == 'f' && b == 'l' && c == 'a') return "FLA";
            }
            if (ext.size() == 4) {
                const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
                const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
                const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[2])));
                const char d = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[3])));
                if (a == 'f' && b == 'l' && c == 'a' && d == 'c') return "FLAC";
            }
            return {};
        }

        static void format_duration_compact(int total_sec, char* out, std::size_t out_size) noexcept {
            if (!out || out_size == 0) return;
            out[0] = '\0';
            if (total_sec <= 0) return;
            const int hours = total_sec / 3600;
            const int minutes = (total_sec / 60) % 60;
            const int seconds = total_sec % 60;
            if (hours > 0) {
                std::snprintf(out, out_size, "%d:%02d:%02d", hours, minutes, seconds);
            } else {
                std::snprintf(out, out_size, "%d:%02d", total_sec / 60, seconds);
            }
        }

        static std::uint64_t query_track_size(const char* path) noexcept {
            if (!path || !*path) return 0;
#if defined(CHARM_AUDIO_USE_VFS)
            audio::FsDataSource src{};
#else
            audio::FileDataSource src{};
#endif
            if (!src.open(path)) return 0;
            auto size = src.size();
            if (!size) return 0;
            return static_cast<std::uint64_t>(*size);
        }

        void bind_scene_runtime(PlayerSceneRuntime runtime) noexcept {
            scene_runtime = runtime;
            access = runtime.access;
        }

        void bind_player(audio::AudioPlayer& p) {
            playback.set_player(p);
        }

        static float clamp01(float value) noexcept {
            return std::clamp(value, 0.0f, 1.0f);
        }

        static float ease_out_cubic(float t) noexcept {
            const float x = 1.0f - clamp01(t);
            return 1.0f - x * x * x;
        }

        static float smoothstep01(float t) noexcept {
            const float x = clamp01(t);
            return x * x * (3.0f - 2.0f * x);
        }

        static int lerp_int(int from, int to, float t) noexcept {
            const float x = static_cast<float>(from)
                + static_cast<float>(to - from) * clamp01(t);
            return static_cast<int>(std::lround(x));
        }

        static std::uint8_t lerp_u8(std::uint8_t from, std::uint8_t to, float t) noexcept {
            const int value = lerp_int(static_cast<int>(from), static_cast<int>(to), t);
            return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
        }

        static rgba lerp_color(const rgba& from, const rgba& to, float t) noexcept {
            return {
                lerp_u8(from.r, to.r, t),
                lerp_u8(from.g, to.g, t),
                lerp_u8(from.b, to.b, t),
                lerp_u8(from.a, to.a, t),
            };
        }

        static Rect lerp_rect(const Rect& from, const Rect& to, float t) noexcept {
            return {
                lerp_int(from.x, to.x, t),
                lerp_int(from.y, to.y, t),
                lerp_int(from.w, to.w, t),
                lerp_int(from.h, to.h, t),
            };
        }

        static Rect offset_rect(const Rect& rect, int dx, int dy) noexcept {
            return {
                rect.x + dx,
                rect.y + dy,
                rect.w,
                rect.h,
            };
        }

        void set_handle_visible(WidgetHandle handle, bool visible) noexcept {
            if (!access.valid() || !handle) return;
            access.set_visible(handle, visible);
        }

        PageBinding page_binding_for(PlayerPage page) noexcept {
            switch (page) {
            case PlayerPage::Home:
                return PageBinding{page, &page_home_layer, &handles.page_home, &page_home_show_ctx};
            case PlayerPage::NowPlaying:
                return PageBinding{page, &page_now_layer, &handles.page_now_playing, &page_now_show_ctx};
            case PlayerPage::Library:
                return PageBinding{page, &page_library_layer, &handles.page_library, &page_library_show_ctx};
            case PlayerPage::Probe:
                return PageBinding{page, &page_probe_layer, &handles.page_probe, nullptr};
            default:
                return {};
            }
        }

        void set_page_root_visible(PlayerPage page, bool visible) noexcept {
            if (!access.valid()) return;
            const auto binding = page_binding_for(page);
            if (binding.layer && binding.layer->root()) {
                binding.layer->set_visible(access, visible);
                return;
            }
            set_handle_visible(binding.root_handle(), visible);
        }

        void sync_page_root_visibility(PlayerPage page) noexcept {
            for (const auto candidate : kManagedPages) {
                set_page_root_visible(candidate, candidate == page);
            }
        }

        static bool is_mini_bar_page(PlayerPage page) noexcept {
            return page == PlayerPage::Home || page == PlayerPage::Library;
        }

        void set_mini_bar_chrome_visible(bool visible) noexcept {
            set_handle_visible(handles.bottom_bar, visible);
            set_handle_visible(handles.nav_bar, visible);
        }

        void sync_page_chrome(PlayerPage page) noexcept {
            set_mini_bar_chrome_visible(is_mini_bar_page(page));
            update_nav_page_indicator_for(page);
        }

        bool page_has_show_refresh_hook(PlayerPage page) noexcept {
            const auto binding = page_binding_for(page);
            return binding.show_ctx && binding.layer && binding.layer->root();
        }

        void refresh_page(PlayerPage page) {
            switch (page) {
            case PlayerPage::Home:
                refresh_home();
                break;
            case PlayerPage::Library:
                refresh_library();
                break;
            case PlayerPage::NowPlaying:
                refresh_now_playing();
                break;
            case PlayerPage::Probe:
            default:
                break;
            }
        }

        #include "player.controller.now_playing.inc"

        #include "player.controller.pages.inc"
        #include "player.controller.home.inc"
#if CHARM_PLAYER_LYRICS
        #include "player.controller.lyrics.inc"
#else
        void sync_lyrics_visibility() noexcept {
            if (!access.valid()) return;
            const bool has_cover = has_cover_image();
            set_handle_visible(handles.now_cover_plate, !has_cover);
            set_handle_visible(handles.cover, has_cover);
        }

        void clear_current_lyrics() noexcept {}
        void load_current_track_lyrics(std::string_view) noexcept {}
        void refresh_lyrics_window(bool = false) noexcept {}
        void toggle_lyrics_panel() noexcept {}
#endif

        const char* track_path() const noexcept {
            return playback.track_path();
        }

        bool track_ready() const noexcept {
            return playback.track_ready();
        }

        void clear_track_state() noexcept {
            playback.set_track_path(nullptr);
            playback.set_track_ready(false);
            track_size_bytes = 0;
            track_format_text.clear();
            last_info_text.clear();
            clear_current_lyrics();
            set_info_label("");
            reset_cover_image();
        }

        void init_text_slots() {
            if (!access.valid()) return;
            auto alloc = [this]() noexcept {
                return access.alloc_text_slot();
            };
            text_slots.title = alloc();
            text_slots.subtitle = alloc();
            text_slots.status = alloc();
#if CHARM_PLAYER_LYRICS
            text_slots.lyrics_prev = alloc();
            text_slots.lyrics_current = alloc();
            text_slots.lyrics_next = alloc();
            text_slots.lyrics_status = alloc();
#endif
            text_slots.time_left = alloc();
            text_slots.time_right = alloc();
            text_slots.info_tag = alloc();
            text_slots.mode_hint = alloc();
            text_slots.list_title = alloc();
            text_slots.list_path = alloc();
            text_slots.list_sort = alloc();
            text_slots.list_shuffle = alloc();
            text_slots.list_hint = alloc();
            text_slots.btn_pause = alloc();
            text_slots.cover_debug = alloc();
            for (auto& slot : text_slots.eq_values) {
                slot = alloc();
            }
            text_slots.volume_value = alloc();
            text_slots.clip_value = alloc();
            text_slots.debug_text = alloc();
            text_slots.home_stats_total = alloc();
            text_slots.home_stats_plays = alloc();
            text_slots.home_stats_avg = alloc();
            sync_lyrics_visibility();
            clear_current_lyrics();
            set_handle_visible(handles.list_info_scrim, false);
            set_handle_visible(handles.list_info_card, false);
            clear_image_slot_if_present(library_info_cover_slot(), false);
        }

        void set_label(WidgetHandle h, const char* text) {
            if (!access.valid() || !h) return;
            player::font_cache::ensure_text(text);
            access.set_text(h, text);
        }

        void set_label_slot(WidgetHandle h, ::ui::scene::TextSlotId slot, const char* text) {
            if (!access.valid() || !h) return;
            player::font_cache::ensure_text(text);
            if (slot != ::ui::scene::kInvalidTextSlot) {
                access.set_text_slot(h, slot, text);
                return;
            }
            access.set_text(h, text);
        }

        void set_status(const char* text) {
            const char* value = text ? text : "";
            if (last_status_text.view() == value) return;
            last_status_text.assign(value);
            set_label_slot(handles.status, text_slots.status, value);
        }

        const char* mount_not_ready_status_text() const noexcept {
            return mount_status.empty() ? "Mount not ready" : mount_status.c_str();
        }

        bool ensure_playback_fs_ready() {
            if (fs_ready) return true;
            set_status(mount_not_ready_status_text());
            return false;
        }

        int available_track_count() const noexcept {
            const auto* tracks = storage.tracks;
            return tracks ? static_cast<int>(tracks->size()) : 0;
        }

        bool has_available_tracks() const noexcept {
            return available_track_count() > 0;
        }

        template <std::size_t N>
        void set_status_if_present(const FixedString<N>& status) {
            if (!status.empty()) set_status(status.c_str());
        }

        template <typename Fn>
        bool set_status_on_failure(Fn&& fn) {
            FixedString<128> status;
            const bool ok = fn(status);
            if (!ok) {
                set_status_if_present(status);
            }
            return ok;
        }

        void set_info_label(std::string_view value) {
            if (!access.valid() || !handles.info_tag) return;
            if (last_info_text.view() == value && !value.empty()) return;
            last_info_text.assign(value);
            set_label_slot(handles.info_tag, text_slots.info_tag, last_info_text.c_str());
        }

        bool is_playing() const noexcept { return playback.playing(); }
        bool is_paused() const noexcept { return playback.paused(); }
        bool has_active_playback() const noexcept { return is_playing() || is_paused(); }
        void sync_active_playback_ui(bool count_play, bool reset_progress = false) {
            set_play_button_text(true);
            if (reset_progress) {
                sync_progress_value(0);
            }
            begin_weekly_listening_session(count_play);
        }

        void sync_inactive_playback_ui(bool reset_timeline = false) {
            end_weekly_listening_session();
            set_play_button_text(false);
            if (reset_timeline) {
                set_time_label(0);
                sync_progress_value(0);
            }
        }

        bool has_cover_image() const noexcept { return ::ui::scene::image_id_valid(current_cover.image_id); }
        void sync_now_cover_plate_surface() noexcept {
            if (!access.valid() || !handles.now_cover_plate) return;
            const bool has_cover = has_cover_image();
            const rgba plate_bg = has_cover
                ? blend_on(now_playing_roles.cover_plate.bg, now_playing_roles.page_backdrop, 34)
                : now_playing_roles.cover_plate.bg;
            access.set_style_override(handles.now_cover_plate,
                                      make_now_cover_plate_patch(plate_bg, 48, has_cover));
        }

        void apply_surface_role(WidgetHandle h, const player::cover_theme::SurfaceRole& role) noexcept {
            if (!access.valid() || !h) return;
            access.set_style_override(h, ::ui::scene::make_surface_recolor_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_font_color = true,
                .bg_color = role.bg,
                .border_color = role.border,
                .font_color = role.fg,
            }));
        }

        void apply_text_color(WidgetHandle h, const rgba& color) noexcept {
            if (!access.valid() || !h) return;
            StylePatch patch{};
            patch.has_font_color = true;
            patch.font_color = color;
            access.set_style_override(h, patch);
        }

        static ::ui::scene::ImageSlotHandles make_image_slot_handles(WidgetHandle image,
                                                                      WidgetHandle plate = {}) noexcept {
            return ::ui::scene::ImageSlotHandles{image, plate};
        }

        auto current_track_cover_slots() const noexcept {
            return std::array<::ui::scene::ImageSlotHandles, 3>{
                make_image_slot_handles(handles.cover),
                make_image_slot_handles(handles.bottom_cover),
                make_image_slot_handles(handles.transition_cover),
            };
        }

        ::ui::scene::ImageSlotHandles library_context_cover_slot() const noexcept {
            return make_image_slot_handles(handles.list_context_cover);
        }

        ::ui::scene::ImageSlotHandles library_info_cover_slot() const noexcept {
            return make_image_slot_handles(handles.list_info_cover);
        }

        void clear_image_slot_if_present(const ::ui::scene::ImageSlotHandles& slot,
                                         bool show_plate_when_empty = false) noexcept {
            if (!access.valid() || !slot.image || access.kind(slot.image) != WidgetKind::Image) return;
            access.clear_image_slot(slot, show_plate_when_empty);
        }

        void set_image_slot_if_present(const ::ui::scene::ImageSlotHandles& slot,
                                       ::ui::scene::ImageId image,
                                       bool show_plate_when_empty = false) noexcept {
            if (!access.valid() || !slot.image || access.kind(slot.image) != WidgetKind::Image) return;
            access.set_image_slot(slot, image, show_plate_when_empty);
        }

        template <std::size_t N>
        void clear_image_slot_range(const std::array<::ui::scene::ImageSlotHandles, N>& slots,
                                    bool show_plate_when_empty = false) noexcept {
            for (const auto& slot : slots) {
                clear_image_slot_if_present(slot, show_plate_when_empty);
            }
        }

        template <std::size_t N>
        void set_image_slot_range(const std::array<::ui::scene::ImageSlotHandles, N>& slots,
                                  ::ui::scene::ImageId image,
                                  bool show_plate_when_empty = false) noexcept {
            for (const auto& slot : slots) {
                set_image_slot_if_present(slot, image, show_plate_when_empty);
            }
        }

        void show_default_current_cover() noexcept {
            release_resolved_cover(current_cover);
            set_image_slot_range(
                current_track_cover_slots(),
                default_cover_image_id(DefaultCoverVariant::HomeHeroPill),
                false);
        }

        void reset_cover_image() noexcept {
            cover_ready = false;
            cover_path.clear();
            cover_embedded_path.clear();
            cover_folder_path.clear();
            cover_failed_embedded_path.clear();
            cover_failed_folder_path.clear();
            release_resolved_cover(current_cover);
            clear_image_slot_range(current_track_cover_slots(), false);
        }

        void update_cover_image() {
            if (!access.valid()) return;
            if (!cover_ready || (cover_embedded_path.empty() && cover_folder_path.empty())) {
                show_default_current_cover();
                apply_cover_theme_from_current_image();
                restore_now_playing_group_visibility();
                restore_bottom_bar_content_visibility();
                sync_now_playing_transition_overlay();
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] no cover for track\n");
#endif
                return;
            }
            auto try_load = [&](std::string_view candidate) -> bool {
                if (candidate.empty()) return false;
                const bool embedded_candidate = candidate == cover_embedded_path.view();
                const bool folder_candidate = candidate == cover_folder_path.view();
                if ((embedded_candidate && cover_failed_embedded_path.view() == candidate)
                    || (folder_candidate && cover_failed_folder_path.view() == candidate)) {
                    return false;
                }
                if (current_cover.key.view() == candidate && ::ui::scene::image_id_valid(current_cover.image_id)) {
                    set_image_slot_range(current_track_cover_slots(), current_cover.image_id, false);
                    cover_path.assign(candidate);
                    restore_now_playing_group_visibility();
                    restore_bottom_bar_content_visibility();
                    sync_now_playing_transition_overlay();
                    if (!cover_theme_tint_source_matches(current_cover.key.view())) {
                        apply_cover_theme_from_current_image(current_cover.key.view());
#if defined(CHARM_PLAYER_COVER_DEBUG)
                        debug_cover_theme(cover_theme, current_cover.key.view());
#endif
                    }
                    if (embedded_candidate) cover_failed_embedded_path.clear();
                    if (folder_candidate) cover_failed_folder_path.clear();
                    return true;
                }
                CoverResourceRequest request{};
                request.path = candidate;
                request.kind = embedded_candidate
                    ? CoverResourceKind::EmbeddedTrack
                    : (folder_candidate ? CoverResourceKind::FolderFile : CoverResourceKind::Unknown);
                request.fallback_variant = DefaultCoverVariant::HomeHeroPill;
                if (resolve_cover(request, current_cover)) {
                    set_image_slot_range(current_track_cover_slots(), current_cover.image_id, false);
                    cover_path.assign(candidate);
                    restore_now_playing_group_visibility();
                    restore_bottom_bar_content_visibility();
                    sync_now_playing_transition_overlay();
                    apply_cover_theme_from_current_image(current_cover.key.view());
#if defined(CHARM_PLAYER_COVER_DEBUG)
                    debug_cover_theme(cover_theme, current_cover.key.view());
#endif
                    if (embedded_candidate) cover_failed_embedded_path.clear();
                    if (folder_candidate) cover_failed_folder_path.clear();
                    return true;
                }
                if (embedded_candidate) cover_failed_embedded_path.assign(candidate);
                if (folder_candidate) cover_failed_folder_path.assign(candidate);
                return false;
            };

            bool loaded = false;
            switch (cover_strategy) {
            case CoverStrategy::embedded_only:
                loaded = try_load(cover_embedded_path.view());
                break;
            case CoverStrategy::folder_only:
                loaded = try_load(cover_folder_path.view());
                break;
            case CoverStrategy::folder_first:
                loaded = try_load(cover_folder_path.view());
                if (!loaded) loaded = try_load(cover_embedded_path.view());
                break;
            case CoverStrategy::embedded_first:
            default:
                loaded = try_load(cover_embedded_path.view());
                if (!loaded) loaded = try_load(cover_folder_path.view());
                break;
            }

            if (loaded) return;
            show_default_current_cover();
            apply_cover_theme_from_current_image();
            restore_now_playing_group_visibility();
            restore_bottom_bar_content_visibility();
            sync_now_playing_transition_overlay();
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] load failed: %s\n", cover_path.c_str());
#endif
        }

        void on_player_error(const char* text) {
            finish_player_shutdown(text);
        }

        void finish_player_shutdown(const char* text) {
            playback.stop_playback();
            set_status(text);
            sync_inactive_playback_ui();
        }

        void handle_player_run_state_drop(const audio::AudioPlayer& player) {
            if (player.state() != audio::PlayerState::error && has_active_playback()) {
                handle_track_end();
            }
        }

        void sync_player_runtime_status(const audio::AudioPlayer& player) {
            if (is_paused()) return;
            const auto st = player.state();
            if (st == audio::PlayerState::opening) {
                set_status("Opening");
            } else if (st == audio::PlayerState::buffering) {
                set_status("Buffering");
            } else if (st == audio::PlayerState::playing) {
                set_status("Playing");
            }
        }

        void tick_player(const audio::AudioPlayer& player) {
            const bool running_now = player.is_running();
            if (last_running && !running_now) {
                handle_player_run_state_drop(player);
            }
            last_running = running_now;
            sync_player_runtime_status(player);
            if (player.state() == audio::PlayerState::error) {
                const auto err = player.last_error();
                const auto stage = player.last_error_stage();
                char buf[96]{};
                std::snprintf(buf, sizeof(buf), "Player error (%s/%s)",
                              player::audio_err_text(err), player::audio_stage_text(stage));
                on_player_error(buf);
            }
            tick_weekly_listening_stats();
#if CHARM_PLAYER_LAYERED_TRANSITIONS
            if (now_playing_transition.active) {
                tick_now_playing_transition(charm::system::ClockCaps::TimeSource::now());
                return;
            }
#endif
            refresh_page(current_page);
        }

        void refresh_home() {
            sync_page_chrome(PlayerPage::Home);
            refresh_home_body_content();
            update_debug_overlay();
        }

        #include "player.controller.library.inc"

        bool handle_probe_key_action() {
            if (current_page != PlayerPage::Probe) return false;
            dismiss_probe();
            return true;
        }

        bool handle_page_modal_key_action(UiKey key) {
            if (current_page != PlayerPage::Library) return false;
            if (handle_list_info_popup_key(key)) return true;
            return handle_list_action_menu_key(key);
        }

        void nav_list_with_focus(int delta) {
            focus_list();
            nav_list(delta);
        }

        bool handle_list_navigation_key(UiKey key) {
            switch (key) {
            case UiKey::Up:
                nav_list_with_focus(-1);
                return true;
            case UiKey::Down:
                nav_list_with_focus(1);
                return true;
            case UiKey::Enter:
                focus_list();
                nav_list_activate();
                return true;
            default:
                return false;
            }
        }

        bool handle_page_back_key(UiKey key) {
            if (key != UiKey::Left) return false;
            if (current_page == PlayerPage::Library) {
                reset_library_context();
            }
            return true;
        }

        void toggle_playback_from_key() {
            if (is_playing()) pause_playback();
            else if (is_paused()) resume_playback();
            else start_playback();
        }

        bool handle_transport_key(UiKey key) {
            switch (key) {
            case UiKey::PlayToggle:
                toggle_playback_from_key();
                return true;
            case UiKey::Next:
                switch_track(1);
                return true;
            case UiKey::Prev:
                switch_track(-1);
                return true;
            case UiKey::Mode:
                cycle_play_mode();
                return true;
            default:
                return false;
            }
        }

        void handle_key_action(UiKey key) {
            if (handle_probe_key_action()) return;
            if (handle_page_modal_key_action(key)) return;
            if (handle_list_navigation_key(key)) return;
            if (handle_page_back_key(key)) return;
            (void)handle_transport_key(key);
        }

        void handle_input_command(PlayerInputCommand command) {
            switch (command) {
            case PlayerInputCommand::Up:
                handle_key_action(UiKey::Up);
                break;
            case PlayerInputCommand::Down:
                handle_key_action(UiKey::Down);
                break;
            case PlayerInputCommand::Left:
            case PlayerInputCommand::Back:
                handle_key_action(UiKey::Left);
                break;
            case PlayerInputCommand::Enter:
                handle_key_action(UiKey::Enter);
                break;
            case PlayerInputCommand::PlayToggle:
                handle_key_action(UiKey::PlayToggle);
                break;
            case PlayerInputCommand::Next:
                handle_key_action(UiKey::Next);
                break;
            case PlayerInputCommand::Prev:
                handle_key_action(UiKey::Prev);
                break;
            case PlayerInputCommand::Mode:
                handle_key_action(UiKey::Mode);
                break;
            default:
                break;
            }
        }

        #include "player.controller.progress.inc"
        #include "player.controller.input_debug.inc"
        void seed_rng() {
            if (rng_state != 0) return;
            auto seed = static_cast<std::uint32_t>(charm::system::ClockCaps::TimeSource::now());
            if (seed == 0) seed = 0xA341316Cu;
            rng_state = seed;
        }

        std::uint32_t next_rng() {
            seed_rng();
            std::uint32_t x = rng_state;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            rng_state = x;
            return x;
        }

        int rand_index(int max) {
            if (max <= 0) return 0;
            return static_cast<int>(next_rng() % static_cast<std::uint32_t>(max));
        }
    };

    struct PlayerControllerMemoryProfile {
        std::size_t controller_size_bytes{0};
        std::size_t track_capacity{0};
        std::size_t list_order_bytes{0};
        std::size_t list_rows_bytes{0};
        std::size_t row_scratch_bytes{0};
        std::size_t duration_cache_bytes{0};
        std::size_t list_cover_paths_bytes{0};
        std::size_t cover_path_scratch_bytes{0};
        std::size_t list_cover_cache_capacity{0};
        std::size_t list_cover_cache_bytes{0};
        std::size_t current_cover_bytes{0};
        std::size_t cover_theme_bytes{0};
        std::size_t now_playing_roles_bytes{0};
        std::size_t text_state_bytes{0};
        std::size_t cover_path_state_bytes{0};
        std::size_t font_path_state_bytes{0};
        std::size_t weekly_stats_bytes{0};
        std::size_t weekly_history_bytes{0};
        std::size_t ui_handles_bytes{0};
        std::size_t icon_ids_bytes{0};
        std::size_t playback_engine_bytes{0};
    };

    inline constexpr PlayerControllerMemoryProfile player_controller_memory_profile() noexcept {
        return PlayerControllerMemoryProfile{
            sizeof(PlayerController),
            kMaxTracks,
            sizeof(std::declval<PlayerController>().list_order),
            0,
            sizeof(std::declval<PlayerController>().list_row_title_scratch)
                + sizeof(std::declval<PlayerController>().list_row_subtitle_scratch)
                + sizeof(std::declval<PlayerController>().list_row_tail_scratch),
            sizeof(std::declval<PlayerController>().track_duration_cache_sec),
            0,
            sizeof(std::declval<PlayerController>().list_cover_path_scratch),
            PlayerController::kListCoverCache,
            PlayerController::kListCoverCache
                * sizeof(PlayerController::ListCoverCacheEntry),
            sizeof(std::declval<PlayerController>().current_cover),
            sizeof(std::declval<PlayerController>().cover_theme),
            sizeof(std::declval<PlayerController>().now_playing_roles),
            sizeof(std::declval<PlayerController>().title_text)
                + sizeof(std::declval<PlayerController>().subtitle_text)
                + sizeof(std::declval<PlayerController>().track_format_text)
                + sizeof(std::declval<PlayerController>().last_status_text)
                + sizeof(std::declval<PlayerController>().last_mode_text)
                + sizeof(std::declval<PlayerController>().last_list_title_text)
                + sizeof(std::declval<PlayerController>().last_list_hint_text)
                + sizeof(std::declval<PlayerController>().last_debug_text)
                + sizeof(std::declval<PlayerController>().last_info_text)
                + sizeof(std::declval<PlayerController>().last_home_stats_total_text)
                + sizeof(std::declval<PlayerController>().last_home_stats_plays_text)
                + sizeof(std::declval<PlayerController>().last_home_stats_avg_text)
#if CHARM_PLAYER_LYRICS
                + sizeof(std::declval<PlayerController>().lyrics_prev_text)
                + sizeof(std::declval<PlayerController>().lyrics_current_text)
                + sizeof(std::declval<PlayerController>().lyrics_next_text)
                + sizeof(std::declval<PlayerController>().lyrics_status_text)
#endif
                + sizeof(std::declval<PlayerController>().library_context_key)
                + sizeof(std::declval<PlayerController>().mount_status),
            sizeof(std::declval<PlayerController>().cover_path)
                + sizeof(std::declval<PlayerController>().cover_embedded_path)
                + sizeof(std::declval<PlayerController>().cover_folder_path)
                + sizeof(std::declval<PlayerController>().cover_failed_embedded_path)
                + sizeof(std::declval<PlayerController>().cover_failed_folder_path)
                + sizeof(std::declval<PlayerController>().cover_tint_path),
            sizeof(std::declval<PlayerController>().font_ttf_path)
                + sizeof(std::declval<PlayerController>().font_fallback_ttf_path),
            sizeof(std::declval<PlayerController>().weekly_listening_stats),
            sizeof(std::declval<PlayerController>().weekly_listening_history),
            sizeof(std::declval<PlayerController>().handles),
            sizeof(std::declval<PlayerController>().icons),
            sizeof(std::declval<PlayerController>().playback),
        };
    }

#if defined(CHARM_PLAYER_MCU) && CHARM_PLAYER_MCU && defined(__GNUC__)
    extern "C" [[gnu::used]] void charm_player_controller_memory_profile_symbols() noexcept {
        constexpr auto profile = player_controller_memory_profile();
        asm volatile(
            ".global charm_player_controller_profile_controller_size_bytes\n"
            ".set charm_player_controller_profile_controller_size_bytes, %c0\n"
            ".global charm_player_controller_profile_track_capacity\n"
            ".set charm_player_controller_profile_track_capacity, %c1\n"
            ".global charm_player_controller_profile_list_order_bytes\n"
            ".set charm_player_controller_profile_list_order_bytes, %c2\n"
            ".global charm_player_controller_profile_list_rows_bytes\n"
            ".set charm_player_controller_profile_list_rows_bytes, %c3\n"
            ".global charm_player_controller_profile_row_scratch_bytes\n"
            ".set charm_player_controller_profile_row_scratch_bytes, %c4\n"
            ".global charm_player_controller_profile_duration_cache_bytes\n"
            ".set charm_player_controller_profile_duration_cache_bytes, %c5\n"
            ".global charm_player_controller_profile_list_cover_paths_bytes\n"
            ".set charm_player_controller_profile_list_cover_paths_bytes, %c6\n"
            ".global charm_player_controller_profile_cover_path_scratch_bytes\n"
            ".set charm_player_controller_profile_cover_path_scratch_bytes, %c7\n"
            ".global charm_player_controller_profile_list_cover_cache_capacity\n"
            ".set charm_player_controller_profile_list_cover_cache_capacity, %c8\n"
            ".global charm_player_controller_profile_list_cover_cache_bytes\n"
            ".set charm_player_controller_profile_list_cover_cache_bytes, %c9\n"
            ".global charm_player_controller_profile_current_cover_bytes\n"
            ".set charm_player_controller_profile_current_cover_bytes, %c10\n"
            ".global charm_player_controller_profile_cover_theme_bytes\n"
            ".set charm_player_controller_profile_cover_theme_bytes, %c11\n"
            ".global charm_player_controller_profile_now_playing_roles_bytes\n"
            ".set charm_player_controller_profile_now_playing_roles_bytes, %c12\n"
            ".global charm_player_controller_profile_text_state_bytes\n"
            ".set charm_player_controller_profile_text_state_bytes, %c13\n"
            ".global charm_player_controller_profile_cover_path_state_bytes\n"
            ".set charm_player_controller_profile_cover_path_state_bytes, %c14\n"
            ".global charm_player_controller_profile_font_path_state_bytes\n"
            ".set charm_player_controller_profile_font_path_state_bytes, %c15\n"
            ".global charm_player_controller_profile_weekly_stats_bytes\n"
            ".set charm_player_controller_profile_weekly_stats_bytes, %c16\n"
            ".global charm_player_controller_profile_weekly_history_bytes\n"
            ".set charm_player_controller_profile_weekly_history_bytes, %c17\n"
            ".global charm_player_controller_profile_ui_handles_bytes\n"
            ".set charm_player_controller_profile_ui_handles_bytes, %c18\n"
            ".global charm_player_controller_profile_icon_ids_bytes\n"
            ".set charm_player_controller_profile_icon_ids_bytes, %c19\n"
            ".global charm_player_controller_profile_playback_engine_bytes\n"
            ".set charm_player_controller_profile_playback_engine_bytes, %c20\n"
            :
            : "i"(profile.controller_size_bytes),
              "i"(profile.track_capacity),
              "i"(profile.list_order_bytes),
              "i"(profile.list_rows_bytes),
              "i"(profile.row_scratch_bytes),
              "i"(profile.duration_cache_bytes),
              "i"(profile.list_cover_paths_bytes),
              "i"(profile.cover_path_scratch_bytes),
              "i"(profile.list_cover_cache_capacity),
              "i"(profile.list_cover_cache_bytes),
              "i"(profile.current_cover_bytes),
              "i"(profile.cover_theme_bytes),
              "i"(profile.now_playing_roles_bytes),
              "i"(profile.text_state_bytes),
              "i"(profile.cover_path_state_bytes),
              "i"(profile.font_path_state_bytes),
              "i"(profile.weekly_stats_bytes),
              "i"(profile.weekly_history_bytes),
              "i"(profile.ui_handles_bytes),
              "i"(profile.icon_ids_bytes),
              "i"(profile.playback_engine_bytes));
    }
#endif
}
