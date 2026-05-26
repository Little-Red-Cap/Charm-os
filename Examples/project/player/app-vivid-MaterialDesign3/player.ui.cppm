module;
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <cstdio>

#if defined(CHARM_PLAYER_HOST_UI) && CHARM_PLAYER_HOST_UI && \
    defined(CHARM_PLAYER_HOST_FILE_FONTS) && CHARM_PLAYER_HOST_FILE_FONTS
#define CHARM_PLAYER_USE_HOST_FILE_FONTS 1
#else
#define CHARM_PLAYER_USE_HOST_FILE_FONTS 0
#endif

export module player.ui;

import charm.core.style;
import charm.core.style_sheet;
import charm.core.theme_preset;
import charm.gfx.color;
import charm.gfx.image;
import charm.font;
import charm.font.typography;
import charm.font.font_noto_ascii_16;
import charm.font.font_noto_ascii_12;
import charm.font.font_noto_sc_16;
import charm.ui.scene.pill_surface;
#if CHARM_PLAYER_USE_HOST_FILE_FONTS
import charm.ui.vivid.font_package;
import charm.font.provider_freetype;
#endif
import player.font_cache;
import charm.widgets.button;
import charm.widgets.chart;
import charm.widgets.cloudy_glass;
import charm.widgets.busy_wheel;
import charm.widgets.console_box;
import charm.widgets.foldable_panel;
import charm.widgets.histogram_view;
import charm.widgets.histogram;
import charm.widgets.dynamic_nebula;
import charm.widgets.crt_screen;
import charm.widgets.spectrum_view;
import charm.widgets.spinning_wheel;
import charm.widgets.image;
import charm.widgets.image_box;
import charm.widgets.label;
import charm.widgets.meter_pointer;
import charm.widgets.list_view;
import charm.widgets.progress;
import charm.widgets.battery_gasgauge;
import charm.widgets.progress_bar_drill;
import charm.widgets.progress_bar_simple;
import charm.widgets.scrollbar;
import charm.widgets.scroll_container;
import charm.widgets.segmented_control;
import charm.widgets.perf_overlay;
import charm.widgets.switcher;
import charm.widgets.slider;
import charm.widgets.dropdown;
import charm.gfx.svg;

export namespace player::ui {
    enum class PlayerStyleClass : StyleClassId {
        ControlSide = 1,
        ControlPlay = 2,
        TopBarButton = 3,
        TabBase = 4,
        ShuffleShadow = 5,
        ListCard = 6,
        InfoTag = 7,
        BottomBar = 8,
        BottomButton = 9,
        BottomPlay = 10,
        LibraryListCard = 11,
    };

    inline constexpr int kUiPadding = 18;
    inline constexpr int kCoverSize = 360;
    inline constexpr int kDemoGap = 16;
    inline constexpr int kHeaderTitleOffset = 18;
    inline constexpr int kHeaderSubtitleOffset = 44;
    inline constexpr int kHeaderProgressOffset = 86;
    inline constexpr int kHeaderTimeOffset = 114;
    inline constexpr int kHeaderStatusOffset = 136;
    inline constexpr int kHeaderModeOffset = 160;
    inline constexpr int kModeWidth = 240;
    inline constexpr int kModeHeight = 28;
    inline constexpr int kModeHintHeight = 18;
    inline constexpr int kModeHintGap = 6;
    inline constexpr int kSpectrumHeight = 80;
    inline constexpr int kSpectrumGap = 12;
    inline constexpr int kOptionsHeight = 34;
    inline constexpr int kOptionsGap = 10;
    inline constexpr int kOptionLabelWidth = 92;
    inline constexpr int kEqBands = 5;
    inline constexpr int kEqPanelHeight = 248;
    inline constexpr int kEqTitleHeight = 18;
    inline constexpr int kEqRowHeight = 22;
    inline constexpr int kEqRowGap = 6;
    inline constexpr int kEqLabelWidth = 40;
    inline constexpr int kEqValueWidth = 44;
    inline constexpr int kEqRowGapX = 10;
    inline constexpr int kDspToggleWidth = 44;
    inline constexpr int kEqPresetLabelWidth = 56;
    inline constexpr int kEqPresetWidth = 140;
    inline constexpr int kListTitleGap = 26;
    inline constexpr int kListBottomReserve = 150;
    inline constexpr int kListScrollWidth = 10;
    inline constexpr int kControlsBottomMargin = 22;
    inline constexpr int kButtonWidth = 100;
    inline constexpr int kButtonHeight = 56;
    inline constexpr int kButtonGap = 12;
    inline constexpr int kModeButtonWidth = 72;
    inline constexpr int kPerfOverlayWidth = 300;
    inline constexpr int kPerfOverlayHeight = 96;

    inline constexpr rgba kUiBackground = {16, 18, 24, 255};
    inline constexpr rgba kUiCover = {40, 42, 56, 255};
    inline constexpr rgba kUiTitle = {240, 242, 250, 255};
    inline constexpr rgba kUiSubtitle = {170, 176, 200, 255};
    inline constexpr rgba kUiTime = {122, 132, 156, 255};
    inline constexpr rgba kUiTimeSoft = {140, 148, 170, 200};
    inline constexpr rgba kUiStatus = {160, 166, 190, 255};
    inline constexpr rgba kUiListTitle = {210, 214, 230, 255};
    inline constexpr rgba kUiHint = {130, 138, 160, 255};
    inline constexpr rgba kUiError = {232, 130, 140, 255};
    inline constexpr rgba kUiOk = {156, 210, 196, 255};
    inline constexpr rgba kUiPaused = {230, 190, 120, 255};
    inline constexpr rgba kUiOption = {190, 198, 220, 255};
    inline constexpr rgba kUiSwitchOn = {140, 176, 255, 255};
    inline constexpr rgba kUiEqTitle = {210, 216, 235, 255};

