// app.ui.cppm

module;
#include <cstdio>
#include <iostream>
#include <span>
#include <array>
export module app.ui;

import app.state;
import app.pages;
import app.theme;
import app.icons;

import gui.core;
import gui.ui_immediate;
import gui.ui_settings;
import gui.widgets;
import gui.list_view;
import alg_list_layout;
import gui.renderer;
import gui.layout;
import gui.font;
import gui.ui_tree;
import gui.ui_hit_test;
import gui.ui_button;
import gui.ui_focus;
import gui.ui_highlight;
import gui.ui_semantics;
import gui.ui_list;
import gui.ui_feedback;
import gui.ui_context;
import gui.ui_list_page;
import gui.ui_popup;
import gui.ui_perf;
import gui.ui_vtree;
import gui.ui_list_shell;
import gui.motion;
import gui.chart_scope;
import gui.image_1bpp;
import gui.qr_widget;
// NO_LISTWRAP_IN_PAGES
// UI contract: pages must not reference ListWrap / with_wrap / NavWrap.
// Choose behavior via default_list_spec(..., ListPreset::Standard|Clamped) only.


namespace app::detail
{
    constexpr int kScopeSamples = 96;

    constexpr double kPi = 3.14159265358979323846;

    constexpr double wrap_pi(double x) noexcept
    {
        constexpr double two_pi = 2.0 * kPi;
        while (x > kPi) x -= two_pi;
        while (x < -kPi) x += two_pi;
        return x;
    }

    constexpr double sin_approx(double x) noexcept
    {
        x = wrap_pi(x);
        const double x2 = x * x;
        const double x3 = x * x2;
        const double x5 = x3 * x2;
        const double x7 = x5 * x2;
        return x - (x3 / 6.0) + (x5 / 120.0) - (x7 / 5040.0);
    }

