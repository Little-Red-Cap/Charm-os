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
import charm.font.font_noto_ascii_16;
import charm.font.font_noto_sc_16;
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
import charm.widgets.image_box;
import charm.widgets.meter_pointer;
import charm.widgets.list_view;
import charm.widgets.progress;
import charm.widgets.battery_gasgauge;
import charm.widgets.progress_bar_drill;
import charm.widgets.progress_bar_simple;
import charm.widgets.scrollbar;
import charm.widgets.segmented_control;
import charm.widgets.perf_overlay;
import charm.widgets.switcher;
import charm.widgets.slider;
import charm.widgets.dropdown;

export namespace player::ui {
    inline constexpr int kUiPadding = 24;
    inline constexpr int kCoverSize = 320;
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
    inline constexpr int kEqPanelHeight = 148;
    inline constexpr int kEqTitleHeight = 18;
    inline constexpr int kEqRowHeight = 22;
    inline constexpr int kEqRowGap = 6;
    inline constexpr int kEqLabelWidth = 40;
    inline constexpr int kEqValueWidth = 44;
    inline constexpr int kEqRowGapX = 10;
    inline constexpr int kEqPresetLabelWidth = 56;
    inline constexpr int kEqPresetWidth = 140;
    inline constexpr int kListTitleGap = 26;
    inline constexpr int kListBottomReserve = 150;
    inline constexpr int kListScrollWidth = 10;
    inline constexpr int kControlsBottomMargin = 18;
    inline constexpr int kButtonWidth = 112;
    inline constexpr int kButtonHeight = 52;
    inline constexpr int kButtonGap = 10;
    inline constexpr int kModeButtonWidth = 76;
    inline constexpr int kPerfOverlayWidth = 300;
    inline constexpr int kPerfOverlayHeight = 72;

    inline constexpr rgba kUiBackground = {18, 20, 28, 255};
    inline constexpr rgba kUiCover = {32, 36, 52, 255};
    inline constexpr rgba kUiTitle = {236, 238, 246, 255};
    inline constexpr rgba kUiSubtitle = {156, 162, 188, 255};
    inline constexpr rgba kUiTime = {136, 142, 166, 255};
    inline constexpr rgba kUiStatus = {140, 150, 175, 255};
    inline constexpr rgba kUiListTitle = {190, 196, 218, 255};
    inline constexpr rgba kUiHint = {120, 128, 150, 255};
    inline constexpr rgba kUiError = {220, 120, 120, 255};
    inline constexpr rgba kUiOk = {120, 200, 170, 255};
    inline constexpr rgba kUiPaused = {230, 185, 90, 255};
    inline constexpr rgba kUiOption = {170, 178, 205, 255};
    inline constexpr rgba kUiSwitchOn = {90, 160, 210, 255};
    inline constexpr rgba kUiEqTitle = {200, 206, 228, 255};

    inline constexpr rgba kUiButtonBg = {26, 30, 44, 255};
    inline constexpr rgba kUiButtonBorder = {70, 90, 120, 255};
    inline constexpr rgba kUiButtonHover = {44, 60, 82, 255};
    inline constexpr rgba kUiListBg = {22, 24, 34, 255};
    inline constexpr rgba kUiListBorder = {60, 70, 90, 255};
    inline constexpr rgba kUiListFont = {210, 220, 240, 255};
    inline constexpr rgba kUiProgressBg = {28, 32, 46, 255};
    inline constexpr rgba kUiProgressBorder = {90, 110, 150, 255};
    inline constexpr rgba kUiScrollBg = {30, 34, 48, 255};
    inline constexpr rgba kUiScrollBorder = {70, 90, 120, 255};
    inline constexpr rgba kUiPerfBg = {24, 26, 36, 230};
    inline constexpr rgba kUiPerfBorder = {70, 90, 120, 255};
    inline constexpr rgba kUiPerfFont = {220, 228, 242, 255};

    namespace detail {
        constexpr int kIconSize = 16;
        constexpr int kIconStride = kIconSize * 4;
        using IconBuffer = std::array<std::byte, kIconSize * kIconSize * 4>;

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