    inline constexpr rgba kUiButtonBg = {36, 40, 58, 255};
    inline constexpr rgba kUiButtonBorder = {64, 74, 102, 255};
    inline constexpr rgba kUiButtonHover = {48, 56, 82, 255};
    inline constexpr rgba kUiTabBg = {32, 36, 52, 255};
    inline constexpr rgba kUiTabActive = {72, 92, 132, 255};
    inline constexpr rgba kUiListBg = {34, 38, 56, 255};
    inline constexpr rgba kUiInfoTagBg = {28, 32, 46, 255};
    inline constexpr rgba kUiInfoTagBgSoft = {28, 32, 46, 180};
    inline constexpr rgba kUiInfoTagBorderSoft = {64, 74, 102, 120};
    inline constexpr rgba kUiControlSideBg = {36, 40, 58, 210};
    inline constexpr rgba kUiControlSideBorder = {64, 74, 102, 140};
    inline constexpr rgba kUiListBorder = {62, 72, 104, 255};
    inline constexpr rgba kUiListFont = {220, 228, 246, 255};
    inline constexpr rgba kUiProgressBg = {22, 26, 40, 255};
    inline constexpr rgba kUiProgressBorder = {64, 74, 102, 255};
    inline constexpr rgba kUiBackdropBase = {18, 20, 30, 255};
    inline constexpr rgba kUiScrollBg = {36, 40, 58, 255};
    inline constexpr rgba kUiScrollBorder = {72, 82, 110, 255};
    inline constexpr rgba kUiPerfBg = {26, 28, 40, 220};
    inline constexpr rgba kUiPerfBorder = {70, 80, 110, 255};
    inline constexpr rgba kUiPerfFont = {230, 236, 248, 255};
    inline constexpr rgba kUiPlayBg = {14, 18, 30, 255};
    inline constexpr rgba kUiPlayShadow = {0, 0, 0, 160};
    inline constexpr rgba kUiCardShadow = {0, 0, 0, 70};
    inline constexpr rgba kUiLibraryHeroTopStart = {102, 136, 198, 108};
    inline constexpr rgba kUiLibraryHeroTopEnd = {24, 40, 80, 18};
    inline constexpr rgba kUiLibraryHeroBottomStart = {12, 22, 48, 92};
    inline constexpr rgba kUiLibraryHeroBottomEnd = {4, 8, 22, 252};
    inline constexpr rgba kUiLibrarySurfaceBg = {15, 19, 28, 255};
    inline constexpr rgba kUiLibraryControlsTop = {26, 34, 58, 236};
    inline constexpr rgba kUiLibraryControlsBottom = {10, 14, 24, 248};
    inline constexpr rgba kUiLibraryControlsBorder = {126, 144, 188, 54};
    inline constexpr rgba kUiLibraryCardTop = {18, 22, 32, 252};
    inline constexpr rgba kUiLibraryCardBottom = {18, 22, 32, 252};
    inline constexpr rgba kUiLibraryCardBorder = {102, 118, 154, 62};
    inline constexpr rgba kUiLibraryHeaderPlate = {28, 33, 46, 255};
    inline constexpr rgba kUiLibraryHeaderBorder = {110, 128, 170, 42};
    inline constexpr rgba kUiLibraryBodyPlate = {19, 23, 34, 255};
    inline constexpr rgba kUiLibraryBodyBorder = {70, 84, 120, 34};
    inline constexpr rgba kUiHomeDailyMixTop = {94, 130, 220, 255};
    inline constexpr rgba kUiHomeDailyMixBottom = {172, 102, 202, 255};
    inline constexpr rgba kUiLibraryChipIdle = {24, 28, 40, 236};
    inline constexpr rgba kUiLibraryChipBorder = {82, 96, 134, 144};
    inline constexpr rgba kUiLibraryChipActive = {66, 82, 118, 240};
    inline constexpr rgba kUiLibraryChipTextMuted = {194, 202, 226, 220};
    inline constexpr rgba kUiLibraryChipText = {226, 232, 246, 244};
    inline constexpr rgba kUiLibraryChipTextActive = {248, 250, 255, 255};
    inline constexpr rgba kUiLibraryTabActive = {176, 194, 236, 246};
    inline constexpr rgba kUiLibraryTabBorderActive = {220, 230, 255, 255};
    inline constexpr rgba kUiLibraryTabTextActive = {24, 38, 76, 255};
    inline constexpr rgba kUiLibraryListAccent = {88, 108, 156, 228};
    inline constexpr rgba kUiLibraryListOnAccent = {238, 242, 250, 255};
    inline constexpr rgba kUiLibraryPathIdle = {30, 35, 49, 236};
    inline constexpr rgba kUiLibraryPathActive = {42, 49, 68, 244};
    inline constexpr rgba kUiLibraryPathBorderActive = {132, 162, 226, 246};
    inline constexpr rgba kUiLibraryPathText = {210, 220, 240, 226};
    inline constexpr rgba kUiLibraryPathTextActive = {232, 240, 255, 255};
    inline constexpr rgba kUiBottomBarBg = {22, 26, 38, 248};
    inline constexpr rgba kUiBottomButtonBg = {30, 35, 49, 255};
    inline constexpr rgba kUiBottomButtonFg = {232, 238, 248, 255};
    inline constexpr rgba kUiBottomPlayBg = {50, 65, 98, 255};
    inline constexpr rgba kUiBottomPlayFg = {246, 249, 255, 255};

    struct PlayerIconIds {
        ::ui::gfx::ImageId prev{};
        ::ui::gfx::ImageId play{};
        ::ui::gfx::ImageId pause{};
        ::ui::gfx::ImageId next{};
        ::ui::gfx::ImageId chevron_right{};
        ::ui::gfx::ImageId loop{};
        ::ui::gfx::ImageId single{};
        ::ui::gfx::ImageId shuffle{};
        ::ui::gfx::ImageId folder{};
        ::ui::gfx::ImageId home{};
        ::ui::gfx::ImageId home_active{};
        ::ui::gfx::ImageId search{};
        ::ui::gfx::ImageId search_active{};
        ::ui::gfx::ImageId settings{};
        ::ui::gfx::ImageId down{};
        ::ui::gfx::ImageId more{};
        ::ui::gfx::ImageId folder_active{};
    };