    consteval std::array<std::uint8_t, kScopeSamples> make_sine_table() noexcept
    {
        std::array<std::uint8_t, kScopeSamples> out{};
        constexpr double two_pi = 2.0 * kPi;
        for (int i = 0; i < kScopeSamples; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(kScopeSamples);
            const double s = sin_approx(two_pi * t);
            const double v = (s * 0.5 + 0.5) * 100.0;
            int iv = static_cast<int>(v + 0.5);
            if (iv < 0) iv = 0;
            if (iv > 100) iv = 100;
            out[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(iv);
        }
        return out;
    }

    constexpr auto kSineTable = make_sine_table();


    struct TextItem {
        const char* text{nullptr};
        const gui::Font* font{nullptr};
        bool center{false};
        gui::Rect rect{};
    };

    [[nodiscard]] inline gui::Size measure_text_item(void* ctx, const gui::layout::Constraints& c) noexcept {
        auto* t = static_cast<TextItem*>(ctx);
        const int w = gui::layout::text_width(*t->font, t->text ? t->text : "");
        const int h = t->font ? t->font->line_height : 0;
        return gui::layout::clamp_size(c, gui::Size{(std::int16_t)w, (std::int16_t)h});
    }

    inline void arrange_text_item(void* ctx, const gui::Rect& r) noexcept {
        auto* t = static_cast<TextItem*>(ctx);
        t->rect = r;
    }

    struct BarItem {
        std::uint8_t percent{0};
        std::int16_t height{10};
        gui::Rect rect{};
    };

    [[nodiscard]] inline gui::Size measure_bar_item(void* ctx, const gui::layout::Constraints& c) noexcept {
        auto* b = static_cast<BarItem*>(ctx);
        return gui::layout::clamp_size(c, gui::Size{0, b->height});
    }

    inline void arrange_bar_item(void* ctx, const gui::Rect& r) noexcept {
        auto* b = static_cast<BarItem*>(ctx);
        b->rect = r;
    }

    struct ButtonItem {
        const char* label{nullptr};
        const gui::Font* font{nullptr};
        std::int16_t height{14};
        gui::Rect rect{};
    };

    [[nodiscard]] inline gui::Size measure_button_item(void* ctx, const gui::layout::Constraints& c) noexcept {
        auto* b = static_cast<ButtonItem*>(ctx);
        return gui::layout::clamp_size(c, gui::Size{0, b->height});
    }

    inline void arrange_button_item(void* ctx, const gui::Rect& r) noexcept {
        auto* b = static_cast<ButtonItem*>(ctx);
        b->rect = r;
    }

    template<class Ctx, int Max>
    using ListWidget = gui::ui::ListWidget<Ctx, Max>;

    template <class R>
    void draw_header(R& r, const char* title) noexcept {
        using gui::Rect;
        const auto& th = app::theme::current();
        r.fillRect(Rect{0, 0, R::kWidth, (std::int16_t)th.header_h}, true);
        const int base = gui::layout::baseline_from_top(*th.font_default, th.pad_xs);
        r.drawText(*th.font_default, th.pad_xs, base, title, false);
    }


    template <class R>
    void draw_fps_overlay(R& r, const AppState& s) noexcept
    {
        const auto& th = app::theme::current();
        gui::ui::draw_fps_overlay_default(r, th, s.fps.value(), gui::ui::is_on(s.ui.fps_overlay), s.fps_ui.value());
    }

    template <class R>
    void draw_footer(R& r, const char* hint) noexcept {
        const auto& th = app::theme::current();
        const int top = R::kHeight - th.footer_h;
        const int base = gui::layout::baseline_from_top(*th.font_default, top);
        r.drawText(*th.font_default, th.pad_xs, base, hint, true);
    }

    inline gui::ui::UiContext make_ui_context(AppState& s,
                                              const gui::Rect& clip,
                                              gui::ListViewport* vp) noexcept
    {
        gui::ui::UiContext ctx{};
        ctx.sem = &s.semantics;
        ctx.theme = &app::theme::current();
        ctx.settings = &s.ui;
        ctx.now_ms = s.now_ms;
        ctx.viewport = vp;
        ctx.clip = clip;
        return ctx;
    }

    [[nodiscard]] inline int focus_knob_y(const gui::Rect& area, std::int16_t count, std::int16_t focus) noexcept
    {
        if (count <= 0) return area.y;
        const int range = (count > 1) ? (count - 1) : 1;
        const int pos = (area.h - 3) * focus / range;
        return area.y + pos;
    }

    template <class R>
    void draw_focus_indicator(R& r, const gui::Rect& area, std::int16_t count, int knob_y) noexcept
    {
        if (count <= 0) return;
        const int x = area.x + area.w - 2;
        const int y0 = area.y;
        const int y1 = area.y + area.h - 1;
        if (area.h >= 2) {
            for (int xx = x - 1; xx <= x + 1; ++xx) {
                r.setPixel(xx, (std::int16_t)y0, true);
                r.setPixel(xx, (std::int16_t)y1, true);
            }
        }
        for (int y = y0 + 1; y < y1; ++y) {
            r.setPixel(x, (std::int16_t)y, true);
        }
        int knob_h = area.h / count;
        if (knob_h < 3) knob_h = 3;
        if (knob_h > area.h) knob_h = area.h;
        const int travel = area.h - knob_h;
        int ky = y0;
        if (travel > 0 && count > 1) {
            ky = y0 + (travel * knob_y) / (count - 1);
        }
        if (ky < y0) ky = y0;
        if (ky > y1 - (knob_h - 1)) ky = y1 - (knob_h - 1);
        const gui::Rect knob{(std::int16_t)(x - 1), (std::int16_t)ky, 3, (std::int16_t)knob_h};
        r.drawRect(knob, true);
        if (knob_h >= 3) {
            r.setPixel(x, (std::int16_t)(ky + knob_h / 2), false);
        }
    }

    template<class R>
    void draw_big_progress(R& r,
                                  const gui::Rect bar,
                                  const std::uint8_t value_0_100,
                                  const bool focused) noexcept
    {
        r.fillRect(bar, focused);

        r.drawRect(bar, !focused);

        const int innerX = bar.x + 2;
        const int innerY = bar.y + 2;
        const int innerW = bar.w - 4;
        const int innerH = bar.h - 4;

        int fillW = 0;
        {
            const int v = value_0_100;
            fillW = (innerW * v) / 100;
            if (fillW < 0) fillW = 0;
            if (fillW > innerW) fillW = innerW;
        }

        if (fillW > 0) {
            r.fillRect(gui::Rect{(int16_t)innerX, (int16_t)innerY, (int16_t)fillW, (int16_t)innerH}, !focused);
        }
    }
    template <class R>
    void draw_popup(R& r, AppState& s) noexcept
    {
        const auto& sem = s.semantics;
        const auto& th = app::theme::current();

        gui::ui::PopupStyle style{};
        style.font = th.font_default;
        style.title_pad_top = (std::int16_t)(th.pad_sm + 2);
        style.bottom_pad = (std::int16_t)(th.pad_sm + 2);
        style.bar_gap = 4;
        style.bar_h = 8;
        style.side_pad = (std::int16_t)th.pad_sm;
        style.inner_pad = 6;
        style.title_value_gap = 12;
        style.min_w = 72;
        style.max_w = (std::int16_t)(R::kWidth - th.pad_sm * 2);

        char size_value_buf[12]{};
        gui::ui::PopupContent size_content{};
        if (s.popup.content == PopupContentKind::Lamp) {
            size_content.title = "Lamp";
            std::snprintf(size_value_buf, sizeof(size_value_buf), "%u", (unsigned)s.data.lamp_brightness);
            size_content.value = size_value_buf;
            size_content.percent = (std::int16_t)s.data.lamp_brightness;
        } else if (s.popup.content == PopupContentKind::Setting) {
            if (s.popup.setting == SettingField::SpringOmega) {
                const int v = (int)(s.ui.anim.spring_omega * 100.0f + 0.5f);
                std::snprintf(size_value_buf, sizeof(size_value_buf), "%d.%02d", v / 100, v % 100);
                size_content.value = size_value_buf;
                size_content.percent = 0;
            } else if (s.popup.setting == SettingField::SpringZeta) {
                const int v = (int)(s.ui.anim.spring_zeta * 100.0f + 0.5f);
                std::snprintf(size_value_buf, sizeof(size_value_buf), "%d.%02d", v / 100, v % 100);
                size_content.value = size_value_buf;
                size_content.percent = 0;
            } else {
                const std::uint16_t value = (s.popup.setting == SettingField::WinSpeed)
                    ? s.ui.anim.win.base_ms
                    : (s.popup.setting == SettingField::SpotSpeed ? s.ui.anim.spot.base_ms : s.ui.anim.list.base_ms);
                std::snprintf(size_value_buf, sizeof(size_value_buf), "%ums", (unsigned)value);
                size_content.value = size_value_buf;
                size_content.percent = 0;
            }
        }

        const auto size_measure = gui::ui::measure_popup(style, size_content);
        int popup_w = size_measure.width;
        int popup_h = size_measure.height;

        const bool want_open = (sem.capture.kind == gui::ui::CaptureKind::Popup)
            && (sem.capture.owner_id == kPopupLampOwner || sem.capture.owner_id == kPopupSettingsOwner);
        if (want_open) {
            s.popup.last_owner_id = sem.capture.owner_id;
            s.popup.last_owner_valid = true;
            s.popup.last_content = s.popup.content;
            s.popup.last_content_valid = true;
            s.popup.popup_w = (std::int16_t)popup_w;
            s.popup.popup_h = (std::int16_t)popup_h;
            s.popup.popup_size_valid = true;
        } else if (!s.popup.popup_size_valid) {
            return;
        }

        const gui::ui::NodeId draw_owner = want_open ? sem.capture.owner_id
            : (s.popup.last_owner_valid ? s.popup.last_owner_id : 0);
        const PopupContentKind draw_kind = want_open ? s.popup.content
            : (s.popup.last_content_valid ? s.popup.last_content : PopupContentKind::None);

        popup_w = s.popup.popup_w;
        popup_h = s.popup.popup_h;

        const std::int16_t onscreen_y = (std::int16_t)((R::kHeight - popup_h) / 2);
        const std::int16_t offscreen_y = (std::int16_t)(-popup_h);
        auto& anim = s.popup.popup_anim_y;
        if (!s.popup.popup_anim_valid) {
            s.popup.popup_anim_valid = true;
            anim.set_target(offscreen_y, s.now_ms, 0, true);
        }
        const auto win = gui::motion::channel_params(
            s.ui.anim,
            gui::motion::AnimChannelId::Win,
            gui::ui::is_on(s.ui.anim_enabled),
            false,
            true);
        gui::motion::apply_anim(anim,
                                (std::int16_t)(want_open ? onscreen_y : offscreen_y),
                                s.now_ms,
                                win.duration,
                                win.snap,
                                win.curve);
        if (!want_open && !anim.running && anim.value == offscreen_y) {
            s.popup.popup_size_valid = false;
            s.popup.last_owner_valid = false;
            s.popup.last_content_valid = false;
            s.popup.popup_anim_valid = false;
            return;
        }

        const gui::Rect box{
            (std::int16_t)((R::kWidth - popup_w) / 2),
            (std::int16_t)anim.value,
            (std::int16_t)popup_w,
            (std::int16_t)popup_h
        };

        gui::ui::PopupContent draw_content{};
        char draw_value_buf[12]{};
        if (draw_owner == kPopupLampOwner && draw_kind == PopupContentKind::Lamp) {
            draw_content.title = "Lamp";
            std::snprintf(draw_value_buf, sizeof(draw_value_buf), "%u", (unsigned)s.data.lamp_brightness);
            draw_content.value = draw_value_buf;
            draw_content.percent = (std::int16_t)s.data.lamp_brightness;
        } else if (draw_owner == kPopupSettingsOwner && draw_kind == PopupContentKind::Setting) {
            if (s.popup.setting == SettingField::SpringOmega) {
                const int v = (int)(s.ui.anim.spring_omega * 100.0f + 0.5f);
                std::snprintf(draw_value_buf, sizeof(draw_value_buf), "%d.%02d", v / 100, v % 100);
                draw_content.value = draw_value_buf;
                draw_content.center_value = true;
                constexpr int kMin = 100;
                constexpr int kMax = 6000;
                int percent = 0;
                if (kMax > kMin) {
                    percent = (v - kMin) * 100 / (kMax - kMin);
                    if (percent < 0) percent = 0;
                    if (percent > 100) percent = 100;
                }
                draw_content.percent = (std::int16_t)percent;
            } else if (s.popup.setting == SettingField::SpringZeta) {
                const int v = (int)(s.ui.anim.spring_zeta * 100.0f + 0.5f);
                std::snprintf(draw_value_buf, sizeof(draw_value_buf), "%d.%02d", v / 100, v % 100);
                draw_content.value = draw_value_buf;
                draw_content.center_value = true;
                constexpr int kMin = 0;
                constexpr int kMax = 200;
                int percent = 0;
                if (kMax > kMin) {
                    percent = (v - kMin) * 100 / (kMax - kMin);
                    if (percent < 0) percent = 0;
                    if (percent > 100) percent = 100;
                }
                draw_content.percent = (std::int16_t)percent;
            } else {
                std::uint16_t value = s.ui.anim.list.base_ms;
                if (s.popup.setting == SettingField::WinSpeed) {
                    value = s.ui.anim.win.base_ms;
                } else if (s.popup.setting == SettingField::SpotSpeed) {
                    value = s.ui.anim.spot.base_ms;
                }
                std::snprintf(draw_value_buf, sizeof(draw_value_buf), "%ums", (unsigned)value);
                draw_content.value = draw_value_buf;
                draw_content.center_value = true;
                constexpr int kMin = 40;
                constexpr int kMax = 400;
                int percent = 0;
                if (kMax > kMin) {
                    const int vi = (int)value;
                    percent = (vi - kMin) * 100 / (kMax - kMin);
                    if (percent < 0) percent = 0;
                    if (percent > 100) percent = 100;
                }
                draw_content.percent = (std::int16_t)percent;
            }
        }

        if (draw_content.value == nullptr && draw_content.title == nullptr) {
            return;
        }

        gui::ui::PopupView view{};
        view.style = style;
        view.content = draw_content;
        view.box = box;
        view.fill_on = false;
        view.border_on = true;
        gui::ui::draw_popup_view(r, view);
    }

    template <class R>
    void draw_main_ui(R& r, AppState& s, gui::ui::UiContext& ctx) noexcept
    {
        using gui::Rect;

        const auto& th = *ctx.theme;
        const std::int16_t pad = (std::int16_t)th.list_pad;
        const Rect drawer{
            pad,
            pad,
            (std::int16_t)(R::kWidth - pad * 2),
            (std::int16_t)(R::kHeight - pad * 2)
        };
        const Rect area{
            drawer.x,
            drawer.y,
            drawer.w,
            drawer.h
        };

        auto& sem = s.semantics;

        auto& list = s.main_page.list;
        list.draw_frame = false;
        std::int16_t main_focus_index = sem.focus.index;
        if (main_focus_index < 0 || main_focus_index >= MainPageState::item_count) main_focus_index = 0;
        list.focus.index = main_focus_index;

        auto spec = gui::ui::default_list_spec(ctx,
                                               gui::ui::fnv1a("main_list"),
                                               MainPageState::item_count,
                                               gui::ui::ListKind::Main,
                                               gui::ui::ListPreset::StandardWithScrollbar,
                                               s.main_page.shell);

        gui::ui::FocusList<MainPageState::item_count> domain{};
        gui::ui::fill_linear_domain(spec.domain_id, spec.item_count, domain);

        auto row_fn = [](std::int16_t i) noexcept -> const gui::ListItem<AppState>* {
            if (i < 0 || i >= MainPageState::item_count) return nullptr;
            return &kMainItems[i];
        };

        auto pressed_fn = [&](std::int16_t focus) noexcept -> std::int16_t {
            return gui::ui::flash_active(ctx.now_ms, s.main_page.last_activate_ms) ? focus : (std::int16_t)-1;
        };

        auto highlight_fn = [&](const alg::list::Layout& layout, std::int16_t focus_index) noexcept -> int {
            if (layout.row_count <= 0) return gui::kNoOverrideY;
            if (focus_index < layout.top_index || focus_index >= layout.top_index + layout.row_count) {
                return gui::kNoOverrideY;
            }

            const int stride = spec.item_h + spec.gap;
            const int focus_row = focus_index - layout.top_index;
            const int row_y = area.y + focus_row * stride + layout.row_offset;
            const int label_h = (ctx.theme && ctx.theme->font_default)
                ? (ctx.theme->font_default->line_height + 2)
                : 0;
            const int target_y = row_y + (spec.item_h - label_h) / 2;

            auto& highlight = s.main_page;
            if (!highlight.highlight_valid) {
                highlight.highlight_valid = true;
                highlight.highlight_last_ms = ctx.now_ms;
                highlight.highlight_spring.reset((float)target_y);
            }

            if (layout.row_count > 0) {
                const int first_y = area.y + layout.row_offset;
                if (first_y != highlight.highlight_prev_first_y) {
                    const int scroll_dy = first_y - highlight.highlight_prev_first_y;
                    highlight.highlight_prev_first_y = (std::int16_t)first_y;
                    highlight.highlight_spring.value += (float)scroll_dy;
                    highlight.highlight_spring.target += (float)scroll_dy;
                }
            }

            const auto& sem = s.semantics;
            const auto p = gui::motion::channel_params(
                s.ui.anim,
                gui::motion::AnimChannelId::Highlight,
                gui::ui::is_on(s.ui.anim_enabled),
                sem.focus.last_jump,
                sem.phase == gui::ui::InteractionPhase::Navigate);
            if (p.snap || p.duration == 0) {
                highlight.highlight_spring.reset((float)target_y);
            } else {
                const float sec = (p.duration > 0) ? ((float)p.duration * 0.001f) : 0.0f;
                float omega = s.ui.anim.spring_omega;
                float zeta = s.ui.anim.spring_zeta;
                if (s.ui.anim.spring_override || s.ui.anim.spring_preset == gui::motion::SpringPreset::Custom) {
                    // use custom params
                } else if (s.ui.anim.spring_preset == gui::motion::SpringPreset::Default) {
                    omega = (sec > 0.0f) ? (6.0f / sec) : 60.0f;
                    zeta = 0.8f;
                } else {
                    gui::motion::spring_preset_params(s.ui.anim.spring_preset, omega, zeta);
                }
                highlight.highlight_spring.set_params(omega, zeta);
                highlight.highlight_spring.target = (float)target_y;
                const std::uint32_t dt_ms = ctx.now_ms - highlight.highlight_last_ms;
                highlight.highlight_last_ms = ctx.now_ms;
                highlight.highlight_spring.step_ms(dt_ms);
            }

            return (std::int16_t)highlight.highlight_spring.value;
        };

        const int target_h = area.h;
        auto& expand = s.main_page.expand_anim;
        if (!s.main_page.expand_valid) {
            s.main_page.expand_valid = true;
            expand.set_target(0, ctx.now_ms, 0, true);
        }
        const auto expand_p = gui::motion::channel_params(
            s.ui.anim,
            gui::motion::AnimChannelId::Win,
            gui::ui::is_on(s.ui.anim_enabled),
            false,
            true);
        gui::motion::apply_anim(expand,
                                (std::int16_t)target_h,
                                ctx.now_ms,
                                expand_p.duration,
                                expand_p.snap,
                                expand_p.curve);

        int scale_q8 = 256;
        if (target_h > 0) {
            const int v = expand.value;
            if (v <= 0) {
                scale_q8 = 0;
            } else {
                scale_q8 = (v * 256) / target_h;
                if (scale_q8 > 256) scale_q8 = 256;
                if (scale_q8 < 0) scale_q8 = 0;
            }
        }
        list.draw_scale_q8 = (std::int16_t)scale_q8;
        list.draw_origin_y = area.y;
        r.push_clip(area);
        const auto frame = gui::ui::draw_list_page(r, s, ctx, area, spec, domain, list,
                                                   row_fn, pressed_fn, highlight_fn);
        if (frame.layout.row_count > 0 && app::kMainIconItemIndex >= 0) {
            const std::int16_t icon_idx = app::kMainIconItemIndex;
            const int top = frame.layout.top_index;
            const int rows = frame.layout.row_count;
            if (icon_idx >= top && icon_idx < top + rows) {
                const int row = icon_idx - top;
                const int stride = spec.item_h + spec.gap;
                const int base_y = area.y + row * stride + frame.layout.row_offset;
                int draw_y = base_y;
                int draw_h = spec.item_h;
                if (list.draw_scale_q8 != 256) {
                    draw_y = list.draw_origin_y + ((base_y - list.draw_origin_y) * list.draw_scale_q8) / 256;
                    draw_h = (spec.item_h * list.draw_scale_q8) / 256;
                }
                if (draw_h > 0) {
                    const gui::Rect row_rc{
                        area.x,
                        (std::int16_t)draw_y,
                        area.w,
                        (std::int16_t)draw_h
                    };
                    const auto& icon = app::icons::icon_at(0);
                    if (icon.image) {
                        const int icon_x = row_rc.x + row_rc.w - icon.image->width - th.pad_xs;
                        const int icon_y = row_rc.y + (row_rc.h - icon.image->height) / 2;
                        r.push_clip(row_rc);
                        gui::draw_image_1bpp(r,
                                             (std::int16_t)icon_x,
                                             (std::int16_t)icon_y,
                                             *icon.image,
                                             true);
                        r.pop_clip();
                    }
                }
            }
        }
        r.pop_clip();

    }

    template <class R>
    void draw_chart_ui(R& r, AppState& s) noexcept
    {
        const auto& th = app::theme::current();
        const std::int16_t pad = (std::int16_t)th.pad_sm;
        const gui::Rect area{
            pad,
            pad,
            (std::int16_t)(R::kWidth - pad * 2),
            (std::int16_t)(R::kHeight - pad * 2)
        };
        s.chart_page.shell.draw_chrome(r, area, s.ui.list_chrome, *th.font_default, "Sine", false);

        const int phase = (int)((s.now_ms / 40) % kScopeSamples);
        gui::ScopeView view = gui::make_scope_view_u8(kSineTable.data(), kScopeSamples);
        gui::scope_widget(r, area, view, phase);
    }

    template <class R>
    void draw_chart_wave_ui(R& r, AppState& s) noexcept
    {
        const auto& th = app::theme::current();
        const std::int16_t pad = (std::int16_t)th.pad_sm;
        const gui::Rect area{
            pad,
            pad,
            (std::int16_t)(R::kWidth - pad * 2),
            (std::int16_t)(R::kHeight - pad * 2)
        };
        s.chart_page.shell.draw_chrome(r, area, s.ui.list_chrome, *th.font_default, "Wave", false);

        auto& scope = s.chart_page;
        const std::uint32_t step_ms = 30;
        if (!scope.scope_valid) {
            for (int i = 0; i < scope.scope_count; ++i) {
                const int idx = i % kScopeSamples;
                scope.scope[i] = (std::uint16_t)(kSineTable[idx] * 10);
            }
            scope.scope_valid = true;
            scope.last_sample_ms = s.now_ms;
        }

        while ((s.now_ms - scope.last_sample_ms) >= step_ms) {
            scope.last_sample_ms += step_ms;
            for (int i = 0; i < scope.scope_count - 1; ++i) {
                scope.scope[i] = scope.scope[i + 1];
            }
            const int phase = (int)((scope.last_sample_ms / step_ms) % kScopeSamples);
            scope.scope[scope.scope_count - 1] = (std::uint16_t)(kSineTable[phase] * 10);
        }

        gui::ScopeView view = gui::make_scope_view_u16(scope.scope, (std::uint16_t)scope.scope_count, (std::uint16_t)area.h);
        gui::scope_widget(r, area, view, 0);
    }

    template <class R>
    void draw_icon_ui(R& r, AppState& s) noexcept
    {
        const auto& th = app::theme::current();
        const std::int16_t count = app::icons::icon_count();
        if (count <= 0) return;

        static bool metrics_valid = false;
        static int max_w = 0;
        static int max_h = 0;
        if (!metrics_valid) {
            for (std::int16_t i = 0; i < count; ++i) {
                const auto& icon = app::icons::icon_at(i);
                if (!icon.image) continue;
                if (icon.image->width > max_w) max_w = icon.image->width;
                if (icon.image->height > max_h) max_h = icon.image->height;
            }
            metrics_valid = true;
        }
        if (max_w <= 0 || max_h <= 0) return;

        const std::uint32_t now = s.now_ms;
        const std::uint32_t period_ms = 1200;
        if (s.icon_page.last_ms == 0) {
            s.icon_page.last_ms = now;
        } else if ((now - s.icon_page.last_ms) >= period_ms) {
            s.icon_page.last_ms = now;
            s.icon_page.index = (std::int16_t)((s.icon_page.index + 1) % count);
        }

        const int max_x = (R::kWidth - max_w) / 2;
        const int max_y = (R::kHeight - max_h) / 2 - 6;
        const gui::Rect icon_area{
            (std::int16_t)max_x,
            (std::int16_t)max_y,
            (std::int16_t)max_w,
            (std::int16_t)max_h
        };
        const auto& icon = app::icons::icon_at(s.icon_page.index);
        if (!icon.image) return;

        const int icon_w = icon.image->width;
        const int icon_h = icon.image->height;
        const int x = (R::kWidth - icon_w) / 2;
        const int y = (R::kHeight - icon_h) / 2 - 6;

        const gui::Rect footer{
            0,
            (std::int16_t)(R::kHeight - th.footer_h),
            (std::int16_t)R::kWidth,
            (std::int16_t)th.footer_h
        };
        gui::ui::VTree<12> tree{};
        tree.add_image(1, icon_area, *icon.image, (std::int16_t)x, (std::int16_t)y, true);

        const char* label = icon.name ? icon.name : "";
        const int label_w = gui::layout::text_width(*th.font_default, label);
        const int label_x = (R::kWidth - label_w) / 2;
        const int base = gui::layout::baseline_from_top(*th.font_default, (int)(R::kHeight - th.footer_h + 1));
        const gui::Rect label_rc{
            (std::int16_t)label_x,
            (std::int16_t)(base - th.font_default->baseline),
            (std::int16_t)label_w,
            (std::int16_t)th.font_default->line_height
        };
        tree.add_text(2, label_rc, *th.font_default, (std::int16_t)label_x, (std::int16_t)base, label, true);

        auto chrome_style = s.ui.list_chrome;
        chrome_style.show_title = false;
        s.icon_page.shell.draw_chrome(r, icon_area, chrome_style, *th.font_default, label, false);
        gui::ui::draw_tree(r, tree);
    }

    template <class R>
    void draw_qr_ui(R& r, AppState& s) noexcept
    {
        auto& qr = s.qr_page;
        if (!qr.code.valid()) {
            char text[] = "https://www.baidu.com/";
            (void)qr.code.encode(text);
        }

        const gui::Rect rc{0, 0, (std::int16_t)R::kWidth, (std::int16_t)R::kHeight};
        qr.code.draw(r, rc, true);
    }

    template <class R>
    void draw_settings_ui(R& r, AppState& s, gui::ui::UiContext& ctx) noexcept
    {
        using gui::Rect;

        const auto& th = *ctx.theme;
        const std::int16_t pad = (std::int16_t)th.list_pad;
        const Rect drawer{
            pad,
            pad,
            (std::int16_t)(R::kWidth - pad * 2),
            (std::int16_t)(R::kHeight - pad * 2)
        };
        const Rect area{
            drawer.x,
            drawer.y,
            drawer.w,
            drawer.h
        };

        auto& list = s.settings_page.list;
        list.draw_frame = false;

        auto spec = gui::ui::default_list_spec(ctx,
                                               gui::ui::fnv1a("settings_list"),
                                               SettingsPageState::item_count,
                                               gui::ui::ListKind::Settings,
                                               gui::ui::ListPreset::StandardWithScrollbar,
                                               s.settings_page.shell);

        gui::ui::FocusList<SettingsPageState::item_count> domain{};
        gui::ui::fill_linear_domain(spec.domain_id, spec.item_count, domain);

        auto row_fn = [](std::int16_t i) noexcept -> const gui::ListItem<AppState>* {
            if (i < 0 || i >= SettingsPageState::item_count) return nullptr;
            return &kSettingsItems[i];
        };

        auto pressed_fn = [&](std::int16_t focus) noexcept -> std::int16_t {
            return gui::ui::flash_active(ctx.now_ms, s.settings_page.last_activate_ms) ? focus : (std::int16_t)-1;
        };

        auto highlight_fn = [&](const alg::list::Layout& layout, std::int16_t focus_index) noexcept -> int {
            if (layout.row_count <= 0) return gui::kNoOverrideY;
            if (focus_index < layout.top_index || focus_index >= layout.top_index + layout.row_count) {
                return gui::kNoOverrideY;
            }

            const int stride = spec.item_h + spec.gap;
            const int focus_row = focus_index - layout.top_index;
            const int row_y = area.y + focus_row * stride + layout.row_offset;
            const int label_h = (ctx.theme && ctx.theme->font_default)
                ? (ctx.theme->font_default->line_height + 2)
                : 0;
            const int target_y = row_y + (spec.item_h - label_h) / 2;

            auto& highlight = s.settings_page;
            if (!highlight.highlight_valid) {
                highlight.highlight_valid = true;
                highlight.highlight_last_ms = ctx.now_ms;
                highlight.highlight_spring.reset((float)target_y);
            }

            if (layout.row_count > 0) {
                const int first_y = area.y + layout.row_offset;
                if (first_y != highlight.highlight_prev_first_y) {
                    const int scroll_dy = first_y - highlight.highlight_prev_first_y;
                    highlight.highlight_prev_first_y = (std::int16_t)first_y;
                    highlight.highlight_spring.value += (float)scroll_dy;
                    highlight.highlight_spring.target += (float)scroll_dy;
                }
            }

            const auto& sem = s.semantics;
            const auto p = gui::motion::channel_params(
                s.ui.anim,
                gui::motion::AnimChannelId::Highlight,
                gui::ui::is_on(s.ui.anim_enabled),
                sem.focus.last_jump,
                sem.phase == gui::ui::InteractionPhase::Navigate);
            if (p.snap || p.duration == 0) {
                highlight.highlight_spring.reset((float)target_y);
            } else {
                const float sec = (p.duration > 0) ? ((float)p.duration * 0.001f) : 0.0f;
                float omega = s.ui.anim.spring_omega;
                float zeta = s.ui.anim.spring_zeta;
                if (s.ui.anim.spring_override || s.ui.anim.spring_preset == gui::motion::SpringPreset::Custom) {
                    // use custom params
                } else if (s.ui.anim.spring_preset == gui::motion::SpringPreset::Default) {
                    omega = (sec > 0.0f) ? (6.0f / sec) : 60.0f;
                    zeta = 0.8f;
                } else {
                    gui::motion::spring_preset_params(s.ui.anim.spring_preset, omega, zeta);
                }
                highlight.highlight_spring.set_params(omega, zeta);
                highlight.highlight_spring.target = (float)target_y;
                const std::uint32_t dt_ms = ctx.now_ms - highlight.highlight_last_ms;
                highlight.highlight_last_ms = ctx.now_ms;
                highlight.highlight_spring.step_ms(dt_ms);
            }

            return (std::int16_t)highlight.highlight_spring.value;
        };

        const int target_h = area.h;
        auto& expand = s.settings_page.expand_anim;
        if (!s.settings_page.expand_valid) {
            s.settings_page.expand_valid = true;
            expand.set_target(0, ctx.now_ms, 0, true);
        }
        const auto expand_p = gui::motion::channel_params(
            s.ui.anim,
            gui::motion::AnimChannelId::Win,
            gui::ui::is_on(s.ui.anim_enabled),
            false,
            true);
        gui::motion::apply_anim(expand,
                                (std::int16_t)target_h,
                                ctx.now_ms,
                                expand_p.duration,
                                expand_p.snap,
                                expand_p.curve);

        int scale_q8 = 256;
        if (target_h > 0) {
            const int v = expand.value;
            if (v <= 0) {
                scale_q8 = 0;
            } else {
                scale_q8 = (v * 256) / target_h;
                if (scale_q8 > 256) scale_q8 = 256;
                if (scale_q8 < 0) scale_q8 = 0;
            }
        }
        list.draw_scale_q8 = (std::int16_t)scale_q8;
        list.draw_origin_y = area.y;
        r.push_clip(area);
        const auto frame = gui::ui::draw_list_page(r, s, ctx, area, spec, domain, list,
                                                   row_fn, pressed_fn, highlight_fn);
        (void)frame;
        r.pop_clip();

    }

    template <class R>
    void draw_widgets_ui(R& r, AppState& s, gui::ui::UiContext& ctx) noexcept
    {
        using gui::Rect;
        const auto& th = *ctx.theme;
        const std::int16_t pad = (std::int16_t)th.list_pad;
        const Rect drawer{
            pad,
            pad,
            (std::int16_t)(R::kWidth - pad * 2),
            (std::int16_t)(R::kHeight - pad * 2)
        };
        const Rect area{
            drawer.x,
            drawer.y,
            drawer.w,
            drawer.h
        };

        auto& list = s.widgets_page.list;
        list.draw_frame = false;

        auto spec = gui::ui::default_list_spec(ctx,
                                               gui::ui::fnv1a("widgets_list"),
                                               WidgetsPageState::item_count,
                                               gui::ui::ListKind::Settings,
                                               gui::ui::ListPreset::StandardWithScrollbar,
                                               s.widgets_page.shell,
                                               "Widgets");

        gui::ui::FocusList<WidgetsPageState::item_count> domain{};
        gui::ui::fill_linear_domain(spec.domain_id, spec.item_count, domain);

        auto row_fn = [](std::int16_t i) noexcept -> const gui::ListItem<AppState>* {
            if (i < 0 || i >= WidgetsPageState::item_count) return nullptr;
            return &kWidgetsItems[i];
        };

        auto pressed_fn = [&](std::int16_t focus) noexcept -> std::int16_t {
            return gui::ui::flash_active(ctx.now_ms, s.widgets_page.last_activate_ms) ? focus : (std::int16_t)-1;
        };

        auto highlight_fn = [&](const alg::list::Layout&, std::int16_t) noexcept -> int {
            return gui::kNoOverrideY;
        };

        const bool flash = gui::ui::flash_active(ctx.now_ms, s.widgets_page.last_activate_ms);
        list.focus_style = flash ? gui::FocusStyle::Underline : gui::FocusStyle::ReverseRoundRect;

        r.push_clip(area);
        const auto frame = gui::ui::draw_list_page(r, s, ctx, area, spec, domain, list,
                                                   row_fn, pressed_fn, highlight_fn);
        r.pop_clip();

        if (flash && frame.layout.row_count > 0) {
            const std::int16_t focus = frame.focus_index;
            if (focus >= frame.layout.top_index && focus < frame.layout.top_index + frame.layout.row_count) {
                const int row = focus - frame.layout.top_index;
                const int stride = spec.item_h + spec.gap;
                const int row_y = area.y + row * stride + frame.layout.row_offset;
                const gui::Rect uline{
                    area.x,
                    (std::int16_t)(row_y + spec.item_h - 1),
                    area.w,
                    1
                };
                r.fillRect(uline, true);
            }
        }

        if (s.widgets_page.toast_text && s.widgets_page.toast_until_ms > s.now_ms) {
            const gui::Rect full{0, 0, (std::int16_t)R::kWidth, (std::int16_t)R::kHeight};
            gui::draw_toast(r, full, s.widgets_page.toast_text);
        }
    }

    template<class R>
    void draw_battery_ui(R& r, AppState& s) noexcept
    {
        const int W = R::kWidth;
        const auto& th = app::theme::current();
        auto& page = s.battery_page;
        if (!page.vtree_valid) {
            r.clear(false);
        }

        // Title bar area.
        const gui::Rect title{0, 0, W, (std::int16_t)th.header_h};
        const int title_base = gui::layout::baseline_from_top(*th.font_default, th.pad_xs);

        // Centered percentage text.
        char buf[8]{};
        std::snprintf(buf, sizeof(buf), "%u%%", (unsigned)s.data.battery);

        // Use a fixed width so shrinking text does not leave stale pixels.
        const int max_text_w = gui::measure_text(*th.font_default, "100%");
        const int textX = gui::layout::align_center_x(gui::Rect{0, 0, W, 0}, max_text_w);
        const int percent_base = gui::layout::baseline_from_top(*th.font_default, 18);

        const gui::Rect bar{12, 34, W - 24, 14};
        const int innerX = bar.x + 2;
        const int innerY = bar.y + 2;
        const int innerW = bar.w - 4;
        const int innerH = bar.h - 4;
        int fillW = (innerW * (int)s.data.battery) / 100;
        if (fillW < 0) fillW = 0;
        if (fillW > innerW) fillW = innerW;

        // Footer hint.
        char foot[24]{};
        std::snprintf(foot, sizeof(foot), "Step:%u  Back:Esc", (unsigned)s.battery_page.step);
        const int foot_base = gui::layout::baseline_from_top(*th.font_default, 52);

        gui::ui::VTree<12> tree{};
        tree.add_fill_rect(1, title, false);
        {
            const int w = gui::measure_text(*th.font_default, "Battery");
            const gui::Rect rc{
                (std::int16_t)th.pad_xs,
                (std::int16_t)(title_base - th.font_default->baseline),
                (std::int16_t)w,
                (std::int16_t)th.font_default->line_height
            };
            tree.add_text(2, rc, *th.font_default, (std::int16_t)th.pad_xs, (std::int16_t)title_base, "Battery", true);
        }
        {
            const gui::Rect rc{
                0,
                (std::int16_t)(percent_base - th.font_default->baseline),
                (std::int16_t)W,
                (std::int16_t)th.font_default->line_height
            };
            tree.add_text(3, rc, *th.font_default, (std::int16_t)textX, (std::int16_t)percent_base, buf, true);
        }
        tree.add_fill_rect(4, bar, false);
        tree.add_stroke_rect(5, bar, true);
        if (fillW > 0) {
            const gui::Rect fill{
                (std::int16_t)innerX,
                (std::int16_t)innerY,
                (std::int16_t)fillW,
                (std::int16_t)innerH
            };
            tree.add_fill_rect(6, fill, true);
        }
        {
            const int w = gui::measure_text(*th.font_default, foot);
            const gui::Rect rc{
                (std::int16_t)th.pad_xs,
                (std::int16_t)(foot_base - th.font_default->baseline),
                (std::int16_t)w,
                (std::int16_t)th.font_default->line_height
            };
            tree.add_text(7, rc, *th.font_default, (std::int16_t)th.pad_xs, (std::int16_t)foot_base, foot, true);
        }

        if (page.vtree_valid) {
            const auto dirty = gui::ui::diff_tree(page.vtree, tree);
            if (dirty.full) {
                r.clear(false);
            } else {
                for (int i = 0; i < dirty.count; ++i) {
                    r.fillRect(dirty.rects[i], false);
                }
            }
        }
        gui::ui::draw_tree(r, tree);
        page.vtree = tree;
        page.vtree_valid = true;
    }

} // namespace app::detail


export namespace app
{
    template <class R>
    void draw_current_ui(R& r, AppState& s) noexcept;
}


export template <class R>
void app::draw_current_ui(R& r, AppState& s) noexcept
{
    app::theme::set_current(gui::ui::is_on(s.ui.theme_compact));
    r.set_invert(gui::ui::is_on(s.ui.theme_invert));
    const gui::Rect full_clip{0, 0, R::kWidth, R::kHeight};
    const auto page = s.pages.current();
    if (s.last_page != page) {
        r.clear(false);
        s.last_page = page;
    }
    switch (page) {
    case PageId::Main:
        {
            auto ctx = detail::make_ui_context(s, full_clip, &s.main_page.viewport);
            detail::draw_main_ui(r, s, ctx);
        }
        break;
    case PageId::Battery:
        detail::draw_battery_ui(r, s);
        break;
    case PageId::Chart:
        detail::draw_chart_ui(r, s);
        break;
    case PageId::ChartWave:
        detail::draw_chart_wave_ui(r, s);
        break;
    case PageId::Icon:
        detail::draw_icon_ui(r, s);
        break;
    case PageId::Qr:
        detail::draw_qr_ui(r, s);
        break;
    case PageId::Settings:
        {
            auto ctx = detail::make_ui_context(s, full_clip, &s.settings_page.viewport);
            detail::draw_settings_ui(r, s, ctx);
        }
        break;
    case PageId::Widgets:
        {
            auto ctx = detail::make_ui_context(s, full_clip, &s.widgets_page.viewport);
            detail::draw_widgets_ui(r, s, ctx);
        }
        break;
    default:
        {
            auto ctx = detail::make_ui_context(s, full_clip, &s.main_page.viewport);
            detail::draw_main_ui(r, s, ctx);
        }
        break;
    }

    detail::draw_popup(r, s);
    detail::draw_fps_overlay(r, s);
}
