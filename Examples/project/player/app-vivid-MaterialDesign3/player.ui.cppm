module;
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

export module player.ui;

import charm.core.style;
import charm.core.style_sheet;
import charm.core.theme_preset;
import charm.gfx.color;
import charm.gfx.image;
import charm.font;
import charm.font.typography;
import charm.font.font_noto_ascii_16;
import charm.font.font_noto_sc_16;
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
    };

    inline constexpr int kUiPadding = 18;
    inline constexpr int kCoverSize = 340;
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

    struct PlayerIconIds {
        ::ui::gfx::ImageId prev{};
        ::ui::gfx::ImageId play{};
        ::ui::gfx::ImageId pause{};
        ::ui::gfx::ImageId next{};
        ::ui::gfx::ImageId loop{};
        ::ui::gfx::ImageId single{};
        ::ui::gfx::ImageId shuffle{};
        ::ui::gfx::ImageId folder{};
    };

    namespace detail {
        constexpr int kIconSize = 32;
        constexpr int kIconStride = kIconSize * 4;
        using IconBuffer = std::array<std::byte, kIconSize * kIconSize * 4>;
        struct FPoint {
            float x{};
            float y{};
        };

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

        void fill_rect(IconBuffer& buf, int size, float x0, float y0, float x1, float y1, const rgba& color) {
            const int ix0 = std::max(0, static_cast<int>(x0 * size));
            const int iy0 = std::max(0, static_cast<int>(y0 * size));
            const int ix1 = std::min(size - 1, static_cast<int>(x1 * size));
            const int iy1 = std::min(size - 1, static_cast<int>(y1 * size));
            for (int y = iy0; y <= iy1; ++y) {
                for (int x = ix0; x <= ix1; ++x) {
                    icon_set_pixel(buf, x, y, color);
                }
            }
        }

        void fill_triangle(IconBuffer& buf, int size, FPoint a, FPoint b, FPoint c, const rgba& color) {
            const auto to_px = [&](FPoint p) noexcept {
                return FPoint{p.x * (size - 1), p.y * (size - 1)};
            };
            a = to_px(a);
            b = to_px(b);
            c = to_px(c);
            const float min_y = std::min(a.y, std::min(b.y, c.y));
            const float max_y = std::max(a.y, std::max(b.y, c.y));
            const int y0 = std::max(0, static_cast<int>(std::floor(min_y)));
            const int y1 = std::min(size - 1, static_cast<int>(std::ceil(max_y)));
            for (int y = y0; y <= y1; ++y) {
                float xs[3];
                int hits = 0;
                auto edge_hit = [&](FPoint p0, FPoint p1) {
                    if ((y < p0.y && y < p1.y) || (y > p0.y && y > p1.y) || (p0.y == p1.y)) return;
                    const float t = (y - p0.y) / (p1.y - p0.y);
                    xs[hits++] = p0.x + (p1.x - p0.x) * t;
                };
                edge_hit(a, b);
                edge_hit(b, c);
                edge_hit(c, a);
                if (hits < 2) continue;
                float x_min = xs[0];
                float x_max = xs[1];
                if (x_min > x_max) std::swap(x_min, x_max);
                const int ix0 = std::max(0, static_cast<int>(std::floor(x_min)));
                const int ix1 = std::min(size - 1, static_cast<int>(std::ceil(x_max)));
                for (int x = ix0; x <= ix1; ++x) {
                    icon_set_pixel(buf, x, y, color);
                }
            }
        }

        void stroke_line(IconBuffer& buf, int size, FPoint a, FPoint b, int thickness, const rgba& color) {
            const int x0 = static_cast<int>(a.x * (size - 1));
            const int y0 = static_cast<int>(a.y * (size - 1));
            const int x1 = static_cast<int>(b.x * (size - 1));
            const int y1 = static_cast<int>(b.y * (size - 1));
            const int dx = std::abs(x1 - x0);
            const int dy = std::abs(y1 - y0);
            const int sx = (x0 < x1) ? 1 : -1;
            const int sy = (y0 < y1) ? 1 : -1;
            int err = dx - dy;
            int x = x0;
            int y = y0;
            const int half = std::max(0, thickness / 2);
            while (true) {
                for (int oy = -half; oy <= half; ++oy) {
                    for (int ox = -half; ox <= half; ++ox) {
                        icon_set_pixel(buf, x + ox, y + oy, color);
                    }
                }
                if (x == x1 && y == y1) break;
                const int e2 = 2 * err;
                if (e2 > -dy) {
                    err -= dy;
                    x += sx;
                }
                if (e2 < dx) {
                    err += dx;
                    y += sy;
                }
            }
        }

        void build_prev_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            fill_rect(buf, kIconSize, 0.12f, 0.22f, 0.20f, 0.78f, color);
            fill_triangle(buf, kIconSize,
                          FPoint{0.76f, 0.22f},
                          FPoint{0.76f, 0.78f},
                          FPoint{0.24f, 0.50f},
                          color);
        }

        void build_play_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            fill_triangle(buf, kIconSize,
                          FPoint{0.30f, 0.20f},
                          FPoint{0.30f, 0.80f},
                          FPoint{0.82f, 0.50f},
                          color);
        }

        void build_pause_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            fill_rect(buf, kIconSize, 0.32f, 0.22f, 0.44f, 0.78f, color);
            fill_rect(buf, kIconSize, 0.56f, 0.22f, 0.68f, 0.78f, color);
        }

        void build_loop_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            const int thick = 2;
            stroke_line(buf, kIconSize, {0.24f, 0.30f}, {0.76f, 0.30f}, thick, color);
            stroke_line(buf, kIconSize, {0.76f, 0.30f}, {0.76f, 0.70f}, thick, color);
            stroke_line(buf, kIconSize, {0.76f, 0.70f}, {0.24f, 0.70f}, thick, color);
            stroke_line(buf, kIconSize, {0.24f, 0.70f}, {0.24f, 0.30f}, thick, color);
            fill_triangle(buf, kIconSize,
                          FPoint{0.66f, 0.18f},
                          FPoint{0.84f, 0.30f},
                          FPoint{0.66f, 0.42f},
                          color);
            fill_triangle(buf, kIconSize,
                          FPoint{0.34f, 0.58f},
                          FPoint{0.16f, 0.70f},
                          FPoint{0.34f, 0.82f},
                          color);
        }

        void build_single_icon(IconBuffer& buf, const rgba& color) {
            build_loop_icon(buf, color);
            fill_rect(buf, kIconSize, 0.48f, 0.32f, 0.52f, 0.68f, color);
            fill_rect(buf, kIconSize, 0.44f, 0.32f, 0.56f, 0.36f, color);
        }

        void build_shuffle_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            const int thick = 2;
            stroke_line(buf, kIconSize, {0.22f, 0.30f}, {0.78f, 0.70f}, thick, color);
            stroke_line(buf, kIconSize, {0.22f, 0.70f}, {0.78f, 0.30f}, thick, color);
            fill_triangle(buf, kIconSize,
                          FPoint{0.70f, 0.22f},
                          FPoint{0.86f, 0.30f},
                          FPoint{0.70f, 0.38f},
                          color);
            fill_triangle(buf, kIconSize,
                          FPoint{0.70f, 0.62f},
                          FPoint{0.86f, 0.70f},
                          FPoint{0.70f, 0.78f},
                          color);
        }

        void build_next_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            fill_rect(buf, kIconSize, 0.80f, 0.22f, 0.88f, 0.78f, color);
            fill_triangle(buf, kIconSize,
                          FPoint{0.24f, 0.22f},
                          FPoint{0.24f, 0.78f},
                          FPoint{0.76f, 0.50f},
                          color);
        }

        void build_folder_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            fill_rect(buf, kIconSize, 0.18f, 0.40f, 0.82f, 0.80f, color);
            fill_rect(buf, kIconSize, 0.26f, 0.26f, 0.58f, 0.44f, color);
            fill_rect(buf, kIconSize, 0.18f, 0.36f, 0.26f, 0.42f, color);
            fill_rect(buf, kIconSize, 0.58f, 0.36f, 0.82f, 0.42f, color);
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
        out.loop = reg(icon_loop());
        out.single = reg(icon_single());
        out.shuffle = reg(icon_shuffle());
        out.folder = reg(icon_folder());
        return out;
    }

    inline const Font& player_default_font() noexcept {
        return get_font(FontId::Normal);
    }

    inline void apply_player_theme() {
        set_default_font(FontId::Small, &font_noto_ascii_16);
        set_default_font(FontId::Normal, &font_noto_ascii_16);
        set_default_font(FontId::Large, &font_noto_ascii_16);
        set_default_font(FontId::Mono, &font_noto_ascii_16);
        if (!player::font_cache::init()) {
            set_default_fallback_font(&font_noto_sc_16);
        }

        auto& theme = Theme::instance();
        theme.set_default_font(player_default_font());

        {
            StylePatch side{};
            side.has_corner_radius = true;
            side.corner_radius = 24;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::ControlSide), side);

            StylePatch play{};
            play.has_corner_radius = true;
            play.corner_radius = 24;
            play.has_shadow_enabled = true;
            play.shadow_enabled = true;
            play.has_shadow_color = true;
            play.shadow_color = kUiPlayShadow;
            play.has_shadow_offset_x = true;
            play.shadow_offset_x = 0;
            play.has_shadow_offset_y = true;
            play.shadow_offset_y = 6;
            play.has_shadow_spread = true;
            play.shadow_spread = 0;
            play.has_shadow_radius = true;
            play.shadow_radius = 18;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::ControlPlay), play);
        }

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiButtonBg;
            patch.has_border_color = true;
            patch.border_color = kUiButtonBorder;
            patch.has_corner_radius = true;
            patch.corner_radius = 12;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::TopBarButton), patch);
        }

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiTabBg;
            patch.has_border_color = true;
            patch.border_color = kUiButtonBorder;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::TabBase), patch);
        }

        {
            StylePatch patch{};
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_x = true;
            patch.shadow_offset_x = 0;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 3;
            patch.has_shadow_spread = true;
            patch.shadow_spread = 3;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 16;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::ShuffleShadow), patch);
        }

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiListBg;
            patch.has_border_color = true;
            patch.border_color = kUiListBorder;
            patch.has_corner_radius = true;
            patch.corner_radius = 16;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_x = true;
            patch.shadow_offset_x = 0;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 4;
            patch.has_shadow_spread = true;
            patch.shadow_spread = 4;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 18;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::ListCard), patch);
        }

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiInfoTagBg;
            patch.has_border_color = true;
            patch.border_color = kUiButtonBorder;
            patch.has_corner_radius = true;
            patch.corner_radius = 10;
            patch.has_font_color = true;
            patch.font_color = kUiTime;
            patch.has_font = true;
            patch.font = &get_font(FontId::Small);
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::InfoTag), patch);
        }

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiListBg;
            patch.has_border_color = true;
            patch.border_color = kUiListBorder;
            patch.has_corner_radius = true;
            patch.corner_radius = 18;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_x = true;
            patch.shadow_offset_x = 0;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 4;
            patch.has_shadow_spread = true;
            patch.shadow_spread = 6;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 18;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::BottomBar), patch);
        }
        {
            StylePatch patch{};
            patch.has_corner_radius = true;
            patch.corner_radius = 16;
            patch.has_bg_color = true;
            patch.bg_color = kUiButtonBg;
            patch.has_border_color = true;
            patch.border_color = kUiButtonBorder;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 3;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 10;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::BottomButton), patch);

            StylePatch play = patch;
            play.corner_radius = 18;
            play.has_shadow_offset_y = true;
            play.shadow_offset_y = 4;
            play.has_shadow_radius = true;
            play.shadow_radius = 14;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::BottomPlay), play);
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
        preset.list_view.colors.on_accent = kUiListFont;
        preset.list_view.metrics.corner_radius = 12;
        preset.list_view.metrics.padding = 12;
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