    namespace detail {
        constexpr int kIconSize = 48;
        constexpr int kIconStride = kIconSize * 4;
        using IconBuffer = std::array<std::byte, kIconSize * kIconSize * 4>;
        constexpr ::ui::gfx::svg::ViewBox kIconView{24.0f, 24.0f};
        constexpr ::ui::gfx::svg::ViewBox kLegacyIconView{960.0f, 960.0f};

        void icon_clear(IconBuffer& buf) {
            buf.fill(std::byte{0});
        }

        void icon_set_pixel(IconBuffer& buf, int x, int y, const rgba& color) {
            if (x < 0 || y < 0 || x >= kIconSize || y >= kIconSize) return;
            const std::size_t idx = static_cast<std::size_t>(y * kIconSize + x) * 4;
            buf[idx + 0] = std::byte{color.a};
            buf[idx + 1] = std::byte{color.r};
            buf[idx + 2] = std::byte{color.g};
            buf[idx + 3] = std::byte{color.b};
        }

        bool rasterize_svg_path(IconBuffer& buf, std::string_view path, const rgba& color,
                                ::ui::gfx::svg::ViewBox view = kIconView) {
            const ::ui::gfx::svg::RasterConfig cfg{.width = kIconSize, .height = kIconSize, .view = view};
            return ::ui::gfx::svg::rasterize_path(path, cfg,
                                                std::span<std::byte>(buf.data(), buf.size()),
                                                color, true);
        }

        constexpr std::string_view kPathPrev =
            "M6 6h2v12H6z"
            "M16 18V6l-8.5 6L16 18z";
        constexpr std::string_view kPathPlay =
            "M8 6.82v10.36c0 .79.87 1.27 1.54.84l8.14-5.18a1 1 0 0 0 0-1.69L9.54 5.98"
            "A.998.998 0 0 0 8 6.82z";
        constexpr std::string_view kPathPause =
            "M6 5h4v14H6z"
            "M14 5h4v14h-4z";
        constexpr std::string_view kPathNext =
            "M8 18V6l8.5 6L8 18z"
            "M16 6h2v12h-2z";
        constexpr std::string_view kPathChevronRight =
            "M9.29 6.71a.996.996 0 0 0 0 1.41L13.17 12l-3.88 3.88"
            "a.996.996 0 1 0 1.41 1.41l4.59-4.59c.39-.39.39-1.02 0-1.41"
            "L10.7 6.7a.996.996 0 0 0-1.41.01z";
        constexpr std::string_view kPathLoop =
            "M5 7h10v2H5z"
            "M15 4l4 4-4 4z"
            "M17 9h2v4h-2z"
            "M9 15h10v2H9z"
            "M9 12l-4 4 4 4z"
            "M5 9h2v4H5z";
        constexpr std::string_view kPathSingle =
            "M5 7h10v2H5z"
            "M15 4l4 4-4 4z"
            "M17 9h2v4h-2z"
            "M9 15h10v2H9z"
            "M9 12l-4 4 4 4z"
            "M5 9h2v4H5z"
            "M11 9h2v8h-2z";
        constexpr std::string_view kPathShuffle =
            "M10.59 9.17L6.12 4.7a.996.996 0 1 0-1.41 1.41l4.46 4.46l1.42-1.4z"
            "m4.76-4.32l1.19 1.19L4.7 17.88a.996.996 0 1 0 1.41 1.41L17.96 7.46l1.19 1.19"
            "a.5.5 0 0 0 .85-.36V4.5c0-.28-.22-.5-.5-.5h-3.79a.5.5 0 0 0-.36.85z"
            "m-.52 8.56l-1.41 1.41l3.13 3.13l-1.2 1.2a.5.5 0 0 0 .36.85h3.79c.28 0 .5-.22.5-.5"
            "v-3.79c0-.45-.54-.67-.85-.35l-1.19 1.19l-3.13-3.14z";
        constexpr std::string_view kPathFolder =
            "M10.59 4.59C10.21 4.21 9.7 4 9.17 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2"
            "h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-1.41-1.41z";
        constexpr std::string_view kPathHome =
            "M10 19v-5h4v5c0 .55.45 1 1 1h3c.55 0 1-.45 1-1v-7h1.7c.46 0 .68-.57.33-.87L12.67 3.6"
            "c-.38-.34-.96-.34-1.34 0l-8.36 7.53c-.34.3-.13.87.33.87H5v7c0 .55.45 1 1 1h3"
            "c.55 0 1-.45 1-1z";
        constexpr std::string_view kPathSearch =
            "M9 3.5L12.9 5.1L14.5 9L12.9 12.9L9 14.5L5.1 12.9L3.5 9L5.1 5.1Z"
            "M9 6.5L6.9 7.4L6 9L6.9 10.6L9 11.5L11.1 10.6L12 9L11.1 7.4Z"
            "M12.8 13.8L14.2 12.4L19.2 17.4L17.8 18.8Z";
        constexpr std::string_view kPathSettings =
            "M3 17v2h6v-2H3z"
            "M3 5v2h10V5H3z"
            "M13 21v-2h8v-2h-8v-2h-2v6h2z"
            "M7 11h2V9h12V7H9V5H7v6z"
            "M15 15v2h6v-2h-6z";
        constexpr std::string_view kPathDown =
            "M15.88 9.29L12 13.17L8.12 9.29a.996.996 0 1 0-1.41 1.41l4.59 4.59"
            "c.39.39 1.02.39 1.41 0l4.59-4.59a.996.996 0 0 0 0-1.41c-.39-.38-1.03-.39-1.42 0z";
        constexpr std::string_view kPathMore =
            "M12 4a2 2 0 1 0 0 4a2 2 0 1 0 0-4z"
            "M12 10a2 2 0 1 0 0 4a2 2 0 1 0 0-4z"
            "M12 16a2 2 0 1 0 0 4a2 2 0 1 0 0-4z";

