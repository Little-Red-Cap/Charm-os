module;
#include <cstdint>

export module player.ui;

import charm.core.style;
import charm.core.style_sheet;
import charm.core.theme_preset;
import charm.gfx.color;
import charm.widgets.button;
import charm.widgets.chart;
import charm.widgets.cloudy_glass;
import charm.widgets.foldable_panel;
import charm.widgets.histogram_view;
import charm.widgets.list_view;
import charm.widgets.progress;
import charm.widgets.scrollbar;
import charm.widgets.segmented_control;
import charm.widgets.perf_overlay;

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
    inline constexpr int kSpectrumHeight = 80;
    inline constexpr int kSpectrumGap = 12;
    inline constexpr int kListTitleGap = 26;
    inline constexpr int kListBottomReserve = 170;
    inline constexpr int kListScrollWidth = 10;
    inline constexpr int kControlsBottomMargin = 20;
    inline constexpr int kButtonWidth = 120;
    inline constexpr int kButtonHeight = 48;
    inline constexpr int kButtonGap = 12;
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

    inline void apply_player_theme() {
        auto& theme = Theme::instance();
        ThemePreset preset{};
        preset.has_label = true;
        preset.label = theme.get<Label>();
        preset.label.font_color = kUiTitle;
        preset.has_button = true;
        preset.button = theme.get<Button>();
        preset.button.bg_color = kUiButtonBg;
        preset.button.border_color = kUiButtonBorder;
        preset.button.padding = 6;
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
    }
}