        void build_prev_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            constexpr int center = 7;
            constexpr int top = 3;
            constexpr int bottom = 12;
            constexpr int base_half = (bottom - top) / 2;
            constexpr int apex_x = 3;
            constexpr int base_x = 11;
            for (int y = top; y <= bottom; ++y) {
                for (int x = apex_x; x <= base_x; ++x) {
                    const int span = (x - apex_x) * base_half / (base_x - apex_x);
                    if (std::abs(y - center) <= span) {
                        icon_set_pixel(buf, x, y, color);
                    }
                }
            }
            for (int y = top; y <= bottom; ++y) {
                icon_set_pixel(buf, 1, y, color);
                icon_set_pixel(buf, 2, y, color);
            }
        }

        void build_play_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            constexpr int center = 7;
            constexpr int top = 3;
            constexpr int bottom = 12;
            constexpr int base_half = (bottom - top) / 2;
            constexpr int base_x = 4;
            constexpr int apex_x = 12;
            for (int y = top; y <= bottom; ++y) {
                for (int x = base_x; x <= apex_x; ++x) {
                    const int span = (apex_x - x) * base_half / (apex_x - base_x);
                    if (std::abs(y - center) <= span) {
                        icon_set_pixel(buf, x, y, color);
                    }
                }
            }
        }

        void build_pause_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            for (int y = 3; y <= 12; ++y) {
                icon_set_pixel(buf, 5, y, color);
                icon_set_pixel(buf, 6, y, color);
                icon_set_pixel(buf, 10, y, color);
                icon_set_pixel(buf, 11, y, color);
            }
        }

        void build_loop_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            for (int x = 4; x <= 11; ++x) {
                icon_set_pixel(buf, x, 4, color);
                icon_set_pixel(buf, x, 11, color);
            }
            for (int y = 4; y <= 11; ++y) {
                icon_set_pixel(buf, 4, y, color);
                icon_set_pixel(buf, 11, y, color);
            }
            // arrow heads (top right, bottom left)
            icon_set_pixel(buf, 10, 3, color);
            icon_set_pixel(buf, 11, 4, color);
            icon_set_pixel(buf, 12, 5, color);
            icon_set_pixel(buf, 5, 12, color);
            icon_set_pixel(buf, 4, 11, color);
            icon_set_pixel(buf, 3, 10, color);
        }

        void build_single_icon(IconBuffer& buf, const rgba& color) {
            build_loop_icon(buf, color);
            for (int y = 5; y <= 10; ++y) {
                icon_set_pixel(buf, 7, y, color);
                icon_set_pixel(buf, 8, y, color);
            }
            icon_set_pixel(buf, 6, 5, color);
            icon_set_pixel(buf, 9, 5, color);
        }

        void build_shuffle_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            for (int x = 4; x <= 11; ++x) {
                const int y1 = 5 + (x - 4) / 2;
                const int y2 = 10 - (x - 4) / 2;
                icon_set_pixel(buf, x, y1, color);
                icon_set_pixel(buf, x, y2, color);
            }
            // arrow heads (right side)
            icon_set_pixel(buf, 11, 5, color);
            icon_set_pixel(buf, 12, 6, color);
            icon_set_pixel(buf, 11, 10, color);
            icon_set_pixel(buf, 12, 9, color);
            // arrow heads (left side)
            icon_set_pixel(buf, 4, 6, color);
            icon_set_pixel(buf, 3, 7, color);
            icon_set_pixel(buf, 4, 9, color);
            icon_set_pixel(buf, 3, 8, color);
        }

        void build_next_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            constexpr int center = 7;
            constexpr int top = 3;
            constexpr int bottom = 12;
            constexpr int base_half = (bottom - top) / 2;
            constexpr int base_x = 4;
            constexpr int apex_x = 12;
            for (int y = top; y <= bottom; ++y) {
                for (int x = base_x; x <= apex_x; ++x) {
                    const int span = (apex_x - x) * base_half / (apex_x - base_x);
                    if (std::abs(y - center) <= span) {
                        icon_set_pixel(buf, x, y, color);
                    }
                }
            }
            for (int y = top; y <= bottom; ++y) {
                icon_set_pixel(buf, 13, y, color);
                icon_set_pixel(buf, 14, y, color);
            }
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

    inline const Font& player_default_font() noexcept {
        return get_font(FontId::Normal);
    }

    inline void apply_player_theme() {
        auto& theme = Theme::instance();
        theme.set_default_font(player_default_font());
        Style baseline = theme.get<Button>();
        baseline.font = &player_default_font();
        baseline.border_color = kUiButtonBorder;
        baseline.border_focus = kUiOk;
        baseline.padding = 8;
        baseline.corner_radius = 8;
        apply_baseline_theme_preset(baseline);

        ThemePreset preset{};
        preset.has_label = true;
        preset.label = theme.get<Label>();
        preset.label.font_color = kUiTitle;
        preset.has_button = true;
        preset.button = theme.get<Button>();
        preset.button.bg_color = kUiButtonBg;
        preset.button.border_color = kUiButtonBorder;
        preset.button.padding = 8;
        preset.button.font_color = kUiListFont;
        preset.has_list_view = true;
        preset.list_view = theme.get<ListView>();
        preset.list_view.bg_color = kUiListBg;
        preset.list_view.border_color = kUiListBorder;
        preset.list_view.corner_radius = 6;
        preset.list_view.padding = 6;
        preset.list_view.font_color = kUiListFont;
        preset.has_progress = true;
        preset.progress = theme.get<Progress>();
        preset.progress.bg_color = kUiProgressBg;
        preset.progress.border_color = kUiProgressBorder;
        preset.has_progress_bar_simple = true;
        preset.progress_bar_simple = theme.get<ProgressBarSimple>();
        preset.progress_bar_simple.bg_color = kUiProgressBg;
        preset.progress_bar_simple.border_color = kUiProgressBorder;
        preset.has_scroll_bar = true;
        preset.scroll_bar = theme.get<ScrollBar>();
        preset.scroll_bar.bg_color = kUiScrollBg;
        preset.scroll_bar.border_color = kUiScrollBorder;
        preset.scroll_bar.corner_radius = 6;
        preset.has_segmented_control = true;
        preset.segmented_control = theme.get<SegmentedControl>();
        preset.segmented_control.bg_color = kUiButtonBg;
        preset.segmented_control.border_color = kUiButtonBorder;
        preset.segmented_control.bg_pressed = kUiButtonHover;
        preset.segmented_control.border_pressed = kUiButtonBorder;
        preset.segmented_control.font_color = kUiListFont;
        preset.has_perf_overlay = true;
        preset.perf_overlay = theme.get<PerfOverlay>();
        preset.perf_overlay.bg_color = kUiPerfBg;
        preset.perf_overlay.border_color = kUiPerfBorder;
        preset.perf_overlay.font_color = kUiPerfFont;
        preset.perf_overlay.padding = 6;
        preset.has_foldable_panel = true;
        preset.foldable_panel = theme.get<FoldablePanel>();
        preset.foldable_panel.header_padding = 10;
        preset.foldable_panel.content_padding = 10;
        preset.has_cloudy_glass = true;
        preset.cloudy_glass = theme.get<CloudyGlass>();
        preset.cloudy_glass.glass_highlight_pos = 12;
        preset.cloudy_glass.glass_highlight_alpha = 90;
        preset.cloudy_glass.glass_shadow_alpha = 50;
        preset.cloudy_glass.glass_opacity_min = 60;
        preset.cloudy_glass.glass_opacity_max = 200;
        preset.has_spectrum_view = true;
        preset.spectrum_view = theme.get<SpectrumView>();
        preset.spectrum_view.bg_color = kUiListBg;
        preset.spectrum_view.border_color = kUiListBorder;
        preset.spectrum_view.bg_pressed = kUiSwitchOn;
        preset.spectrum_view.border_focus = kUiOk;
        preset.has_battery_gasgauge = true;
        preset.battery_gasgauge = theme.get<BatteryGasGauge>();
        preset.battery_gasgauge.bg_color = kUiListBg;
        preset.battery_gasgauge.border_color = kUiListBorder;
        preset.battery_gasgauge.corner_radius = 6;
        preset.battery_gasgauge.padding = 6;
        preset.has_histogram = true;
        preset.histogram = theme.get<Histogram>();
        preset.histogram.bg_color = kUiListBg;
        preset.histogram.border_color = kUiListBorder;
        preset.histogram.corner_radius = 6;
        preset.histogram.padding = 6;
        preset.histogram.font_color = kUiListFont;
        preset.has_busy_wheel = true;
        preset.busy_wheel = theme.get<BusyWheel>();
        preset.busy_wheel.font_color = kUiListFont;
        preset.has_console_box = true;
        preset.console_box = theme.get<ConsoleBox>();
        preset.console_box.bg_color = kUiListBg;
        preset.console_box.border_color = kUiListBorder;
        preset.console_box.padding = 6;
        apply_theme_preset(preset);

        auto& sheet = StyleSheet::instance();
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
    }
}