        void build_prev_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathPrev, color);
        }

        void build_play_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathPlay, color);
        }

        void build_pause_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathPause, color);
        }

        void build_loop_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathLoop, color);
        }

        void build_single_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathSingle, color);
        }

        void build_shuffle_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathShuffle, color);
        }

        void build_next_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathNext, color);
        }

        void build_chevron_right_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathChevronRight, color);
        }

        void build_folder_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathFolder, color);
        }

        void build_home_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathHome, color);
        }

        void build_search_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathSearch, color);
        }

        void build_settings_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathSettings, color);
        }

        void build_down_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathDown, color);
        }

        void build_more_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathMore, color);
        }

        // TODO(player/ui): Make exact font cache size product-configurable after host-side typography tuning stabilizes.
        inline constexpr std::size_t kPlayerExactFontCacheSlots = 24;

#if CHARM_PLAYER_USE_HOST_FILE_FONTS
        struct FontPackageState {
            charm::font::VfsFontPackage package{};
            bool bound{false};
        };

        FontPackageState& font_package_state() {
            static FontPackageState state{};
            return state;
        }

        struct ExactFontSlot {
            Font font{};
            std::string path{};
            int px{0};
            FontWeight weight{FontWeight::Regular};
            std::uint64_t last_used{0};
            bool loaded{false};
        };

        struct FreetypeLoaderState {
            charm::font::FreetypeFontLoader loader{};
            std::string ttf_path{};
            std::string ttf_fallback_path{};
            std::string ttf_small{};
            std::string ttf_normal{};
            std::string ttf_large{};
            std::string ttf_mono{};
            std::string ttf_small_medium{};
            std::string ttf_normal_medium{};
            std::string ttf_large_medium{};
            std::string ttf_mono_medium{};
            std::string ttf_small_bold{};
            std::string ttf_normal_bold{};
            std::string ttf_large_bold{};
            std::string ttf_mono_bold{};
            std::string ttf_fallback{};
            std::array<ExactFontSlot, kPlayerExactFontCacheSlots> exact_fonts{};
            std::uint64_t exact_font_use_tick{0};
            bool ready{false};
        };

        FreetypeLoaderState& freetype_state() {
            static FreetypeLoaderState state{};
            return state;
        }

        void reset_exact_font_cache(FreetypeLoaderState& state) noexcept {
            const auto api = state.loader.vfs_api();
            for (auto& slot : state.exact_fonts) {
                if (api.reset) {
                    api.reset(&state.loader, slot.font);
                } else {
                    slot.font = Font{};
                }
                slot.path.clear();
                slot.px = 0;
                slot.weight = FontWeight::Regular;
                slot.last_used = 0;
                slot.loaded = false;
            }
            state.exact_font_use_tick = 0;
        }

        const Font& fallback_font_for_px(int px, FontWeight weight) noexcept {
            if (px >= 48) return get_font_weighted(FontId::Large, weight);
            if (px <= 14) return get_font_weighted(FontId::Small, weight);
            return get_font_weighted(FontId::Normal, weight);
        }
#else
        const Font& fallback_font_for_px(int px, FontWeight weight) noexcept {
            if (px >= 48) return get_font_weighted(FontId::Large, weight);
            if (px <= 14) return get_font_weighted(FontId::Small, weight);
            return get_font_weighted(FontId::Normal, weight);
        }
#endif

        bool& system_font_fallback_enabled_state() noexcept {
            static bool enabled = true;
            return enabled;
        }
    }

    inline ImageView icon_prev() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_prev_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_play() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_play_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_pause() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_pause_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_loop() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_loop_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_single() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_single_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_shuffle() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_shuffle_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_next() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_next_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_chevron_right() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_chevron_right_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_folder() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_folder_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_home() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_home_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_home_active() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_home_icon(buf, kUiOk);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_search() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_search_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_search_active() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_search_icon(buf, kUiOk);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_settings() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_settings_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_down() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_down_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_more() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_more_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_folder_active() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_folder_icon(buf, kUiOk);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline PlayerIconIds register_player_icons() noexcept {
        PlayerIconIds out{};
        auto reg = [](const ImageView& view) noexcept {
            const auto res = ::ui::gfx::register_image_dedup(view);
            return res.ok() ? res.id : ::ui::gfx::invalid_image_id();
        };
        out.prev = reg(icon_prev());
        out.play = reg(icon_play());
        out.pause = reg(icon_pause());
        out.next = reg(icon_next());
        out.chevron_right = reg(icon_chevron_right());
        out.loop = reg(icon_loop());
        out.single = reg(icon_single());
        out.shuffle = reg(icon_shuffle());
        out.folder = reg(icon_folder());
        out.home = reg(icon_home());
        out.home_active = reg(icon_home_active());
        out.search = reg(icon_search());
        out.search_active = reg(icon_search_active());
        out.settings = reg(icon_settings());
        out.down = reg(icon_down());
        out.more = reg(icon_more());
        out.folder_active = reg(icon_folder_active());
        return out;
    }

    inline const Font& player_default_font() noexcept {
        return get_font(FontId::Normal);
    }

    void set_player_system_font_fallback_enabled(bool enabled) noexcept {
        detail::system_font_fallback_enabled_state() = enabled;
    }

    bool player_system_font_fallback_enabled() noexcept {
        return detail::system_font_fallback_enabled_state();
    }

    bool font_package_bound() noexcept {
#if CHARM_PLAYER_USE_HOST_FILE_FONTS
        return detail::font_package_state().bound;
#else
        return false;
#endif
    }

    void clear_player_font_binding() noexcept {
        set_font_provider(FontProvider{});
        set_font_weight_provider(FontWeightProvider{});
        set_font_glyph_loader(FontGlyphLoaderApi{}, nullptr);
#if CHARM_PLAYER_USE_HOST_FILE_FONTS
        auto& package_state = detail::font_package_state();
        package_state.package.reset_cache();
        package_state.bound = false;
        auto& freetype = detail::freetype_state();
        detail::reset_exact_font_cache(freetype);
        freetype.ttf_path.clear();
        freetype.ttf_fallback_path.clear();
        freetype.ttf_small.clear();
        freetype.ttf_normal.clear();
        freetype.ttf_large.clear();
        freetype.ttf_mono.clear();
        freetype.ttf_small_medium.clear();
        freetype.ttf_normal_medium.clear();
        freetype.ttf_large_medium.clear();
        freetype.ttf_mono_medium.clear();
        freetype.ttf_small_bold.clear();
        freetype.ttf_normal_bold.clear();
        freetype.ttf_large_bold.clear();
        freetype.ttf_mono_bold.clear();
        freetype.ttf_fallback.clear();
        freetype.ready = false;
#endif
    }

    void reset_player_font_package_cache() noexcept {
#if CHARM_PLAYER_USE_HOST_FILE_FONTS
        auto& state = detail::font_package_state();
        state.package.reset_cache();
        state.bound = false;
        detail::reset_exact_font_cache(detail::freetype_state());
#endif
    }

