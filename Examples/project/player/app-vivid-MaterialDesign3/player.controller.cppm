module;
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

export module player.controller;

import player.fixed_string;
import player.mcu_policy;
import audio.eq;
import audio.player;
import audio.result;
import alg_color_extract;
import charm.core.event;
import charm.core.geometry;
import charm.core.handle;
import charm.gfx.color;
import charm.gfx.image;
import charm.ui.scene;
import charm.ui.scene.anchored_menu;
import charm.ui.scene.pill_surface;
import charm.font.typography;
import charm.system.clock;
import player.playback;
import player.fs_utils;
import player.stats_history;
import player.storage;
import player.track_probe;
import player.ui;
import player.cover;
import player.cover_theme;
import player.font_cache;
import charm.widgets.perf_overlay;
import charm.core.style_sheet;
#if defined(CHARM_AUDIO_USE_VFS)
import audio.source.fs;
#else
import audio.source.file;
#endif

inline constexpr bool kPlayerControllerMcuGuard =
    (player::mcu_policy::guard("player.controller uses std::string/std::vector; port before MCU build."), true);

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
        std::array<WidgetHandle, 3> home_daily_mix_covers{};
        std::array<WidgetHandle, 3> home_recently_played_covers{};
        WidgetHandle now_back{};
        WidgetHandle now_more{};
        WidgetHandle now_lyrics{};
        WidgetHandle now_backdrop{};
        WidgetHandle now_top_bar{};
        WidgetHandle now_cover_plate{};
        WidgetHandle now_text_group{};
        WidgetHandle now_progress_group{};
        WidgetHandle now_aux_group{};
        WidgetHandle cover{};
        WidgetHandle cover_left{};
        WidgetHandle cover_right{};
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
        WidgetHandle list_context_cover{};
        WidgetHandle list_context_cover_scrim{};
        WidgetHandle list_action_scrim{};
        WidgetHandle list_action_card{};
        WidgetHandle list_action_title{};
        std::array<WidgetHandle, 3> list_action_items{};
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
        struct LibraryRowModel {
            FixedString<192> title{};
            FixedString<128> subtitle{};
            FixedString<32> tail{};
            bool group_row{false};
        };

        struct LibraryRowRecipe {
            const LibraryRowModel* model{};
            int row_index{-1};
            int track_index{-1};
            bool group_row{false};
            bool current_row{false};
            bool selected_row{false};
            bool show_tail_action{false};
            const char* menu_title_cstr{"Track"};
            std::string_view detail_primary{};
            std::string_view detail_secondary{};
            std::string_view track_path{};
            std::string_view cover_path{};
            ::ui::scene::ImageId tail_icon{};
            ::ui::scene::ImageId tail_action_icon{};
            ::ui::scene::ImageId fallback_icon{};
            bool prefer_cover{false};

            std::string_view title() const noexcept {
                return model ? model->title.view() : std::string_view{};
            }

            std::string_view subtitle() const noexcept {
                return model ? model->subtitle.view() : std::string_view{};
            }

            std::string_view tail() const noexcept {
                return model ? model->tail.view() : std::string_view{};
            }

            const char* title_c_str() const noexcept {
                return model ? model->title.c_str() : "";
            }

            const char* subtitle_c_str() const noexcept {
                return model ? model->subtitle.c_str() : "";
            }

            const char* tail_c_str() const noexcept {
                return model ? model->tail.c_str() : "";
            }
        };

        PlaybackEngine playback{};
        ::ui::scene::SceneAccess access{};
        UiHandles handles{};
        PlayerIconIds icons{};
        int last_time_sec{-1};
        StorageView storage{};
        int track_index{0};
        FixedString<192> title_text{};
        FixedString<64> subtitle_text{};
        FixedString<260> cover_path{};
        FixedString<260> cover_embedded_path{};
        FixedString<260> cover_folder_path{};
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
        std::uint64_t track_size_bytes{0};
        CoverImage cover_image{};
        cover_theme::CoverTheme cover_theme{};
        bool cover_ready{false};
        CoverStrategy cover_strategy{CoverStrategy::embedded_first};
        cover_theme::CoverThemeMode cover_theme_mode{cover_theme::CoverThemeMode::primary_container};
        alg::PaletteStyle cover_palette_style{alg::PaletteStyle::tonal_spot};
        bool fs_ready{false};
        bool track_preloaded{false};
        int preloaded_duration_sec{0};
        int play_mode{0};
        int last_play_button_state{-1};
        int last_list_count{-1};
        bool ignore_list_select{false};
        int last_list_selected{-1};
        int list_action_menu_index{-1};
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
        std::vector<int> list_order{};
        std::vector<LibraryRowModel> list_rows{};
        std::vector<int> track_duration_cache_sec{};
        std::size_t list_duration_probe_cursor{0};
        std::uint64_t last_list_duration_probe_ms{0};
        std::vector<FixedString<260>> list_cover_paths{};
        struct ListCoverCacheEntry {
            FixedString<260> path{};
            CoverImage image{};
        };
        static constexpr std::size_t kHomeCollageSlots = 6;
        static constexpr std::size_t kListCoverCache = 12;
        std::array<ListCoverCacheEntry, kListCoverCache> list_cover_cache{};
        std::size_t list_cover_next{0};
        std::array<int, kHomeCollageSlots> home_collage_track_indices{{-1, -1, -1, -1, -1, -1}};
        std::array<int, kHomeCollageSlots> last_home_collage_track_indices{{-2, -2, -2, -2, -2, -2}};
        bool progress_dragging{false};
        int progress_drag_value{0};
        int progress_drag_sec{0};
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
        std::string font_ttf_path{};
        std::string font_fallback_ttf_path{};
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

        cover_theme::CoverTheme derive_cover_theme(const CoverImage& img) noexcept {
            cover_theme::CoverThemeConfig config{};
            config.mode = cover_theme_mode;
            config.is_dark = true;
            config.palette_style = cover_palette_style;
            config.fallback = kUiBackdropBase;
            return cover_theme::compute_cover_theme(img, config);
        }

        static const char* palette_style_name(alg::PaletteStyle style) noexcept {
            switch (style) {
            case alg::PaletteStyle::vibrant: return "vibrant";
            case alg::PaletteStyle::expressive: return "expressive";
            case alg::PaletteStyle::fruit_salad: return "fruit_salad";
            case alg::PaletteStyle::tonal_spot:
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
            if (!cover_image.path.empty()) {
                cover_theme = derive_cover_theme(cover_image);
                cover_tint_path.assign(cover_image.path);
                apply_now_theme(cover_theme);
            }
        }

        void cycle_palette_style() noexcept {
            switch (cover_palette_style) {
            case alg::PaletteStyle::tonal_spot:
                cover_palette_style = alg::PaletteStyle::vibrant;
                break;
            case alg::PaletteStyle::vibrant:
                cover_palette_style = alg::PaletteStyle::expressive;
                break;
            case alg::PaletteStyle::expressive:
                cover_palette_style = alg::PaletteStyle::fruit_salad;
                break;
            case alg::PaletteStyle::fruit_salad:
            default:
                cover_palette_style = alg::PaletteStyle::tonal_spot;
                break;
            }
            cover_tint_path.clear();
            if (!cover_image.path.empty()) {
                cover_theme = derive_cover_theme(cover_image);
                cover_tint_path.assign(cover_image.path);
                apply_now_theme(cover_theme);
            }
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] palette=%s\n", palette_style_name(cover_palette_style));
#endif
        }

        PlayerPage current_page{PlayerPage::Home};
        PlayerPage start_page{PlayerPage::Home};
        PlayerPage default_page{PlayerPage::Home};
        PlayerPage return_page{PlayerPage::Home};
        ::ui::scene::PageLayer page_probe_layer{};
        ::ui::scene::PageLayer page_now_layer{};
        ::ui::scene::PageLayer page_library_layer{};
        ::ui::scene::PageLayer page_home_layer{};
        static constexpr std::uint64_t kNowPlayingExpandDurationMs = 360;
        enum class NowPlayingTransitionDirection : std::uint8_t {
            Expand,
            Collapse,
        };
        struct NowPlayingExpandTransition {
            bool active{false};
            bool reveal_started{false};
            NowPlayingTransitionDirection direction{NowPlayingTransitionDirection::Expand};
            PlayerPage source_page{PlayerPage::Home};
            PlayerPage destination_page{PlayerPage::NowPlaying};
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

        void bind_scene(::ui::scene::Scene& scene) {
            access = scene.access();
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

        void set_page_root_visible(PlayerPage page, bool visible) noexcept {
            if (!access.valid()) return;
            switch (page) {
            case PlayerPage::Home:
                if (page_home_layer.root()) page_home_layer.set_visible(access, visible);
                else if (handles.page_home) access.set_visible(handles.page_home, visible);
                break;
            case PlayerPage::NowPlaying:
                if (page_now_layer.root()) page_now_layer.set_visible(access, visible);
                else if (handles.page_now_playing) access.set_visible(handles.page_now_playing, visible);
                break;
            case PlayerPage::Library:
                if (page_library_layer.root()) page_library_layer.set_visible(access, visible);
                else if (handles.page_library) access.set_visible(handles.page_library, visible);
                break;
            case PlayerPage::Probe:
                if (page_probe_layer.root()) page_probe_layer.set_visible(access, visible);
                else if (handles.page_probe) access.set_visible(handles.page_probe, visible);
                break;
            }
        }

        static bool is_mini_bar_page(PlayerPage page) noexcept {
            return page == PlayerPage::Home || page == PlayerPage::Library;
        }

        void refresh_page_for_transition(PlayerPage page) {
            switch (page) {
            case PlayerPage::Home:
                refresh_home();
                update_nav_page_indicator_for(PlayerPage::Home);
                break;
            case PlayerPage::Library:
                refresh_library();
                update_nav_page_indicator_for(PlayerPage::Library);
                break;
            case PlayerPage::NowPlaying:
                refresh_now_playing();
                set_time_label(playback.current_sec());
                break;
            case PlayerPage::Probe:
            default:
                break;
            }
        }
        #include "player.controller.now_playing.inc"

        #include "player.controller.pages.inc"
        #include "player.controller.home.inc"

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

        void set_info_label(std::string_view value) {
            if (!access.valid() || !handles.info_tag) return;
            if (last_info_text.view() == value) return;
            last_info_text.assign(value);
            set_label_slot(handles.info_tag, text_slots.info_tag, last_info_text.c_str());
        }

        bool is_playing() const noexcept { return playback.playing(); }
        bool is_paused() const noexcept { return playback.paused(); }

        void reset_cover_image() noexcept {
            cover_ready = false;
            cover_path.clear();
            cover_embedded_path.clear();
            cover_folder_path.clear();
            release_cover_image(cover_image);
            if (!access.valid()) return;
            const auto clear_image = [&](WidgetHandle handle) {
                if (handle && access.kind(handle) == WidgetKind::Image) {
                    access.set_image(handle, ::ui::scene::invalid_image_id());
                }
            };
            clear_image(handles.cover);
            clear_image(handles.cover_left);
            clear_image(handles.cover_right);
            clear_image(handles.bottom_cover);
            clear_image(handles.transition_cover);
        }

        void update_cover_image() {
            if (!access.valid()) return;
            if (!cover_ready || (cover_embedded_path.empty() && cover_folder_path.empty())) {
                release_cover_image(cover_image);
                cover_tint_path.clear();
                cover_theme = derive_cover_theme(cover_image);
                apply_now_theme(cover_theme);
                const auto clear_image = [&](WidgetHandle handle) {
                    if (handle && access.kind(handle) == WidgetKind::Image) {
                        access.set_image(handle, ::ui::scene::invalid_image_id());
                    }
                };
                clear_image(handles.cover);
                clear_image(handles.cover_left);
                clear_image(handles.cover_right);
                clear_image(handles.bottom_cover);
                clear_image(handles.transition_cover);
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] no cover for track\n");
#endif
                return;
            }
            auto try_load = [&](std::string_view candidate) -> bool {
                if (candidate.empty()) return false;
                if (cover_image.path == candidate && ::ui::scene::image_id_valid(cover_image.image_id)) {
                    const auto set_image = [&](WidgetHandle handle) {
                        if (handle && access.kind(handle) == WidgetKind::Image) {
                            access.set_image(handle, cover_image.image_id);
                        }
                    };
                    set_image(handles.cover);
                    set_image(handles.cover_left);
                    set_image(handles.cover_right);
                    set_image(handles.bottom_cover);
                    set_image(handles.transition_cover);
                    cover_path.assign(candidate);
                    if (cover_tint_path.view() != cover_image.path) {
                        cover_theme = derive_cover_theme(cover_image);
                        cover_tint_path.assign(cover_image.path);
                        apply_now_theme(cover_theme);
#if defined(CHARM_PLAYER_COVER_DEBUG)
                        debug_cover_theme(cover_theme, cover_image.path);
#endif
                    }
                    return true;
                }
                if (load_cover_image(candidate, cover_image)) {
                    const auto set_image = [&](WidgetHandle handle) {
                        if (handle && access.kind(handle) == WidgetKind::Image) {
                            access.set_image(handle, cover_image.image_id);
                        }
                    };
                    set_image(handles.cover);
                    set_image(handles.cover_left);
                    set_image(handles.cover_right);
                    set_image(handles.bottom_cover);
                    set_image(handles.transition_cover);
                    cover_path.assign(candidate);
                    cover_theme = derive_cover_theme(cover_image);
                    cover_tint_path.assign(cover_image.path);
                    apply_now_theme(cover_theme);
#if defined(CHARM_PLAYER_COVER_DEBUG)
                    debug_cover_theme(cover_theme, cover_image.path);
#endif
                    return true;
                }
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
            const auto clear_image = [&](WidgetHandle handle) {
                if (handle && access.kind(handle) == WidgetKind::Image) {
                    access.set_image(handle, ::ui::scene::invalid_image_id());
                }
            };
            clear_image(handles.cover);
            clear_image(handles.cover_left);
            clear_image(handles.cover_right);
            clear_image(handles.bottom_cover);
            clear_image(handles.transition_cover);
            cover_tint_path.clear();
            cover_theme = derive_cover_theme(cover_image);
            apply_now_theme(cover_theme);
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] load failed: %s\n", cover_path.c_str());
#endif
        }

        void on_player_stopped() {
            end_weekly_listening_session();
            playback.stop_playback();
            set_status("Stopped");
            set_play_button_text(false);
        }

        void on_player_error(const char* text) {
            end_weekly_listening_session();
            playback.stop_playback();
            set_status(text);
            set_play_button_text(false);
        }

        void tick_player(const audio::AudioPlayer& player) {
            const bool running_now = player.is_running();
            if (last_running && !running_now) {
                if (player.state() == audio::PlayerState::error) {
                    on_player_stopped();
                } else if (playback.playing() || playback.paused()) {
                    handle_track_end();
                }
            }
            last_running = running_now;
            if (!playback.paused()) {
                const auto st = player.state();
                if (st == audio::PlayerState::opening) {
                    set_status("Opening");
                } else if (st == audio::PlayerState::buffering) {
                    set_status("Buffering");
                } else if (st == audio::PlayerState::playing) {
                    set_status("Playing");
                }
            }
            if (player.state() == audio::PlayerState::error) {
                const auto err = player.last_error();
                const auto stage = player.last_error_stage();
                char buf[96]{};
                std::snprintf(buf, sizeof(buf), "Player error (%s/%s)",
                              player::audio_err_text(err), player::audio_stage_text(stage));
                on_player_error(buf);
            }
            tick_weekly_listening_stats();
            if (now_playing_transition.active) {
                tick_now_playing_transition(charm::system::ClockCaps::TimeSource::now());
                return;
            }
            if (current_page == PlayerPage::Library) {
                refresh_library();
            } else if (current_page == PlayerPage::Home) {
                refresh_home();
            } else {
                refresh_now_playing();
            }
        }

        void refresh_home() {
            if (handles.nav_bar) {
                access.set_visible(handles.nav_bar, true);
            }
            if (handles.bottom_bar) {
                access.set_visible(handles.bottom_bar, true);
            }
            update_nav_page_indicator();
            update_duration_from_player();
            update_info_label();
            sync_home_collage_images();
            sync_weekly_listening_stats_card();
            update_debug_overlay();
        }

        #include "player.controller.library.inc"

        void handle_key_action(UiKey key) {
            if (current_page == PlayerPage::Probe) {
                dismiss_probe();
                return;
            }
            if (current_page == PlayerPage::Library && handle_list_action_menu_key(key)) {
                return;
            }
            switch (key) {
            case UiKey::Up:
                focus_list();
                nav_list(-1);
                break;
            case UiKey::Down:
                focus_list();
                nav_list(1);
                break;
            case UiKey::Left:
                if (current_page == PlayerPage::Library) {
                    reset_library_context();
                }
                break;
            case UiKey::Enter:
                focus_list();
                nav_list_activate();
                break;
            case UiKey::PlayToggle:
                if (is_playing()) pause_playback();
                else if (is_paused()) resume_playback();
                else start_playback();
                break;
            case UiKey::Next:
                switch_track(1);
                break;
            case UiKey::Prev:
                switch_track(-1);
                break;
            case UiKey::Mode:
                cycle_play_mode();
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
}
