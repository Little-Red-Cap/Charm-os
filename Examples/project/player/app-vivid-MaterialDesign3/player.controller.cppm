module;
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
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
import charm.font.typography;
import charm.system.clock;
import player.playback;
import player.fs_utils;
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
        Home,
        NowPlaying,
        Library,
    };

    enum class ListSort : std::uint8_t {
        NameAsc,
        NameDesc,
    };

    struct UiHandles {
        WidgetHandle page_home{};
        WidgetHandle page_now_playing{};
        WidgetHandle page_library{};
        WidgetHandle root{};
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
        WidgetHandle now_back{};
        WidgetHandle now_more{};
        WidgetHandle now_lyrics{};
        WidgetHandle now_backdrop{};
        WidgetHandle cover{};
        WidgetHandle cover_left{};
        WidgetHandle cover_right{};
        WidgetHandle title{};
        WidgetHandle subtitle{};
        WidgetHandle status{};
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
        WidgetHandle list_title{};
        WidgetHandle list_path{};
        WidgetHandle list_sort{};
        WidgetHandle list_hint{};
        WidgetHandle list_scroll{};
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
        WidgetHandle nav_bar{};
        WidgetHandle nav_home{};
        WidgetHandle nav_search{};
        WidgetHandle nav_library{};
        WidgetHandle cover_debug{};
        WidgetHandle debug_text{};
    };

    struct PlayerController {
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
        FixedString<64> last_list_title_text{};
        FixedString<128> last_list_hint_text{};
        FixedString<128> last_debug_text{};
        FixedString<96> last_info_text{};
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
        ListSort list_sort{ListSort::NameAsc};
        std::vector<int> list_order{};
        std::vector<FixedString<260>> list_cover_paths{};
        struct ListCoverCacheEntry {
            FixedString<260> path{};
            CoverImage image{};
        };
        static constexpr std::size_t kListCoverCache = 12;
        std::array<ListCoverCacheEntry, kListCoverCache> list_cover_cache{};
        std::size_t list_cover_next{0};
        bool progress_dragging{false};
        int progress_drag_value{0};
        int progress_drag_sec{0};
        std::string font_ttf_path{};
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

        void refresh_exact_font_styles() noexcept {
            if (!access.valid()) return;
            const Font& title_font = get_player_font_px(72, FontWeight::Bold);
            const Font& subtitle_font = get_player_font_px(18, FontWeight::Regular);
            std::printf("[font-px] refresh begin title(line=%d base=%d) subtitle(line=%d base=%d)\n",
                        title_font.line_height,
                        title_font.baseline,
                        subtitle_font.line_height,
                        subtitle_font.baseline);
            auto apply_font = [&](WidgetHandle h, const Font& font) {
                if (!h) return;
                StylePatch patch{};
                patch.has_font = true;
                patch.font = &font;
                access.set_style_override(h, patch);
            };
            apply_font(handles.home_title_top, title_font);
            apply_font(handles.home_title_bottom, title_font);
            apply_font(handles.home_subtitle, subtitle_font);
            access.set_text(handles.home_title_top, "Your");
            access.set_text(handles.home_title_bottom, "Mix");
            access.set_text(handles.home_subtitle, "Today's Mix for you");
            std::printf("[font-px] refresh labels reapplied\n");
        }

        void apply_now_theme(const cover_theme::CoverTheme& theme) noexcept {
            if (!access.valid() || !handles.now_backdrop) return;
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = theme.backdrop;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 0;
            access.set_style_override(handles.now_backdrop, patch);

            const rgba title = theme.on_backdrop;
            const rgba subtitle = blend_on(theme.on_backdrop, theme.backdrop, 200);
            const rgba time = blend_on(theme.on_backdrop, theme.backdrop, 170);

            auto apply_label = [&](WidgetHandle h, const rgba& color) {
                if (!h) return;
                StylePatch p{};
                p.has_font_color = true;
                p.font_color = color;
                access.set_style_override(h, p);
            };
            apply_label(handles.title, title);
            apply_label(handles.subtitle, subtitle);
            apply_label(handles.time_left, time);
            apply_label(handles.time_right, time);

            if (handles.bottom_bar) {
                StylePatch bar{};
                bar.has_bg_color = true;
                bar.bg_color = theme.primary_container;
                bar.has_border_color = true;
                bar.border_color = with_alpha(theme.outline_variant, 90);
                access.set_style_override(handles.bottom_bar, bar);
            }

            if (handles.info_tag) {
                StylePatch tag{};
                tag.has_bg_color = true;
                tag.bg_color = theme.surface_high;
                tag.has_border_color = true;
                tag.border_color = with_alpha(theme.outline_variant, 90);
                tag.has_font_color = true;
                tag.font_color = blend_on(theme.on_backdrop, theme.surface_high, 200);
                access.set_style_override(handles.info_tag, tag);
            }

            if (handles.progress) {
                StylePatch prog{};
                prog.has_border_color = true;
                prog.border_color = with_alpha(theme.on_backdrop, 70);
                prog.has_accent_color = true;
                prog.accent_color = theme.primary;
                access.set_style_override(handles.progress, prog);
            }

            auto apply_btn = [&](WidgetHandle h, const rgba& bg, const rgba& border, const rgba& font) {
                if (!h) return;
                StylePatch btn{};
                btn.has_bg_color = true;
                btn.bg_color = bg;
                btn.has_border_color = true;
                btn.border_color = border;
                btn.has_font_color = true;
                btn.font_color = font;
                access.set_style_override(h, btn);
            };
            const rgba top_bg = blend_on(theme.surface_high, theme.backdrop, 140);
            const rgba top_border = blend_on(theme.outline_variant, theme.backdrop, 120);
            const rgba top_font = blend_on(theme.on_surface, theme.backdrop, 220);
            apply_btn(handles.now_back, top_bg, top_border, top_font);
            apply_btn(handles.now_more, top_bg, top_border, top_font);
            apply_btn(handles.now_lyrics, top_bg, top_border, top_font);

            const rgba control_bg = theme.secondary_container;
            const rgba control_border = with_alpha(theme.secondary_container, 0);
            const rgba control_font = theme.on_secondary_container;
            apply_btn(handles.btn_prev, control_bg, control_border, control_font);
            apply_btn(handles.btn_next, control_bg, control_border, control_font);
            apply_btn(handles.btn_mode, control_bg, control_border, control_font);
            apply_btn(handles.btn_pause, theme.primary, theme.primary, theme.on_primary);

#if defined(CHARM_PLAYER_COVER_DEBUG)
            update_cover_debug_label();
#endif
        }

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
        PlayerPage return_page{PlayerPage::Home};
        ::ui::scene::PageLayer page_now_layer{};
        ::ui::scene::PageLayer page_library_layer{};
        ::ui::scene::PageLayer page_home_layer{};
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

        void set_start_page(PlayerPage page) noexcept {
            start_page = page;
        }

        void set_page(PlayerPage page) noexcept {
            if (page == PlayerPage::NowPlaying && current_page != PlayerPage::NowPlaying) {
                return_page = current_page;
            } else if (page != PlayerPage::NowPlaying) {
                return_page = page;
            }
            current_page = page;
            if (!access.valid()) return;
            if (page_home_layer.root()) {
                if (page == PlayerPage::Home) page_home_layer.show(access);
                else page_home_layer.hide(access);
            } else if (handles.page_home) {
                access.set_visible(handles.page_home, page == PlayerPage::Home);
            }
            if (page_now_layer.root()) {
                if (page == PlayerPage::NowPlaying) page_now_layer.show(access);
                else page_now_layer.hide(access);
            } else if (handles.page_now_playing) {
                access.set_visible(handles.page_now_playing, page == PlayerPage::NowPlaying);
            }
            if (page_library_layer.root()) {
                if (page == PlayerPage::Library) page_library_layer.show(access);
                else page_library_layer.hide(access);
            } else if (handles.page_library) {
                access.set_visible(handles.page_library, page == PlayerPage::Library);
            }
            if (handles.debug_text) {
                access.set_visible(handles.debug_text, false);
            }
            if (handles.bottom_bar) {
                const bool show_bottom = (page == PlayerPage::Home || page == PlayerPage::Library);
                access.set_visible(handles.bottom_bar, show_bottom);
            }
            if (handles.nav_bar) {
                const bool show_nav = (page == PlayerPage::Home || page == PlayerPage::Library);
                access.set_visible(handles.nav_bar, show_nav);
            }
            if (page == PlayerPage::Library) {
                refresh_library();
            } else if (page == PlayerPage::Home) {
                refresh_home();
            } else {
                refresh_now_playing();
            }
        }

        static void on_show_home(::ui::scene::SceneAccess& access,
                                 WidgetHandle root,
                                 void* ctx) noexcept {
            (void)access;
            (void)root;
            auto* self = static_cast<PlayerController*>(ctx);
            if (!self) return;
            self->refresh_home();
        }

        static void on_show_now_playing(::ui::scene::SceneAccess& access,
                                        WidgetHandle root,
                                        void* ctx) noexcept {
            (void)access;
            (void)root;
            auto* self = static_cast<PlayerController*>(ctx);
            if (!self) return;
            self->refresh_now_playing();
            self->set_time_label(self->playback.current_sec());
        }

        static void on_show_library(::ui::scene::SceneAccess& access,
                                    WidgetHandle root,
                                    void* ctx) noexcept {
            (void)access;
            (void)root;
            auto* self = static_cast<PlayerController*>(ctx);
            if (!self) return;
            self->refresh_library();
        }

        void init_pages() noexcept {
            if (handles.page_home) {
                page_home_layer.set_root(handles.page_home);
                page_home_layer.set_hooks(::ui::scene::PageHooks{
                    .on_show = &PlayerController::on_show_home,
                    .ctx = this,
                });
                page_home_layer.set_visible(access, false);
            }
            if (handles.page_now_playing) {
                page_now_layer.set_root(handles.page_now_playing);
                page_now_layer.set_hooks(::ui::scene::PageHooks{
                    .on_show = &PlayerController::on_show_now_playing,
                    .ctx = this,
                });
                page_now_layer.set_visible(access, false);
            }
            if (handles.page_library) {
                page_library_layer.set_root(handles.page_library);
                page_library_layer.set_hooks(::ui::scene::PageHooks{
                    .on_show = &PlayerController::on_show_library,
                    .ctx = this,
                });
                page_library_layer.set_visible(access, false);
            }
            if (handles.btn_mode) {
                access.set_visible(handles.btn_mode, false);
            }
            set_page(start_page);
        }

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

        void handle_key_action(UiKey key) {
            switch (key) {
            case UiKey::Up:
                focus_list();
                nav_list(-1);
                break;
            case UiKey::Down:
                focus_list();
                nav_list(1);
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
            text_slots.list_hint = alloc();
            text_slots.btn_pause = alloc();
            text_slots.cover_debug = alloc();
            for (auto& slot : text_slots.eq_values) {
                slot = alloc();
            }
            text_slots.volume_value = alloc();
            text_slots.clip_value = alloc();
            text_slots.debug_text = alloc();
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
            clear_image(handles.home_cover_big);
            clear_image(handles.home_cover_left);
            clear_image(handles.home_cover_right);
            clear_image(handles.home_cover_small);
            clear_image(handles.home_cover_bottom_left);
            clear_image(handles.home_cover_bottom_right);
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
                clear_image(handles.home_cover_big);
                clear_image(handles.home_cover_left);
                clear_image(handles.home_cover_right);
                clear_image(handles.home_cover_small);
                clear_image(handles.home_cover_bottom_left);
                clear_image(handles.home_cover_bottom_right);
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
                    set_image(handles.home_cover_big);
                    set_image(handles.home_cover_left);
                    set_image(handles.home_cover_right);
                    set_image(handles.home_cover_small);
                    set_image(handles.home_cover_bottom_left);
                    set_image(handles.home_cover_bottom_right);
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
                    set_image(handles.home_cover_big);
                    set_image(handles.home_cover_left);
                    set_image(handles.home_cover_right);
                    set_image(handles.home_cover_small);
                    set_image(handles.home_cover_bottom_left);
                    set_image(handles.home_cover_bottom_right);
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
            cover_tint_path.clear();
            cover_theme = derive_cover_theme(cover_image);
            apply_now_theme(cover_theme);
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] load failed: %s\n", cover_path.c_str());
#endif
        }

        void on_player_stopped() {
            playback.stop_playback();
            set_status("Stopped");
            set_play_button_text(false);
        }

        void on_player_error(const char* text) {
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
            if (current_page == PlayerPage::Library) {
                refresh_library();
            } else {
                refresh_now_playing();
            }
        }

        void set_play_button_text(bool playing_now) {
            if (!access.valid()) return;
            const int state = playing_now ? 1 : 0;
            if (last_play_button_state == state) return;
            last_play_button_state = state;
            access.set_button_icon(handles.btn_pause, playing_now ? icons.pause : icons.play);
            set_label_slot(handles.btn_pause, text_slots.btn_pause, "");
            if (handles.bottom_play) {
                access.set_button_icon(handles.bottom_play, playing_now ? icons.pause : icons.play);
            }
        }

        void set_time_label(int elapsed_sec) {
            if (elapsed_sec == last_time_sec) return;
            last_time_sec = elapsed_sec;
            int total = 0;
            if (track_preloaded) {
                total = playback.duration_sec();
                if (preloaded_duration_sec > 0) {
                    total = std::max(total, preloaded_duration_sec);
                }
            }
            const int cur_m = elapsed_sec / 60;
            const int cur_s = elapsed_sec % 60;
            const int total_m = total / 60;
            const int total_s = total % 60;
            char left[16]{};
            char right[16]{};
            std::snprintf(left, sizeof(left), "%d:%02d", cur_m, cur_s);
            std::snprintf(right, sizeof(right), "%d:%02d", total_m, total_s);
            set_label_slot(handles.time_left, text_slots.time_left, left);
            set_label_slot(handles.time_right, text_slots.time_right, right);
#if defined(CHARM_PLAYER_COVER_DEBUG)
            if (elapsed_sec == 0) {
                std::printf("[np] time_label left=%s right=%s total=%d\n", left, right, total);
            }
#endif
        }

        void update_info_label() {
            if (!access.valid() || !handles.info_tag) return;
            int total = playback.duration_sec();
            if (preloaded_duration_sec > 0) {
                total = std::max(total, preloaded_duration_sec);
            }
            if (total <= 0) return;
            std::uint32_t rate = 0;
            audio::PlayerSnapshot snap{};
            if (playback.snapshot(snap)) {
                rate = snap.input_fmt.rate;
            }
            if (rate == 0) rate = 48000;
            std::uint64_t kbps = 0;
            if (track_size_bytes > 0) {
                kbps = (track_size_bytes * 8ull) / (static_cast<std::uint64_t>(total) * 1000ull);
            }

            char rate_buf[16]{};
            const std::uint32_t khz_int = rate / 1000;
            const std::uint32_t khz_dec = (rate % 1000) / 100;
            if (khz_dec == 0) {
                std::snprintf(rate_buf, sizeof(rate_buf), "%ukHz", static_cast<unsigned>(khz_int));
            } else {
                std::snprintf(rate_buf, sizeof(rate_buf), "%u.%ukHz",
                              static_cast<unsigned>(khz_int),
                              static_cast<unsigned>(khz_dec));
            }

            char kbps_buf[16]{};
            if (kbps > 0) {
                std::snprintf(kbps_buf, sizeof(kbps_buf), "%llukbps",
                              static_cast<unsigned long long>(kbps));
            } else {
                std::snprintf(kbps_buf, sizeof(kbps_buf), "--kbps");
            }

            const char* fmt = track_format_text.empty() ? "--" : track_format_text.c_str();
            char info[96]{};
            std::snprintf(info, sizeof(info), "%s * %s * %s", rate_buf, kbps_buf, fmt);
            set_info_label(info);
        }

        void reset_duration() {
            playback.reset_duration();
            last_time_sec = -1;
        }

        static const char* play_mode_text(int mode) noexcept {
            switch (mode) {
            case 1: return "Single";
            case 2: return "Shuffle";
            default: return "Order";
            }
        }

        void update_play_mode_label() {
            char buf[32]{};
            std::snprintf(buf, sizeof(buf), "Mode: %s", play_mode_text(play_mode));
            if (last_mode_text.view() == buf) return;
            last_mode_text.assign(buf);
            set_label_slot(handles.mode_hint, text_slots.mode_hint, buf);
            if (!access.valid()) return;
            auto icon = icons.loop;
            if (play_mode == 1) icon = icons.single;
            else if (play_mode == 2) icon = icons.shuffle;
            access.set_button_icon(handles.btn_mode, icon);
        }

        void refresh_now_playing() {
            update_duration_from_player();
            update_progress();
            update_play_mode_label();
            apply_now_theme(cover_theme);
            update_debug_overlay();
        }

        void refresh_home() {
            if (handles.nav_bar) {
                access.set_visible(handles.nav_bar, true);
            }
            if (handles.bottom_bar) {
                access.set_visible(handles.bottom_bar, true);
            }
            update_duration_from_player();
            update_info_label();
            update_debug_overlay();
        }

        void refresh_library() {
            if (handles.nav_bar) {
                access.set_visible(handles.nav_bar, true);
            }
            if (handles.bottom_bar) {
                access.set_visible(handles.bottom_bar, true);
            }
            update_list_title();
            update_list_path();
            update_list_sort_label();
            update_list_placeholder();
            update_debug_overlay();
        }

        static int compare_ci(std::string_view a, std::string_view b) noexcept {
            const std::size_t n = std::min(a.size(), b.size());
            for (std::size_t i = 0; i < n; ++i) {
                const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
                const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
                if (ca < cb) return -1;
                if (ca > cb) return 1;
            }
            if (a.size() < b.size()) return -1;
            if (a.size() > b.size()) return 1;
            return 0;
        }

        int list_index_to_track(int list_index) const noexcept {
            if (list_index < 0) return -1;
            if (list_order.empty()) return list_index;
            if (list_index >= static_cast<int>(list_order.size())) return -1;
            return list_order[static_cast<std::size_t>(list_index)];
        }

        int track_index_to_list(int track_index) const noexcept {
            if (track_index < 0) return -1;
            if (list_order.empty()) return track_index;
            for (std::size_t i = 0; i < list_order.size(); ++i) {
                if (list_order[i] == track_index) {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        static const char* list_view_text(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<const PlayerController*>(ctx);
            if (!self) return "";
            const auto* labels = self->storage.track_labels;
            if (!labels) return "";
            const int track_index = self->list_index_to_track(static_cast<int>(index));
            if (track_index < 0 || track_index >= static_cast<int>(labels->size())) return "";
            return (*labels)[static_cast<std::size_t>(track_index)].c_str();
        }

        static ::ui::scene::ImageId list_view_icon(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<PlayerController*>(const_cast<void*>(ctx));
            if (!self) return ::ui::scene::invalid_image_id();
            const auto* tracks = self->storage.tracks;
            if (!tracks) return ::ui::scene::invalid_image_id();
            const int track_index = self->list_index_to_track(static_cast<int>(index));
            if (track_index < 0 || track_index >= static_cast<int>(tracks->size())) {
                return ::ui::scene::invalid_image_id();
            }
            const auto& path = (*tracks)[static_cast<std::size_t>(track_index)];
            if (!is_audio_path(path.view())) {
                return self->icons.folder;
            }
            std::string_view cover_path{};
            if (track_index < static_cast<int>(self->list_cover_paths.size())) {
                cover_path = self->list_cover_paths[static_cast<std::size_t>(track_index)].view();
            }
            const auto cover_id = self->resolve_list_cover_icon(path.view(), cover_path);
            if (::ui::scene::image_id_valid(cover_id)) return cover_id;
            return self->icons.play;
        }

        void clear_list_cover_cache() {
            for (auto& entry : list_cover_cache) {
                if (::ui::scene::image_id_valid(entry.image.image_id)) {
                    release_cover_image(entry.image);
                }
                entry.path.clear();
            }
            list_cover_next = 0;
        }

        bool resolve_cover_path_for_track(std::string_view track_path, FixedString<260>& out_path) const {
            if (track_path.empty()) return false;
            FixedString<260> folder_path;
            bool has_folder = false;
            if (cover_strategy == CoverStrategy::folder_only
                || cover_strategy == CoverStrategy::folder_first) {
                has_folder = fs_utils::find_cover_for_track(track_path, folder_path);
            }
            switch (cover_strategy) {
            case CoverStrategy::embedded_only:
                out_path.assign(track_path);
                break;
            case CoverStrategy::folder_only:
                out_path.assign(has_folder ? folder_path.view() : "");
                break;
            case CoverStrategy::folder_first:
                out_path.assign(has_folder ? folder_path.view() : track_path);
                break;
            case CoverStrategy::embedded_first:
            default:
                out_path.assign(track_path);
                break;
            }
            return !out_path.empty();
        }

        void build_list_cover_paths() {
            list_cover_paths.clear();
            const auto* tracks = storage.tracks;
            if (!tracks) return;
            const std::size_t count = tracks->size();
            list_cover_paths.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                FixedString<260> cover_path;
                const auto& track_path = (*tracks)[i];
                cover_path.assign(track_path.view());
                list_cover_paths.push_back(cover_path);
            }
        }

        ::ui::scene::ImageId resolve_list_cover_icon(std::string_view track_path,
                                                     std::string_view cover_path_view) {
            FixedString<260> cover_path;
            if (!cover_path_view.empty()) {
                cover_path.assign(cover_path_view);
            } else if (!track_path.empty()) {
                cover_path.assign(track_path);
            } else {
                return ::ui::scene::invalid_image_id();
            }
            if (cover_image.path == cover_path.view()
                && ::ui::scene::image_id_valid(cover_image.image_id)) {
                return cover_image.image_id;
            }
            for (auto& entry : list_cover_cache) {
                if (entry.path.view() == cover_path.view()
                    && ::ui::scene::image_id_valid(entry.image.image_id)) {
                    return entry.image.image_id;
                }
            }
            if (::ui::gfx::image_registry_locked()) {
                return ::ui::scene::invalid_image_id();
            }
            auto& slot = list_cover_cache[list_cover_next % kListCoverCache];
            list_cover_next += 1;
            if (::ui::scene::image_id_valid(slot.image.image_id)) {
                release_cover_image(slot.image);
            }
            slot.path.assign(cover_path.view());
            if (load_cover_image(cover_path.view(), slot.image)) {
                return slot.image.image_id;
            }
            slot.path.clear();
            return ::ui::scene::invalid_image_id();
        }

        void refresh_track_labels() {
            const auto* labels = storage.track_labels;
            if (!labels) return;
            for (std::size_t i = 0; i < labels->size(); ++i) {
                player::font_cache::ensure_text((*labels)[i].view());
            }
        }

        void rebuild_list_order() {
            list_order.clear();
            const auto* labels = storage.track_labels;
            if (!labels) return;
            const std::size_t count = labels->size();
            list_order.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                list_order.push_back(static_cast<int>(i));
            }
            if (count <= 1) return;
            auto cmp = [&](int a, int b) noexcept {
                const auto& left = (*labels)[static_cast<std::size_t>(a)];
                const auto& right = (*labels)[static_cast<std::size_t>(b)];
                const int res = compare_ci(left.view(), right.view());
                return (list_sort == ListSort::NameAsc) ? (res < 0) : (res > 0);
            };
            std::sort(list_order.begin(), list_order.end(), cmp);
        }

        void sync_list_selection() {
            if (!access.valid() || !handles.list) return;
            const auto* labels = storage.track_labels;
            if (!labels) return;
            if (track_index < 0 || track_index >= static_cast<int>(labels->size())) return;
            ignore_list_select = true;
            const int list_index = track_index_to_list(track_index);
            if (list_index >= 0) {
                access.set_list_view_selected(handles.list, list_index);
                last_list_selected = list_index;
            }
            ignore_list_select = false;
        }

        void refresh_list_view() {
            if (!access.valid() || !handles.list) return;
            const auto* tracks = storage.tracks;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            rebuild_list_order();
            access.set_list_view_source(handles.list,
                                         static_cast<std::uint16_t>(count),
                                         this,
                                         &PlayerController::list_view_text);
            access.set_list_view_icon_source(handles.list, this, &PlayerController::list_view_icon, 32);
            access.set_list_row_height(handles.list, 60);
            access.set_scroll_step(handles.list, 60);
            if (count > 0) {
                sync_list_selection();
            } else {
                last_list_selected = -1;
            }
            update_list_title();
            update_list_path();
            update_list_sort_label();
            update_list_placeholder();
        }

        void apply_storage_view(StorageView view) {
            storage = view;
            fs_ready = storage.fs_ready;
            mount_status.assign(storage.mount_status);
            clear_list_cover_cache();
            refresh_track_labels();
            build_list_cover_paths();
            if (fs_ready && !font_retry_done && !font_ttf_path.empty()) {
                reset_player_font_package_cache();
                bind_player_freetype_font(font_ttf_path,
                                          font_small_px,
                                          font_normal_px,
                                          font_large_px);
                apply_player_theme();
                refresh_exact_font_styles();
                font_retry_done = true;
            }
            if (!storage.status.empty()) { set_status(storage.status.data()); }
#if defined(CHARM_PLAYER_COVER_DEBUG)
            if (player::font_cache::ready()) {
                set_status("FontCache: OK");
            } else {
                set_status("FontCache: OFF");
            }
#endif
            refresh_list_view();
            update_list_placeholder();
            if (fs_ready && storage.tracks && storage.tracks->size() > 0
                && !playback.playing() && !playback.paused()
                && !track_preloaded) {
                load_track_index(track_index);
            }
        }

        void update_list_title() {
            if (!access.valid()) return;
            const auto* tracks = storage.tracks;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            char buf[64]{};
            if (count > 0) {
                std::snprintf(buf, sizeof(buf), "Tracks (%d)", count);
                if (last_list_title_text.view() == buf) return;
                last_list_title_text.assign(buf);
                set_label_slot(handles.list_title, text_slots.list_title, buf);
            } else {
                if (last_list_title_text.view() == "Tracks") return;
                last_list_title_text.assign("Tracks");
                set_label_slot(handles.list_title, text_slots.list_title, "Tracks");
            }
            last_list_count = count;
        }

        void set_font_config(std::string_view path,
                             int small_px,
                             int normal_px,
                             int large_px) {
            font_ttf_path.assign(path.begin(), path.end());
            font_small_px = small_px;
            font_normal_px = normal_px;
            font_large_px = large_px;
            font_retry_done = false;
            if (fs_ready && !font_ttf_path.empty()) {
                reset_player_font_package_cache();
                bind_player_freetype_font(font_ttf_path,
                                          font_small_px,
                                          font_normal_px,
                                          font_large_px);
                apply_player_theme();
                font_retry_done = true;
            }
        }

        void update_list_path() {
            if (!access.valid() || !handles.list_path) return;
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) {
                set_label_slot(handles.list_path, text_slots.list_path, "/");
                return;
            }
            bool all_music = true;
            for (std::size_t i = 0; i < tracks->size(); ++i) {
                const auto& path = (*tracks)[i];
                if (!path.view().starts_with("/music")) {
                    all_music = false;
                    break;
                }
            }
            const char* label = all_music ? "/music" : "/";
            set_label_slot(handles.list_path, text_slots.list_path, label);
        }

        void update_list_sort_label() {
            if (!access.valid() || !handles.list_sort) return;
            const char* label = (list_sort == ListSort::NameAsc) ? "Sort A-Z" : "Sort Z-A";
            set_label_slot(handles.list_sort, text_slots.list_sort, label);
        }

        void update_list_placeholder() {
            if (!access.valid() || !handles.list_hint) return;
            const auto* tracks = storage.tracks;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            const bool show = (count == 0);
            access.set_visible(handles.list_hint, show);
            if (!show) return;
            if (!mount_status.empty()) {
                if (last_list_hint_text.view() == mount_status.view()) return;
                last_list_hint_text.assign(mount_status.view());
                set_label_slot(handles.list_hint, text_slots.list_hint, mount_status.c_str());
                return;
            }
            const char* hint = fs_ready ? "No tracks in /music or /" : "FS not ready";
            if (last_list_hint_text.view() == hint) return;
            last_list_hint_text.assign(hint);
            set_label_slot(handles.list_hint, text_slots.list_hint, hint);
        }

        int resolve_next_track() {
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) return -1;
            const int count = static_cast<int>(tracks->size());
            if (play_mode == 1) {
                return track_index;
            }
            if (play_mode == 2) {
                if (count <= 1) return track_index;
                int next = track_index;
                for (int i = 0; i < 4 && next == track_index; ++i) {
                    next = rand_index(count);
                }
                if (next == track_index) {
                    next = (track_index + 1) % count;
                }
                return next;
            }
            int next = track_index + 1;
            if (next >= count) next = 0;
            return next;
        }

        void handle_track_end() {
            const auto* tracks = storage.tracks;
            if (!fs_ready || !tracks || tracks->size() == 0) {
                stop_playback();
                return;
            }
            const int next = resolve_next_track();
            if (next < 0) {
                stop_playback();
                return;
            }
            stop_playback();
            if (load_track_index(next)) {
                start_playback();
            }
        }

        void update_duration_from_player() {
            if (!playback.update_duration_from_player()) return;
            const int dur = playback.duration_sec();
            if (dur > 0 && preloaded_duration_sec == 0) {
                preloaded_duration_sec = dur;
            }
            if (progress_dragging) return;
            const int cur = playback.current_sec();
            if (cur > dur) {
                set_time_label(dur);
                sync_progress_value(100);
            } else {
                set_time_label(cur);
            }
            update_info_label();
        }

        bool update_progress() {
            if (!access.valid() || progress_dragging) return false;
            auto upd = playback.update_progress();
            if (!upd.updated) return false;
            access.set_value(handles.progress, upd.value);
            set_time_label(upd.current_sec);
            return true;
        }

        void update_debug_overlay() {
#if CHARM_PLAYER_DEBUG_UI
            if (!access.valid() || !handles.debug_text) return;
            const auto now_ms = charm::system::ClockCaps::TimeSource::now();
            if (last_debug_tick_ms != 0) {
                const auto dt = now_ms - last_debug_tick_ms;
                if (dt < 500) return;
            }
            last_debug_tick_ms = now_ms;

            audio::PlayerSnapshot snap{};
            if (!playback.snapshot(snap)) return;
            const auto& fmt = snap.output_fmt;
            const std::uint64_t frame_size = fmt.frame_size();
            const std::uint64_t bytes_per_sec = frame_size * fmt.rate;
            const std::uint64_t water_ms = bytes_per_sec ? (snap.water_bytes * 1000 / bytes_per_sec) : 0;
            const std::uint64_t low_ms = bytes_per_sec ? (snap.low_water * 1000 / bytes_per_sec) : 0;
            const std::uint64_t high_ms = bytes_per_sec ? (snap.high_water * 1000 / bytes_per_sec) : 0;

            const auto pump_min_ms = (snap.pump.has_water && bytes_per_sec)
                ? (snap.pump.water_min * 1000 / bytes_per_sec)
                : 0;
            const auto pump_max_ms = (snap.pump.has_water && bytes_per_sec)
                ? (snap.pump.water_max * 1000 / bytes_per_sec)
                : 0;

            char buf[192]{};
            const auto font_stats = player::font_cache::ready()
                ? player::font_cache::stats()
                : player::font_cache::Stats{};
            const bool font_on = player::font_cache::ready();
            std::uint32_t missing_glyphs = 0;
            std::uint32_t missing_fallbacks = 0;
            std::uint32_t utf8_replaces = 0;
#if defined(VIVID_SOA_TRACE_INPUT)
            missing_glyphs = missing_glyph_count();
            missing_fallbacks = missing_glyph_fallback_count();
            utf8_replaces = utf8_replacement_count();
#endif
            std::snprintf(buf, sizeof(buf),
                          "water %llums (%llu..%llu) pump %llu..%llu underrun %llu/%llu font %s %u/%u/%u glyph %u/%u/%u",
                          static_cast<unsigned long long>(water_ms),
                          static_cast<unsigned long long>(low_ms),
                          static_cast<unsigned long long>(high_ms),
                          static_cast<unsigned long long>(pump_min_ms),
                          static_cast<unsigned long long>(pump_max_ms),
                          static_cast<unsigned long long>(snap.stats.underrun_count),
                          static_cast<unsigned long long>(snap.pump.underrun_count),
                          font_on ? "on" : "off",
                          static_cast<unsigned>(font_stats.requests),
                          static_cast<unsigned>(font_stats.cached),
                          static_cast<unsigned>(font_stats.missing),
                          static_cast<unsigned>(missing_glyphs),
                          static_cast<unsigned>(missing_fallbacks),
                          static_cast<unsigned>(utf8_replaces));
            if (last_debug_text.view() == buf) return;
            last_debug_text.assign(buf);
            set_label_slot(handles.debug_text, text_slots.debug_text, buf);

            if (dbg_style_button == static_cast<std::size_t>(-1)) {
                dbg_style_button = perf_overlay_debug_channel("style.button");
                dbg_style_progress = perf_overlay_debug_channel("style.progress");
            }
            {
                const auto mask = style_kind_state_mask(WidgetKind::Button);
                const auto count = style_kind_state_count(WidgetKind::Button);
                const auto offset = style_kind_state_offset(WidgetKind::Button);
                char line[96]{};
                std::snprintf(line, sizeof(line),
                              "btn mask=0x%02X cnt=%u off=%u",
                              static_cast<unsigned>(mask),
                              static_cast<unsigned>(count),
                              static_cast<unsigned>(offset));
                set_perf_overlay_debug_channel(dbg_style_button, line);
            }
            {
                const auto mask = style_kind_state_mask(WidgetKind::ProgressBarSimple);
                const auto count = style_kind_state_count(WidgetKind::ProgressBarSimple);
                const auto offset = style_kind_state_offset(WidgetKind::ProgressBarSimple);
                char line[96]{};
                std::snprintf(line, sizeof(line),
                              "prog mask=0x%02X cnt=%u off=%u",
                              static_cast<unsigned>(mask),
                              static_cast<unsigned>(count),
                              static_cast<unsigned>(offset));
                set_perf_overlay_debug_channel(dbg_style_progress, line);
            }
#else
            (void)this;
#endif
        }

        void sync_progress_value(int value) {
            if (!access.valid()) return;
            access.set_value(handles.progress, value);
        }

        int progress_value_from_x(int x) const {
            if (!access.valid()) return 0;
            const Rect r = access.world_rect(handles.progress);
            if (r.w <= 1) return 0;
            const int dx = std::clamp(x - r.x, 0, r.w);
            return (dx * 100) / r.w;
        }

        int progress_sec_from_value(int value) const {
            const int duration_sec = playback.duration_sec();
            if (duration_sec <= 0) return 0;
            const int clamped = std::clamp(value, 0, 100);
            return (clamped * duration_sec) / 100;
        }

        struct PendingActions {
            bool prev{false};
            bool next{false};
            bool toggle_play{false};
            bool cycle_mode{false};
            bool seek{false};
            int seek_sec{0};
            bool select{false};
            int select_index{-1};
        };

        void update_progress_drag(int x) {
            progress_drag_value = progress_value_from_x(x);
            progress_drag_sec = progress_sec_from_value(progress_drag_value);
            sync_progress_value(progress_drag_value);
            set_time_label(progress_drag_sec);
        }

        void end_progress_drag(bool apply_seek, PendingActions& actions) {
            if (!progress_dragging) return;
            progress_dragging = false;
            if (apply_seek) {
                actions.seek = true;
                actions.seek_sec = progress_drag_sec;
            }
        }

        void start_playback() {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return;
            }
            FixedString<128> status;
            if (!playback.apply_action(PlaybackAction::start, 0, status)) {
                set_status(status.c_str());
                return;
            }
            set_status(status.c_str());
            set_play_button_text(true);
            sync_progress_value(0);
        }

        void pause_playback() {
            FixedString<128> status;
            if (!playback.apply_action(PlaybackAction::pause, 0, status)) return;
            set_status(status.c_str());
            set_play_button_text(false);
        }

        void resume_playback() {
            FixedString<128> status;
            if (!playback.apply_action(PlaybackAction::resume, 0, status)) return;
            set_status(status.c_str());
            set_play_button_text(true);
        }

        void stop_playback() {
            FixedString<128> status;
            if (playback.apply_action(PlaybackAction::stop, 0, status)) {
                set_status(status.c_str());
            } else {
                set_status("Stopped");
            }
            set_play_button_text(false);
            set_time_label(0);
            sync_progress_value(0);
        }

        void apply_actions(const PendingActions& actions) {
            if (actions.prev) {
                switch_track(-1);
            } else if (actions.next) {
                switch_track(1);
            } else if (actions.select) {
                select_track_index(actions.select_index);
            }

            if (actions.toggle_play) {
                FixedString<128> status;
                if (playback.apply_action(PlaybackAction::toggle, 0, status)) {
                    if (!status.empty()) set_status(status.c_str());
                    const bool playing_now = playback.playing();
                    set_play_button_text(playing_now);
                    if (status.view() == "Opening") {
                        sync_progress_value(0);
                    }
                } else if (!status.empty()) { set_status(status.c_str()); }
            }

            if (actions.cycle_mode) {
                cycle_play_mode();
            }

            if (actions.seek) {
                FixedString<128> status;
                if (playback.apply_action(PlaybackAction::seek, actions.seek_sec, status)) {
                    if (!status.empty()) set_status(status.c_str());
                } else if (!status.empty()) { set_status(status.c_str()); }
            }
        }

        void set_track_labels(int idx) {
            if (!storage.track_titles || !storage.track_subtitles) return;
            if (idx < 0 || idx >= static_cast<int>(storage.track_titles->size())) return;
            title_text.assign((*storage.track_titles)[idx].view());
            subtitle_text.assign((*storage.track_subtitles)[idx].view());
            strip_audio_extension(title_text);
            const std::string_view raw = title_text.view();
            const auto sep = raw.find(" - ");
            if (sep != std::string_view::npos) {
                const std::string_view left = raw.substr(0, sep);
                const std::string_view right = raw.substr(sep + 3);
                if (!right.empty()) {
                    title_text.assign(right);
                    if (!left.empty()) {
                        subtitle_text.assign(left);
                    }
                }
            }
            strip_audio_extension(title_text);
            const auto looks_like_format = [&](std::string_view v) noexcept -> bool {
                if (v.empty()) return true;
                if (v == "UNKNOWN") return true;
                if (is_audio_extension(v)) return true;
                if (v.size() > 5) return false;
                for (char ch : v) {
                    if (!std::isalnum(static_cast<unsigned char>(ch))) return false;
                }
                return true;
            };
            if (looks_like_format(subtitle_text.view())) {
                subtitle_text.assign("");
            }
            set_label_slot(handles.title, text_slots.title, title_text.c_str());
            set_label_slot(handles.subtitle, text_slots.subtitle, subtitle_text.c_str());
            set_label_slot(handles.bottom_title, text_slots.bottom_title, title_text.c_str());
            set_label_slot(handles.bottom_subtitle, text_slots.bottom_subtitle, subtitle_text.c_str());
        }
        bool load_track_index(int idx) {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return false;
            }
            const auto* tracks = storage.tracks;
            const auto* labels = storage.track_labels;
            if (!tracks || tracks->size() == 0) return false;
            if (!labels) return false;
            if (idx < 0) idx = 0;
            if (idx >= static_cast<int>(tracks->size())) idx = static_cast<int>(tracks->size()) - 1;
            track_index = idx;
            const auto& vfs_path = (*tracks)[track_index];
            const char* track_path = vfs_path.c_str();
            set_track_labels(track_index);
            track_format_text.clear();
            if (const auto fmt = format_from_path(vfs_path.view()); !fmt.empty()) {
                track_format_text.assign(fmt);
            }
            track_size_bytes = query_track_size(track_path);
            last_info_text.clear();
            FixedString<128> status;
            const bool track_ready = player::check_track_ready(vfs_path.view(), status);
            if (!status.empty()) { set_status(status.c_str()); }
            playback.set_track_path(track_path);
            playback.set_track_ready(track_ready);
            track_preloaded = true;
            if (track_ready) {
                cover_embedded_path.assign(vfs_path.view());
                cover_folder_path.clear();
                FixedString<260> folder_path;
                bool has_folder = false;
                if (cover_strategy == CoverStrategy::folder_only
                    || cover_strategy == CoverStrategy::folder_first) {
                    has_folder = fs_utils::find_cover_for_track(vfs_path.view(), folder_path);
                }
                cover_folder_path.assign(folder_path.view());
                cover_ready = true;
                switch (cover_strategy) {
                case CoverStrategy::embedded_only:
                    cover_path.assign(cover_embedded_path.c_str());
                    break;
                case CoverStrategy::folder_only:
                    cover_path.assign(has_folder ? cover_folder_path.c_str() : "");
                    cover_ready = has_folder;
                    break;
                case CoverStrategy::folder_first:
                    cover_path.assign(has_folder ? cover_folder_path.c_str() : cover_embedded_path.c_str());
                    break;
                case CoverStrategy::embedded_first:
                default:
                    cover_path.assign(cover_embedded_path.c_str());
                    break;
                }
            } else {
                cover_ready = false;
                cover_path.clear();
                cover_embedded_path.clear();
                cover_folder_path.clear();
            }
            update_cover_image();
            reset_duration();
            preloaded_duration_sec = 0;
            if (track_ready) {
                int secs = 0;
                if (player::probe_duration_seconds(track_path, secs)) {
                    playback.set_duration_from_probe(secs);
                    preloaded_duration_sec = secs;
                    update_info_label();
#if defined(CHARM_PLAYER_COVER_DEBUG)
                    std::printf("[np] preload duration=%d path=%s\n", secs, track_path);
#endif
                }
            }
            set_play_button_text(false);
            last_time_sec = -1;
            set_time_label(0);
            sync_progress_value(0);
            sync_list_selection();
            return playback.track_ready();
        }

        void switch_track(int delta) {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return;
            }
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) return;
            const int count = static_cast<int>(tracks->size());
            int next = track_index + delta;
            if (next < 0) next = count - 1;
            if (next >= count) next = 0;
            const bool was_active = playback.playing() || playback.paused();
            stop_playback();
            load_track_index(next);
            if (was_active && playback.track_ready()) {
                start_playback();
            }
        }

        void select_track_index(int idx) {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return;
            }
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) return;
            const bool was_playing = playback.playing();
            const bool was_paused = playback.paused();
            stop_playback();
            load_track_index(idx);
            if (was_playing && playback.track_ready()) {
                start_playback();
            } else if (was_paused && playback.track_ready()) {
                start_playback();
                pause_playback();
            }
        }

        void set_play_mode(int mode) {
            play_mode = mode;
            update_play_mode_label();
        }

        void cycle_play_mode() {
            set_play_mode((play_mode + 1) % 3);
        }

        void cycle_list_sort() {
            list_sort = (list_sort == ListSort::NameAsc) ? ListSort::NameDesc : ListSort::NameAsc;
            refresh_list_view();
        }

        void focus_list() {
            if (!access.valid() || !handles.list) return;
            access.set_focused(handles.list, true);
        }

        void nav_list(int delta) {
            if (!access.valid() || !handles.list) return;
            const auto* tracks = storage.tracks;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            if (count <= 0) return;
            int selected = access.list_view_selected(handles.list);
            if (selected < 0) selected = 0;
            selected += delta;
            if (selected < 0) selected = 0;
            if (selected >= count) selected = count - 1;
            access.set_list_view_selected(handles.list, selected);
        }

        void nav_list_activate() {
            if (!access.valid() || !handles.list) return;
            const int selected = access.list_view_selected(handles.list);
            const int track_idx = list_index_to_track(selected);
            if (track_idx >= 0) {
                select_track_index(track_idx);
            }
        }

        void sync_eq_values() {
            if (!access.valid()) return;
            bool changed = false;
            for (std::size_t i = 0; i < kEqBands; ++i) {
                if (!handles.eq_sliders[i] || !handles.eq_values[i]) continue;
                const int value = access.value(handles.eq_sliders[i]);
                if (value == last_eq_values[i]) continue;
                last_eq_values[i] = value;
                eq_values[i] = value;
                changed = true;
                char buf[16]{};
                std::snprintf(buf, sizeof(buf), "%+d", value);
                set_label_slot(handles.eq_values[i], text_slots.eq_values[i], buf);
            }
            if (!changed) return;
            static constexpr std::array<std::uint32_t, kEqBands> kEqFreqs{
                60, 250, 1000, 4000, 16000
            };
            eq_config.enabled = true;
            eq_config.band_count = static_cast<std::uint8_t>(kEqBands);
            for (std::size_t i = 0; i < kEqBands; ++i) {
                eq_config.bands[i].freq_hz = kEqFreqs[i];
                eq_config.bands[i].gain_db = static_cast<float>(eq_values[i]);
                eq_config.bands[i].q = 1.0f;
            }
            FixedString<128> status;
            if (!playback.set_eq(eq_config, status) && !status.empty()) {
                set_status(status.c_str());
            }
        }

        void sync_volume_value() {
            if (!access.valid() || !handles.volume_slider || !handles.volume_value) return;
            const int value = access.value(handles.volume_slider);
            if (value == last_volume_value) return;
            last_volume_value = value;
            char buf[16]{};
            std::snprintf(buf, sizeof(buf), "%d", value);
            set_label_slot(handles.volume_value, text_slots.volume_value, buf);
            FixedString<128> status;
            if (!playback.set_volume(value, status) && !status.empty()) {
                set_status(status.c_str());
            }
        }

        void sync_dsp_controls() {
            if (!access.valid()) return;
            if (handles.dc_switch) {
                const int enabled = access.checked(handles.dc_switch) ? 1 : 0;
                if (enabled != last_dc_enabled) {
                    last_dc_enabled = enabled;
                    FixedString<128> status;
                    if (!playback.set_dc_block(enabled != 0, status) && !status.empty()) {
                        set_status(status.c_str());
                    }
                }
            }
            if (!handles.clip_switch || !handles.clip_slider || !handles.clip_value) return;
            const int enabled = access.checked(handles.clip_switch) ? 1 : 0;
            const int threshold = access.value(handles.clip_slider);
            bool changed = false;
            if (enabled != last_clip_enabled) {
                last_clip_enabled = enabled;
                changed = true;
            }
            if (threshold != last_clip_threshold) {
                last_clip_threshold = threshold;
                char buf[16]{};
                std::snprintf(buf, sizeof(buf), "%d", threshold);
                set_label_slot(handles.clip_value, text_slots.clip_value, buf);
                changed = true;
            }
            if (!changed) return;
            FixedString<128> status;
            if (!playback.set_soft_clip(enabled != 0, threshold, status) && !status.empty()) {
                set_status(status.c_str());
            }
        }

        void process_input_events() {
            if (!access.valid()) return;
            PendingActions actions{};
            const std::size_t count = access.input_event_count();
            for (std::size_t i = 0; i < count; ++i) {
                const auto& item = access.input_event(i);
                const auto target = item.target;
                const auto type = item.event.type;
                if (current_page == PlayerPage::NowPlaying) {
                    if (target == handles.progress) {
                        if (type == Event::Type::MouseDown) {
                            progress_dragging = true;
                            update_progress_drag(item.event.x);
                        } else if (type == Event::Type::DragStart || type == Event::Type::DragMove
                                   || type == Event::Type::MouseMove) {
                            if (progress_dragging) {
                                update_progress_drag(item.event.x);
                            }
                        } else if (type == Event::Type::MouseUp || type == Event::Type::DragEnd) {
                            if (progress_dragging) {
                                update_progress_drag(item.event.x);
                                end_progress_drag(true, actions);
                            }
                        } else if (type == Event::Type::Cancel) {
                            end_progress_drag(false, actions);
                        }
                        continue;
                    }
                if (type == Event::Type::MouseUp) {
                    if (target == handles.now_back) {
                        set_page(return_page);
                    } else if (target == handles.btn_prev) {
                        actions.prev = true;
                    } else if (target == handles.btn_next) {
                        actions.next = true;
                    } else if (target == handles.btn_pause) {
                        actions.toggle_play = true;
                    } else if (target == handles.btn_mode) {
                        actions.cycle_mode = true;
#if defined(CHARM_PLAYER_DEBUG_UI)
                    } else if (target == handles.cover) {
                        cycle_palette_style();
                    } else if (target == handles.now_backdrop) {
                        cycle_cover_theme();
#endif
                    }
                }
                } else {
                    if (type == Event::Type::MouseUp) {
                        if (target == handles.bottom_play) {
                            actions.toggle_play = true;
                        } else if (target == handles.bottom_next) {
                            actions.next = true;
                        } else if (target == handles.bottom_hit || target == handles.bottom_bar
                                   || target == handles.bottom_cover
                                   || target == handles.bottom_title || target == handles.bottom_subtitle) {
                            set_page(PlayerPage::NowPlaying);
                        } else if (target == handles.nav_home) {
                            set_page(PlayerPage::Home);
                        } else if (target == handles.nav_library) {
                            set_page(PlayerPage::Library);
                        } else if (target == handles.list_sort) {
                            cycle_list_sort();
                        }
                    }
                }
            }

            if (handles.list) {
                const int selected = access.list_view_selected(handles.list);
                if (selected >= 0 && selected != last_list_selected) {
                    last_list_selected = selected;
                    if (!ignore_list_select) {
                        const int track_idx = list_index_to_track(selected);
                        actions.select = true;
                        actions.select_index = track_idx;
                    }
                }
            }

            apply_actions(actions);
            sync_eq_values();
            sync_volume_value();
            sync_dsp_controls();
        }

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