#if CHARM_PLAYER_USE_HOST_FILE_FONTS
    namespace detail {
        std::string make_exact_font_path(std::string_view base_path,
                                         int px,
                                         FontWeight weight,
                                         std::string_view variation_tokens = {}) {
            std::string out{};
            out.assign(base_path.begin(), base_path.end());
            out += "#px";
            out += std::to_string(px);
            switch (weight) {
            case FontWeight::Bold:
                out += "_bold";
                break;
            case FontWeight::Medium:
                out += "_medium";
                break;
            case FontWeight::Regular:
            default:
                break;
            }
            if (!variation_tokens.empty()) {
                out += "_";
                out.append(variation_tokens.begin(), variation_tokens.end());
            }
            return out;
        }

        const Font& load_player_exact_font(std::string_view resolved_path,
                                           int px,
                                           FontWeight weight) noexcept {
            auto& state = freetype_state();
            const std::uint64_t use_tick = ++state.exact_font_use_tick;
            for (auto& slot : state.exact_fonts) {
                if (slot.loaded && slot.path == resolved_path) {
                    slot.last_used = use_tick;
                    return slot.font;
                }
            }
            auto* free_slot = &state.exact_fonts[0];
            for (auto& slot : state.exact_fonts) {
                if (!slot.loaded) {
                    free_slot = &slot;
                    break;
                }
            }
            if (free_slot->loaded) {
                for (auto& slot : state.exact_fonts) {
                    if (!slot.loaded) continue;
                    if (slot.last_used < free_slot->last_used) {
                        free_slot = &slot;
                    }
                }
            }
            const auto api = state.loader.vfs_api();
            if (free_slot->loaded) {
                if (api.reset) {
                    api.reset(&state.loader, free_slot->font);
                } else {
                    free_slot->font = Font{};
                }
            }
            free_slot->path.assign(resolved_path.begin(), resolved_path.end());
            free_slot->px = px;
            free_slot->weight = weight;
            free_slot->last_used = use_tick;
            free_slot->loaded = false;
            free_slot->font = Font{};
            if (api.load && api.load(&state.loader, free_slot->path, free_slot->font)) {
                free_slot->loaded = true;
                return free_slot->font;
            }
            if (api.reset) {
                api.reset(&state.loader, free_slot->font);
            }
            free_slot->path.clear();
            free_slot->px = 0;
            free_slot->weight = FontWeight::Regular;
            free_slot->last_used = 0;
            const auto& fallback = fallback_font_for_px(px, weight);
            return fallback;
        }
    }

    void bind_font_package(const charm::font::FontPackageConfig& config,
                           const charm::font::VfsFontLoaderApi& loader,
                           void* ctx) noexcept {
        auto& state = detail::font_package_state();
        state.package.set_config(config);
        state.package.set_loader(loader, ctx);
        state.package.bind();
        state.bound = true;
    }

    void bind_player_freetype_font(std::string_view ttf_path,
                                   std::string_view fallback_ttf_path,
                                   int small_px,
                                   int normal_px,
                                   int large_px) noexcept {
        if (ttf_path.empty()) return;
        auto& state = detail::freetype_state();
        detail::reset_exact_font_cache(state);
        state.ttf_path.assign(ttf_path.begin(), ttf_path.end());
        state.ttf_fallback_path.assign(fallback_ttf_path.begin(), fallback_ttf_path.end());
        state.ttf_small = state.ttf_path + "#small";
        state.ttf_normal = state.ttf_path + "#normal";
        state.ttf_large = state.ttf_path + "#large";
        state.ttf_mono = state.ttf_path + "#mono";
        state.ttf_small_medium = state.ttf_path + "#small_medium";
        state.ttf_normal_medium = state.ttf_path + "#normal_medium";
        state.ttf_large_medium = state.ttf_path + "#large_medium";
        state.ttf_mono_medium = state.ttf_path + "#mono_medium";
        state.ttf_small_bold = state.ttf_path + "#small_bold";
        state.ttf_normal_bold = state.ttf_path + "#normal_bold";
        state.ttf_large_bold = state.ttf_path + "#large_bold";
        state.ttf_mono_bold = state.ttf_path + "#mono_bold";
        state.ttf_fallback.clear();
        if (!state.ttf_fallback_path.empty()) {
            state.ttf_fallback = state.ttf_fallback_path + "#normal";
        }
        const char* path_small = state.ttf_small.c_str();
        const char* path_normal = state.ttf_normal.c_str();
        const char* path_large = state.ttf_large.c_str();
        const char* path_mono = state.ttf_mono.c_str();
        const char* path_small_medium = state.ttf_small_medium.c_str();
        const char* path_normal_medium = state.ttf_normal_medium.c_str();
        const char* path_large_medium = state.ttf_large_medium.c_str();
        const char* path_mono_medium = state.ttf_mono_medium.c_str();
        const char* path_small_bold = state.ttf_small_bold.c_str();
        const char* path_normal_bold = state.ttf_normal_bold.c_str();
        const char* path_large_bold = state.ttf_large_bold.c_str();
        const char* path_mono_bold = state.ttf_mono_bold.c_str();
        const char* path_fallback = state.ttf_fallback.empty() ? nullptr : state.ttf_fallback.c_str();

        charm::font::FontPackageConfig pkg{};
        pkg.regular.small_path = path_small;
        pkg.regular.normal_path = path_normal;
        pkg.regular.large_path = path_large;
        pkg.regular.mono_path = path_mono;
        pkg.medium.small_path = path_small_medium;
        pkg.medium.normal_path = path_normal_medium;
        pkg.medium.large_path = path_large_medium;
        pkg.medium.mono_path = path_mono_medium;
        pkg.bold.small_path = path_small_bold;
        pkg.bold.normal_path = path_normal_bold;
        pkg.bold.large_path = path_large_bold;
        pkg.bold.mono_path = path_mono_bold;
        pkg.fallback_path = path_fallback;

        charm::font::FreetypeFontLoaderConfig loader_cfg{};
        loader_cfg.regular.small_path = path_small;
        loader_cfg.regular.normal_path = path_normal;
        loader_cfg.regular.large_path = path_large;
        loader_cfg.regular.mono_path = path_mono;
        loader_cfg.medium.small_path = path_small_medium;
        loader_cfg.medium.normal_path = path_normal_medium;
        loader_cfg.medium.large_path = path_large_medium;
        loader_cfg.medium.mono_path = path_mono_medium;
        loader_cfg.bold.small_path = path_small_bold;
        loader_cfg.bold.normal_path = path_normal_bold;
        loader_cfg.bold.large_path = path_large_bold;
        loader_cfg.bold.mono_path = path_mono_bold;
        loader_cfg.small_px = small_px;
        loader_cfg.normal_px = normal_px;
        loader_cfg.large_px = large_px;
        loader_cfg.mono_px = normal_px;

        state.loader.set_config(loader_cfg);
        state.loader.bind_glyph_loader();
        bind_font_package(pkg, state.loader.vfs_api(), &state.loader);
        state.ready = true;
    }

    const Font& get_player_font_px(int px, FontWeight weight) noexcept {
        auto& state = detail::freetype_state();
        if (!state.ready || state.ttf_path.empty()) {
            const auto& fallback = detail::fallback_font_for_px(px, weight);
            return fallback;
        }
        const auto path = detail::make_exact_font_path(state.ttf_path, px, weight);
        return detail::load_player_exact_font(path, px, weight);
    }

    const Font& get_player_font_px_variant(int px,
                                           FontWeight weight,
                                           std::string_view variation_tokens) noexcept {
        auto& state = detail::freetype_state();
        if (!state.ready || state.ttf_path.empty()) {
            return get_player_font_px(px, weight);
        }
        const auto path = detail::make_exact_font_path(state.ttf_path, px, weight, variation_tokens);
        return detail::load_player_exact_font(path, px, weight);
    }
#else
    void bind_player_freetype_font(std::string_view,
                                   std::string_view,
                                   int,
                                   int,
                                   int) noexcept {}

    const Font& get_player_font_px(int px, FontWeight weight) noexcept {
        return detail::fallback_font_for_px(px, weight);
    }

    const Font& get_player_font_px_variant(int px,
                                           FontWeight weight,
                                           std::string_view) noexcept {
        return get_player_font_px(px, weight);
    }
#endif

    inline void apply_player_theme() {
        set_default_font(FontId::Small, &font_noto_ascii_12);
        set_default_font(FontId::Normal, &font_noto_ascii_16);
        set_default_font(FontId::Large, &font_noto_ascii_16);
        set_default_font(FontId::Mono, &font_noto_ascii_16);
        set_default_font_weight(FontId::Small, FontWeight::Regular, &font_noto_ascii_12);
        set_default_font_weight(FontId::Normal, FontWeight::Regular, &font_noto_ascii_16);
        set_default_font_weight(FontId::Large, FontWeight::Regular, &font_noto_ascii_16);
        set_default_font_weight(FontId::Mono, FontWeight::Regular, &font_noto_ascii_16);

        const bool system_fallback_ready =
            detail::system_font_fallback_enabled_state() && player::font_cache::init();
        if (!system_fallback_ready) {
            set_default_fallback_font(&font_noto_sc_16);
        }

        auto& theme = Theme::instance();
        theme.set_default_font(player_default_font());

        auto set_player_style_class = [&](PlayerStyleClass style_class, const StylePatch& patch) {
            theme.set_style_class(static_cast<StyleClassId>(style_class), patch);
        };
        auto make_control_button_patch = [](rgba bg_color,
                                            rgba border_color,
                                            int corner_radius,
                                            rgba shadow_color,
                                            int shadow_offset_y,
                                            int shadow_radius) {
            StylePatch patch{};
            patch.has_corner_radius = true;
            patch.corner_radius = corner_radius;
            patch.has_bg_color = true;
            patch.bg_color = bg_color;
            patch.has_border_color = true;
            patch.border_color = border_color;
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = shadow_color.a > 0 && shadow_radius > 0;
            if (patch.shadow_enabled) {
                patch.has_shadow_color = true;
                patch.shadow_color = shadow_color;
                patch.has_shadow_offset_x = true;
                patch.shadow_offset_x = 0;
                patch.has_shadow_offset_y = true;
                patch.shadow_offset_y = shadow_offset_y;
                patch.has_shadow_spread = true;
                patch.shadow_spread = 0;
                patch.has_shadow_radius = true;
                patch.shadow_radius = shadow_radius;
            }
            return patch;
        };
        auto make_flat_button_patch = [](rgba bg_color, rgba font_color, int corner_radius) {
            StylePatch patch{};
            patch.has_corner_radius = true;
            patch.corner_radius = corner_radius;
            patch.has_bg_color = true;
            patch.bg_color = bg_color;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_font_color = true;
            patch.font_color = font_color;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = false;
            return patch;
        };

        {
            set_player_style_class(PlayerStyleClass::ControlSide,
                                   make_control_button_patch({kUiControlSideBg.r,
                                                              kUiControlSideBg.g,
                                                              kUiControlSideBg.b,
                                                              188},
                                                             {0, 0, 0, 0},
                                                             28,
                                                             {0, 0, 0, 0},
                                                             0,
                                                             0));

            set_player_style_class(PlayerStyleClass::ControlPlay,
                                   make_control_button_patch({kUiPlayBg.r,
                                                              kUiPlayBg.g,
                                                              kUiPlayBg.b,
                                                              236},
                                                             {0, 0, 0, 0},
                                                             28,
                                                             {0, 0, 0, 0},
                                                             0,
                                                             0));
        }

        {
            StylePatch patch = ::ui::scene::make_pill_surface_patch({
                .bg_color = kUiButtonBg,
                .border_color = kUiButtonBorder,
                .corner_radius = 16,
            });
            set_player_style_class(PlayerStyleClass::TopBarButton, patch);
        }

        {
            StylePatch patch = ::ui::scene::make_pill_surface_patch({
                .bg_color = kUiTabBg,
                .border_color = kUiButtonBorder,
                .corner_radius = 0,
            });
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::TabBase), patch);
        }

        {
            StylePatch patch = ::ui::scene::make_pill_surface_patch({
                .apply_bg_color = false,
                .apply_border_color = false,
                .apply_corner_radius = false,
            });
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = false;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::ShuffleShadow), patch);
        }

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiListBg;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 16;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = false;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::ListCard), patch);
        }

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiLibraryCardTop;
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = false;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 28;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = false;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::LibraryListCard), patch);
        }

        {
            StylePatch patch = ::ui::scene::make_pill_surface_patch({
                .bg_color = kUiInfoTagBgSoft,
                .border_color = kUiInfoTagBorderSoft,
                .corner_radius = 10,
            });
            patch.has_font_color = true;
            patch.font_color = kUiTimeSoft;
            patch.has_font_role = true;
            patch.font_role = FontId::Small;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::InfoTag), patch);
        }

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiBottomBarBg;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 20;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = false;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::BottomBar), patch);
        }
        {
            set_player_style_class(PlayerStyleClass::BottomButton,
                                   make_flat_button_patch(kUiBottomButtonBg,
                                                          kUiBottomButtonFg,
                                                          20));

            set_player_style_class(PlayerStyleClass::BottomPlay,
                                   make_flat_button_patch(kUiBottomPlayBg,
                                                          kUiBottomPlayFg,
                                                          22));
        }
        Style baseline = theme.get<Button>();
        baseline.font = &player_default_font();
        baseline.colors.border_color = kUiButtonBorder;
        baseline.colors.border_focus = kUiOk;
        baseline.metrics.padding = 8;
        baseline.metrics.corner_radius = 14;
        apply_baseline_theme_preset(baseline);

        ThemePreset preset{};
        preset.has_label = true;
        preset.label = theme.get<Label>();
        preset.label.colors.font_color = kUiTitle;
        preset.has_button = true;
        preset.button = theme.get<Button>();
        preset.button.colors.bg_color = kUiButtonBg;
        preset.button.colors.border_color = kUiButtonBorder;
        preset.button.metrics.padding = 8;
        preset.button.metrics.corner_radius = 14;
        preset.button.colors.font_color = kUiListFont;
        preset.has_list_view = true;
        preset.list_view = theme.get<ListView>();
        preset.list_view.colors.bg_color = kUiListBg;
        preset.list_view.colors.border_color = kUiListBorder;
        preset.list_view.colors.accent_color = kUiLibraryListAccent;
        preset.list_view.colors.on_accent = kUiLibraryListOnAccent;
        preset.list_view.colors.border_focus = kUiLibraryPathBorderActive;
        preset.list_view.font_weight = FontWeight::Medium;
        preset.list_view.metrics.corner_radius = 18;
        preset.list_view.metrics.padding = 14;
        preset.list_view.colors.font_color = kUiListFont;
        preset.has_progress = true;
        preset.progress = theme.get<Progress>();
        preset.progress.colors.bg_color = kUiProgressBg;
        preset.progress.colors.border_color = kUiProgressBorder;
        preset.has_scroll_bar = true;
        preset.scroll_bar = theme.get<ScrollBar>();
        preset.scroll_bar.colors.bg_color = kUiScrollBg;
        preset.scroll_bar.colors.border_color = kUiScrollBorder;
        preset.scroll_bar.metrics.corner_radius = 10;
        apply_theme_preset(preset);

        auto& sheet = StyleSheet::instance();
        auto set_base = [&](WidgetKind kind, Style s) {
            if (!s.font) {
                s.font = &player_default_font();
            }
            sheet.set_base_style(kind, s);
        };

        Style scroll_container = theme.get<ScrollContainer>();
        scroll_container.colors.bg_color = kUiBackground;
        scroll_container.colors.border_color = kUiBackground;
        set_base(WidgetKind::ScrollContainer, scroll_container);

        Style progress_bar_simple = theme.get<ProgressBarSimple>();
        progress_bar_simple.colors.bg_color = kUiProgressBg;
        progress_bar_simple.colors.border_color = kUiProgressBorder;
        set_base(WidgetKind::ProgressBarSimple, progress_bar_simple);

        Style segmented_control = theme.get<SegmentedControl>();
        segmented_control.colors.bg_color = kUiButtonBg;
        segmented_control.colors.border_color = kUiButtonBorder;
        segmented_control.colors.bg_pressed = kUiButtonHover;
        segmented_control.colors.border_pressed = kUiButtonBorder;
        segmented_control.colors.font_color = kUiListFont;
        segmented_control.metrics.corner_radius = 14;
        segmented_control.metrics.padding = 6;
        set_base(WidgetKind::SegmentedControl, segmented_control);

        Style perf_overlay = theme.get<PerfOverlay>();
        perf_overlay.colors.bg_color = kUiPerfBg;
        perf_overlay.colors.border_color = kUiPerfBorder;
        perf_overlay.colors.font_color = kUiPerfFont;
        perf_overlay.metrics.padding = 6;
        set_base(WidgetKind::PerfOverlay, perf_overlay);

        Style foldable_panel = theme.get<FoldablePanel>();
        foldable_panel.metrics.header_padding = 10;
        foldable_panel.metrics.content_padding = 10;
        set_base(WidgetKind::FoldablePanel, foldable_panel);

        Style cloudy_glass = theme.get<CloudyGlass>();
        cloudy_glass.metrics.glass_highlight_pos = 12;
        cloudy_glass.metrics.glass_highlight_alpha = 90;
        cloudy_glass.metrics.glass_shadow_alpha = 50;
        cloudy_glass.metrics.glass_opacity_min = 60;
        cloudy_glass.metrics.glass_opacity_max = 200;
        set_base(WidgetKind::CloudyGlass, cloudy_glass);

        Style spectrum_view = theme.get<SpectrumView>();
        spectrum_view.colors.bg_color = kUiListBg;
        spectrum_view.colors.border_color = kUiListBorder;
        spectrum_view.colors.bg_pressed = kUiSwitchOn;
        spectrum_view.colors.border_focus = kUiOk;
        set_base(WidgetKind::SpectrumView, spectrum_view);

        Style battery_gasgauge = theme.get<BatteryGasGauge>();
        battery_gasgauge.colors.bg_color = kUiListBg;
        battery_gasgauge.colors.border_color = kUiListBorder;
        battery_gasgauge.metrics.corner_radius = 12;
        battery_gasgauge.metrics.padding = 6;
        set_base(WidgetKind::BatteryGasGauge, battery_gasgauge);

        Style histogram = theme.get<Histogram>();
        histogram.colors.bg_color = kUiListBg;
        histogram.colors.border_color = kUiListBorder;
        histogram.metrics.corner_radius = 12;
        histogram.metrics.padding = 6;
        histogram.colors.font_color = kUiListFont;
        set_base(WidgetKind::Histogram, histogram);

        Style busy_wheel = theme.get<BusyWheel>();
        busy_wheel.colors.font_color = kUiListFont;
        set_base(WidgetKind::BusyWheel, busy_wheel);

        Style console_box = theme.get<ConsoleBox>();
        console_box.colors.bg_color = kUiListBg;
        console_box.colors.border_color = kUiListBorder;
        console_box.metrics.padding = 6;
        set_base(WidgetKind::ConsoleBox, console_box);

        sheet.clear();
        StylePatch btn_base{};
        btn_base.has_bg_color = true;
        btn_base.bg_color = kUiButtonBg;
        btn_base.has_border_color = true;
        btn_base.border_color = kUiButtonBorder;
        sheet.add_rule({WidgetKind::Button, 0}, btn_base);

        StylePatch btn_hover{};
        btn_hover.has_bg_color = true;
        btn_hover.bg_color = kUiButtonHover;
        sheet.add_rule({WidgetKind::Button, static_cast<std::uint8_t>(StyleStateFlag::Hovered)}, btn_hover);

        StylePatch btn_pressed{};
        btn_pressed.has_bg_color = true;
        btn_pressed.bg_color = kUiSwitchOn;
        btn_pressed.has_border_color = true;
        btn_pressed.border_color = kUiSwitchOn;
        sheet.add_rule({WidgetKind::Button, static_cast<std::uint8_t>(StyleStateFlag::Pressed)}, btn_pressed);

        StylePatch chart_patch{};
        chart_patch.has_bg_color = true;
        chart_patch.bg_color = kUiListBg;
        chart_patch.has_border_color = true;
        chart_patch.border_color = kUiListBorder;
        chart_patch.has_font_color = true;
        chart_patch.font_color = kUiListFont;
        theme.patch<Chart>(chart_patch);

        StylePatch hist_patch = chart_patch;
        theme.patch<HistogramView>(hist_patch);
        theme.patch<Histogram>(hist_patch);

        StylePatch switch_patch{};
        switch_patch.has_bg_color = true;
        switch_patch.bg_color = kUiButtonBg;
        switch_patch.has_border_color = true;
        switch_patch.border_color = kUiButtonBorder;
        switch_patch.has_bg_pressed = true;
        switch_patch.bg_pressed = kUiSwitchOn;
        switch_patch.has_border_pressed = true;
        switch_patch.border_pressed = kUiSwitchOn;
        theme.patch<Switch>(switch_patch);

        StylePatch slider_patch{};
        slider_patch.has_bg_color = true;
        slider_patch.bg_color = kUiButtonBg;
        slider_patch.has_border_color = true;
        slider_patch.border_color = kUiButtonBorder;
        theme.patch<Slider>(slider_patch);

        StylePatch dropdown_patch{};
        dropdown_patch.has_bg_color = true;
        dropdown_patch.bg_color = kUiButtonBg;
        dropdown_patch.has_border_color = true;
        dropdown_patch.border_color = kUiButtonBorder;
        dropdown_patch.has_font_color = true;
        dropdown_patch.font_color = kUiListFont;
        theme.patch<Dropdown>(dropdown_patch);

        StylePatch image_patch{};
        image_patch.has_corner_radius = true;
        image_patch.corner_radius = 24;
        image_patch.has_border_width = true;
        image_patch.border_width = 0;
        image_patch.has_inner_stroke_enabled = true;
        image_patch.inner_stroke_enabled = false;
        image_patch.has_outline_enabled = true;
        image_patch.outline_enabled = false;
        image_patch.has_shadow_enabled = true;
        image_patch.shadow_enabled = false;
        theme.patch<Image>(image_patch);

        StylePatch image_box_patch = chart_patch;
        theme.patch<ImageBox>(image_box_patch);

        StylePatch meter_patch = chart_patch;
        meter_patch.has_border_focus = true;
        meter_patch.border_focus = kUiOk;
        theme.patch<MeterPointer>(meter_patch);

        StylePatch drill_patch = chart_patch;
        drill_patch.has_bg_pressed = true;
        drill_patch.bg_pressed = kUiSwitchOn;
        theme.patch<ProgressBarDrill>(drill_patch);

        StylePatch wheel_patch{};
        wheel_patch.has_font_color = true;
        wheel_patch.font_color = kUiListFont;
        theme.patch<SpinningWheel>(wheel_patch);
        theme.patch<BusyWheel>(wheel_patch);
        StylePatch nebula_patch{};
        nebula_patch.has_font_color = true;
        nebula_patch.font_color = kUiListFont;
        theme.patch<DynamicNebula>(nebula_patch);

        StylePatch crt_patch = chart_patch;
        theme.patch<CrtScreen>(crt_patch);

        StylePatch spectrum_patch = chart_patch;
        spectrum_patch.has_bg_pressed = true;
        spectrum_patch.bg_pressed = kUiSwitchOn;
        spectrum_patch.has_border_focus = true;
        spectrum_patch.border_focus = kUiOk;
        theme.patch<SpectrumView>(spectrum_patch);

        sheet.rebuild_if_needed();
    }
}


