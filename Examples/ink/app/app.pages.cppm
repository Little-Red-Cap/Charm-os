// app.pages.cppm
// Central registry of page items (label/kind/getter/action) shared by UI and logic.

module;
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

export module app.pages;

import app.state;
import gui.list_view;
import gui.ui_tree;
import gui.widgets;
import gui.ui_semantics;
import gui.motion;
import gui.ui_focus_bookmark;
import gui.ui_settings;
import gui.ui_input_policy;

namespace app::detail
{
            // Helpers: save/restore list focus per page (Main / Settings for now)
    export inline void remember_focus(AppState& s, PageId page) noexcept {
        if (page == PageId::Main) {
            gui::ui::save_focus(s.semantics, s.main_page.focus_bk);
        } else if (page == PageId::Settings) {
            gui::ui::save_focus(s.semantics, s.settings_page.focus_bk);
        } else if (page == PageId::Widgets) {
            gui::ui::save_focus(s.semantics, s.widgets_page.focus_bk);
        }
    }

    export inline void restore_focus(AppState& s, PageId page) noexcept {
        if (page == PageId::Main) {
            const auto domain_id = gui::ui::fnv1a("main_list");
            gui::ui::restore_focus(s.semantics, s.main_page.focus_bk, domain_id, MainPageState::item_count, 0);
            s.semantics.capture.kind = gui::ui::CaptureKind::None;
            s.semantics.capture.owner_id = gui::ui::kNullId;
        } else if (page == PageId::Settings) {
            const auto domain_id = gui::ui::fnv1a("settings_list");
            gui::ui::restore_focus(s.semantics, s.settings_page.focus_bk, domain_id, SettingsPageState::item_count, 0);
            s.semantics.capture.kind = gui::ui::CaptureKind::None;
            s.semantics.capture.owner_id = gui::ui::kNullId;
        } else if (page == PageId::Widgets) {
            const auto domain_id = gui::ui::fnv1a("widgets_list");
            gui::ui::restore_focus(s.semantics, s.widgets_page.focus_bk, domain_id, WidgetsPageState::item_count, 0);
            s.semantics.capture.kind = gui::ui::CaptureKind::None;
            s.semantics.capture.owner_id = gui::ui::kNullId;
        }
    }
// ---- getters ----
    inline bool get_lamp_on(const AppState& s) noexcept { return s.data.lamp_on; }

    inline std::uint8_t get_battery(const AppState& s) noexcept { return s.data.battery; }
    inline bool get_check_a(const AppState& s) noexcept { return s.data.check_a; }
    inline bool get_check_b(const AppState& s) noexcept { return s.data.check_b; }
    inline bool get_switch_a(const AppState& s) noexcept { return s.data.switch_a; }
    inline std::uint8_t get_progress_demo(const AppState& s) noexcept { return s.data.progress_demo; }
    inline gui::ChartView get_chart_demo(const AppState& s) noexcept { return gui::ChartView{s.data.chart, 8}; }
    inline bool get_theme_invert(const AppState& s) noexcept { return gui::ui::is_on(s.ui.theme_invert); }
    inline bool get_anim_enabled(const AppState& s) noexcept { return gui::ui::is_on(s.ui.anim_enabled); }
    inline std::uint16_t get_anim_list_ms(const AppState& s) noexcept { return s.ui.anim.list.base_ms; }
    inline std::uint16_t get_anim_win_ms(const AppState& s) noexcept { return s.ui.anim.win.base_ms; }
    inline std::uint16_t get_anim_spot_ms(const AppState& s) noexcept { return s.ui.anim.spot.base_ms; }
    inline bool get_fps_overlay(const AppState& s) noexcept { return gui::ui::is_on(s.ui.fps_overlay); }

    inline bool get_anim_curve(const AppState& s) noexcept { return s.ui.anim.list.curve == gui::motion::EaseKind::Smoothstep; }
    inline bool get_spring_override(const AppState& s) noexcept { return s.ui.anim.spring_override; }
    inline std::uint16_t get_spring_omega_u16(const AppState& s) noexcept
    {
        const float v = s.ui.anim.spring_omega;
        int iv = (int)(v * 100.0f + 0.5f);
        if (iv < 0) iv = 0;
        if (iv > 6000) iv = 6000;
        return (std::uint16_t)iv;
    }
    inline std::uint16_t get_spring_zeta_u16(const AppState& s) noexcept
    {
        const float v = s.ui.anim.spring_zeta;
        int iv = (int)(v * 100.0f + 0.5f);
        if (iv < 0) iv = 0;
        if (iv > 200) iv = 200;
        return (std::uint16_t)iv;
    }
    inline const char* get_spring_omega_label(const AppState& s) noexcept
    {
        static char buf[12]{};
        const int v = (int)get_spring_omega_u16(s);
        std::snprintf(buf, sizeof(buf), "%d.%02d", v / 100, v % 100);
        return buf;
    }
    inline const char* get_spring_zeta_label(const AppState& s) noexcept
    {
        static char buf[12]{};
        const int v = (int)get_spring_zeta_u16(s);
        std::snprintf(buf, sizeof(buf), "%d.%02d", v / 100, v % 100);
        return buf;
    }
    inline const char* get_input_policy_label(const AppState& s) noexcept
    {
        switch (s.input_policy_id) {
        case gui::ui::InputPolicyId::Encoder:
            return "Encoder";
        case gui::ui::InputPolicyId::Touch:
            return "Touch";
        case gui::ui::InputPolicyId::Remote:
            return "Remote";
        case gui::ui::InputPolicyId::Custom:
            return "Custom";
        case gui::ui::InputPolicyId::Default:
        default:
            return "Default";
        }
    }
    inline std::uint8_t get_widget_range(const AppState& s) noexcept { return s.widgets_page.range_value; }
    inline std::uint16_t get_widget_stepper(const AppState& s) noexcept { return s.widgets_page.stepper_value; }
    inline std::uint8_t get_widget_segment(const AppState& s) noexcept { return s.widgets_page.segmented_index; }
    inline const char* get_widget_status(const AppState& s) noexcept
    {
        static char buf[16]{};
        std::snprintf(buf, sizeof(buf), "S:%u R:%u", (unsigned)s.widgets_page.stepper_value,
                      (unsigned)s.widgets_page.range_value);
        return buf;
    }
    // ---- actions ----
    inline void action_open_battery(AppState& s) noexcept
    {
        remember_focus(s, s.pages.current());
        s.battery_page.step = 1;
        (void)s.pages.push(PageId::Battery);
    }

    inline void action_open_chart(AppState& s) noexcept
    {
        remember_focus(s, s.pages.current());
        (void)s.pages.push(PageId::Chart);
    }

    inline void action_open_chart_wave(AppState& s) noexcept
    {
        remember_focus(s, s.pages.current());
        (void)s.pages.push(PageId::ChartWave);
    }

    inline void action_open_settings(AppState& s) noexcept
    {
        remember_focus(s, s.pages.current());
        restore_focus(s, PageId::Settings);
        s.settings_page.expand_valid = false;
        (void)s.pages.push(PageId::Settings);
    }

    inline void action_open_icon(AppState& s) noexcept
    {
        remember_focus(s, s.pages.current());
        s.icon_page.last_ms = 0;
        (void)s.pages.push(PageId::Icon);
    }

    inline void action_open_qr(AppState& s) noexcept
    {
        remember_focus(s, s.pages.current());
        s.qr_page.code = {};
        (void)s.pages.push(PageId::Qr);
    }
    inline void action_open_widgets(AppState& s) noexcept
    {
        remember_focus(s, s.pages.current());
        restore_focus(s, PageId::Widgets);
        (void)s.pages.push(PageId::Widgets);
    }

    inline void action_noop(AppState&) noexcept {}

    inline void action_toggle_lamp(AppState& s) noexcept { s.data.lamp_on = !s.data.lamp_on; }
    inline void action_toggle_check_a(AppState& s) noexcept { s.data.check_a = !s.data.check_a; }
    inline void action_toggle_check_b(AppState& s) noexcept { s.data.check_b = !s.data.check_b; }
    inline void action_toggle_switch_a(AppState& s) noexcept { s.data.switch_a = !s.data.switch_a; }
    inline void action_toggle_theme(AppState& s) noexcept { s.ui.theme_invert = gui::ui::toggled(s.ui.theme_invert); }
    inline void action_toggle_anim(AppState& s) noexcept { s.ui.anim_enabled = gui::ui::toggled(s.ui.anim_enabled); }
    inline void action_toggle_fps_overlay(AppState& s) noexcept { s.ui.fps_overlay = gui::ui::toggled(s.ui.fps_overlay); }
    inline void action_toggle_spring_override(AppState& s) noexcept
    {
        s.ui.anim.spring_override = !s.ui.anim.spring_override;
        if (s.ui.anim.spring_override) {
            s.ui.anim.spring_preset = gui::motion::SpringPreset::Custom;
        }
    }
    inline void action_toggle_curve(AppState& s) noexcept
    {
        const auto next = (s.ui.anim.list.curve == gui::motion::EaseKind::Smoothstep)
            ? gui::motion::EaseKind::Linear
            : gui::motion::EaseKind::Smoothstep;
        s.ui.anim.list.curve = next;
        s.ui.anim.win.curve = next;
        s.ui.anim.spot.curve = next;
        s.ui.anim.highlight.curve = next;
        s.ui.anim_preset = gui::motion::AnimPreset::Custom;
    }
    inline void action_cycle_input_policy(AppState& s) noexcept
    {
        using gui::ui::InputPolicyId;
        constexpr std::uint8_t kMax = static_cast<std::uint8_t>(InputPolicyId::Count);
        std::uint8_t idx = static_cast<std::uint8_t>(s.input_policy_id);
        for (std::uint8_t i = 0; i < kMax; ++i) {
            idx = (std::uint8_t)((idx + 1) % kMax);
            const auto id = static_cast<InputPolicyId>(idx);
            if (s.input_policies.used[idx]) {
                s.input_policy_id = id;
                s.input_policy = s.input_policies.get(id);
                return;
            }
        }
    }
    inline void action_show_widgets_toast(AppState& s) noexcept
    {
        s.widgets_page.toast_text = "Hello Widgets";
        s.widgets_page.toast_until_ms = s.now_ms + 1200;
    }

    inline void set_anim_preset(AppState& s, gui::motion::AnimPreset preset) noexcept
    {
        s.ui.anim_preset = preset;
        if (preset != gui::motion::AnimPreset::Custom) {
            gui::motion::apply_preset(s.ui.anim, preset);
        }
    }

    inline void set_spring_preset(AppState& s, gui::motion::SpringPreset preset) noexcept
    {
        s.ui.anim.spring_preset = preset;
        s.ui.anim.spring_override = (preset == gui::motion::SpringPreset::Custom);
    }

    inline void action_set_anim_preset_default(AppState& s) noexcept { set_anim_preset(s, gui::motion::AnimPreset::Default); }
    inline void action_set_anim_preset_snappy(AppState& s) noexcept  { set_anim_preset(s, gui::motion::AnimPreset::Snappy); }
    inline void action_set_anim_preset_soft(AppState& s) noexcept    { set_anim_preset(s, gui::motion::AnimPreset::Soft); }
    inline void action_set_anim_preset_custom(AppState& s) noexcept  { set_anim_preset(s, gui::motion::AnimPreset::Custom); }

    inline void action_set_spring_preset_default(AppState& s) noexcept      { set_spring_preset(s, gui::motion::SpringPreset::Default); }
    inline void action_set_spring_preset_critical(AppState& s) noexcept     { set_spring_preset(s, gui::motion::SpringPreset::Critical); }
    inline void action_set_spring_preset_critical_fast(AppState& s) noexcept{ set_spring_preset(s, gui::motion::SpringPreset::CriticalFast); }
    inline void action_set_spring_preset_over(AppState& s) noexcept         { set_spring_preset(s, gui::motion::SpringPreset::Over); }
    inline void action_set_spring_preset_under(AppState& s) noexcept        { set_spring_preset(s, gui::motion::SpringPreset::Under); }
    inline void action_set_spring_preset_custom(AppState& s) noexcept       { set_spring_preset(s, gui::motion::SpringPreset::Custom); }

    using ActionFn = void (*)(AppState&) noexcept;

    inline ActionFn anim_preset_action(gui::motion::AnimPreset preset) noexcept
    {
        using gui::motion::AnimPreset;
        switch (preset) {
        case AnimPreset::Snappy:  return &action_set_anim_preset_snappy;
        case AnimPreset::Soft:    return &action_set_anim_preset_soft;
        case AnimPreset::Custom:  return &action_set_anim_preset_custom;
        case AnimPreset::Default:
        default:                  return &action_set_anim_preset_default;
        }
    }

    inline ActionFn spring_preset_action(gui::motion::SpringPreset preset) noexcept
    {
        using gui::motion::SpringPreset;
        switch (preset) {
        case SpringPreset::Critical:     return &action_set_spring_preset_critical;
        case SpringPreset::CriticalFast: return &action_set_spring_preset_critical_fast;
        case SpringPreset::Over:         return &action_set_spring_preset_over;
        case SpringPreset::Under:        return &action_set_spring_preset_under;
        case SpringPreset::Custom:       return &action_set_spring_preset_custom;
        case SpringPreset::Default:
        default:                         return &action_set_spring_preset_default;
        }
    }

    // Forward declarations for settings actions used in item tables.
    inline void action_edit_spring_omega(AppState& s) noexcept;
    inline void action_edit_spring_zeta(AppState& s) noexcept;
    inline void action_edit_list_speed(AppState& s) noexcept;
    inline void action_edit_win_speed(AppState& s) noexcept;
    inline void action_edit_spot_speed(AppState& s) noexcept;
    inline void action_back(AppState& s) noexcept;

    inline std::array<gui::ListItem<AppState>, SettingsPageState::item_count> build_settings_items() noexcept
    {
        std::array<gui::ListItem<AppState>, SettingsPageState::item_count> items{};
        std::size_t i = 0;

        items[i++] = gui::ListItem<AppState>{
            .label = "Back",
            .kind = gui::ItemKind::Action,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = nullptr,
            .on_activate = &action_back,
        };
        items[i++] = gui::ListItem<AppState>{
            .label = "Theme B/W",
            .kind = gui::ItemKind::Switch,
            .get_bool = &get_theme_invert,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = nullptr,
            .on_activate = &action_toggle_theme,
        };
        items[i++] = gui::ListItem<AppState>{
            .label = "Animation",
            .kind = gui::ItemKind::Switch,
            .get_bool = &get_anim_enabled,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = nullptr,
            .on_activate = &action_toggle_anim,
        };
        items[i++] = gui::ListItem<AppState>{
            .label = "FPS Overlay",
            .kind = gui::ItemKind::Switch,
            .get_bool = &get_fps_overlay,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = nullptr,
            .on_activate = &action_toggle_fps_overlay,
        };
        items[i++] = gui::ListItem<AppState>{
            .label = "Input Policy",
            .kind = gui::ItemKind::Value,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = nullptr,
            .get_value_label = &get_input_policy_label,
            .on_activate = &action_cycle_input_policy,
        };
        items[i++] = gui::ListItem<AppState>{
            .label = "Anim Curve",
            .kind = gui::ItemKind::Switch,
            .get_bool = &get_anim_curve,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = nullptr,
            .on_activate = &action_toggle_curve,
        };
        items[i++] = gui::ListItem<AppState>{
            .label = "Anim Presets",
            .kind = gui::ItemKind::Action,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = nullptr,
            .on_activate = &action_noop,
        };
        for (std::uint16_t p = 0; p < gui::motion::anim_preset_count(); ++p) {
            const auto preset = gui::motion::anim_preset_at(p);
            items[i++] = gui::ListItem<AppState>{
                .label = gui::motion::anim_preset_label(preset),
                .kind = gui::ItemKind::Action,
                .get_bool = nullptr,
                .get_u8 = nullptr,
                .get_chart = nullptr,
                .get_u16 = nullptr,
                .on_activate = anim_preset_action(preset),
            };
        }
        items[i++] = gui::ListItem<AppState>{
            .label = "Spring Presets",
            .kind = gui::ItemKind::Action,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = nullptr,
            .on_activate = &action_noop,
        };
        for (std::uint16_t p = 0; p < gui::motion::spring_preset_count(); ++p) {
            const auto preset = gui::motion::spring_preset_at(p);
            items[i++] = gui::ListItem<AppState>{
                .label = gui::motion::spring_preset_label(preset),
                .kind = gui::ItemKind::Action,
                .get_bool = nullptr,
                .get_u8 = nullptr,
                .get_chart = nullptr,
                .get_u16 = nullptr,
                .on_activate = spring_preset_action(preset),
            };
        }
        items[i++] = gui::ListItem<AppState>{
            .label = "Spring Custom",
            .kind = gui::ItemKind::Switch,
            .get_bool = &get_spring_override,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = nullptr,
            .on_activate = &action_toggle_spring_override,
        };
        items[i++] = gui::ListItem<AppState>{
            .label = "Spring W",
            .kind = gui::ItemKind::Value,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = &get_spring_omega_u16,
            .get_value_label = &get_spring_omega_label,
            .on_activate = &action_edit_spring_omega,
        };
        items[i++] = gui::ListItem<AppState>{
            .label = "Spring Z",
            .kind = gui::ItemKind::Value,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = &get_spring_zeta_u16,
            .get_value_label = &get_spring_zeta_label,
            .on_activate = &action_edit_spring_zeta,
        };
        items[i++] = gui::ListItem<AppState>{
            .label = "List Speed",
            .kind = gui::ItemKind::Value,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = &get_anim_list_ms,
            .on_activate = &action_edit_list_speed,
        };
        items[i++] = gui::ListItem<AppState>{
            .label = "Win Speed",
            .kind = gui::ItemKind::Value,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = &get_anim_win_ms,
            .on_activate = &action_edit_win_speed,
        };
        items[i++] = gui::ListItem<AppState>{
            .label = "Spot Speed",
            .kind = gui::ItemKind::Value,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .get_u16 = &get_anim_spot_ms,
            .on_activate = &action_edit_spot_speed,
        };
#ifndef NDEBUG
        assert(i == items.size());
#endif
        return items;
    }

    inline void action_edit_spring_omega(AppState& s) noexcept
    {
        s.popup.content = PopupContentKind::Setting;
        s.popup.setting = SettingField::SpringOmega;
        s.popup.show_ms = s.now_ms;
        s.semantics.capture.kind = gui::ui::CaptureKind::Popup;
        s.semantics.capture.owner_id = kPopupSettingsOwner;
    }

    inline void action_edit_spring_zeta(AppState& s) noexcept
    {
        s.popup.content = PopupContentKind::Setting;
        s.popup.setting = SettingField::SpringZeta;
        s.popup.show_ms = s.now_ms;
        s.semantics.capture.kind = gui::ui::CaptureKind::Popup;
        s.semantics.capture.owner_id = kPopupSettingsOwner;
    }
    inline void action_popup_demo(AppState& s) noexcept
    {
        s.popup.content = PopupContentKind::Lamp;
        s.popup.show_ms = s.now_ms;
        s.semantics.capture.kind = gui::ui::CaptureKind::Popup;
        s.semantics.capture.owner_id = kPopupLampOwner;
    }

    inline void action_open_lamp_popup(AppState& s) noexcept
    {
        s.popup.content = PopupContentKind::Lamp;
        s.popup.show_ms = s.now_ms;
        s.semantics.capture.kind = gui::ui::CaptureKind::Popup;
        s.semantics.capture.owner_id = kPopupLampOwner;
    }

    inline void action_edit_list_speed(AppState& s) noexcept
    {
        s.popup.content = PopupContentKind::Setting;
        s.popup.setting = SettingField::ListSpeed;
        s.popup.show_ms = s.now_ms;
        s.semantics.capture.kind = gui::ui::CaptureKind::Popup;
        s.semantics.capture.owner_id = kPopupSettingsOwner;
    }

    inline void action_edit_win_speed(AppState& s) noexcept
    {
        s.popup.content = PopupContentKind::Setting;
        s.popup.setting = SettingField::WinSpeed;
        s.popup.show_ms = s.now_ms;
        s.semantics.capture.kind = gui::ui::CaptureKind::Popup;
        s.semantics.capture.owner_id = kPopupSettingsOwner;
    }

    inline void action_edit_spot_speed(AppState& s) noexcept
    {
        s.popup.content = PopupContentKind::Setting;
        s.popup.setting = SettingField::SpotSpeed;
        s.popup.show_ms = s.now_ms;
        s.semantics.capture.kind = gui::ui::CaptureKind::Popup;
        s.semantics.capture.owner_id = kPopupSettingsOwner;
    }

    inline void action_back(AppState& s) noexcept
    {
        if (s.pages.is_root()) {
            s.request_quit = true;
            return;
        }
        remember_focus(s, s.pages.current());
        s.main_page.expand_valid = false;
        (void)s.pages.pop();
        restore_focus(s, s.pages.current());
    }
} // namespace app::detail

export namespace app
{
    inline constexpr std::int16_t kMainIconItemIndex = 9;
    inline constexpr const char* kWidgetSegments[] = {"Auto", "Eco", "Pro"};
    inline constexpr std::uint8_t kWidgetSegmentCount =
        (std::uint8_t)(sizeof(kWidgetSegments) / sizeof(kWidgetSegments[0]));
    // Main page items
    inline constexpr gui::ListItem<AppState> kMainItems[MainPageState::item_count] = {
        gui::ListItem<AppState>{
            .label = "Lamp",
            .kind = gui::ItemKind::Toggle,
            .get_bool = &detail::get_lamp_on,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .on_activate = &detail::action_toggle_lamp,
        },
        gui::ListItem<AppState>{
            .label = "Lamp Popup",
            .kind = gui::ItemKind::Action,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .on_activate = &detail::action_popup_demo,
        },
        gui::ListItem<AppState>{
            .label = "Battery:",
            .kind = gui::ItemKind::Progress,
            .get_bool = nullptr,
            .get_u8 = &detail::get_battery,
            .get_chart = nullptr,
            .on_activate = &detail::action_open_battery,
        },
        gui::ListItem<AppState>{
            .label = "Checkbox A",
            .kind = gui::ItemKind::Checkbox,
            .get_bool = &detail::get_check_a,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .on_activate = &detail::action_toggle_check_a,
        },
        gui::ListItem<AppState>{
            .label = "Checkbox B",
            .kind = gui::ItemKind::Checkbox,
            .get_bool = &detail::get_check_b,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .on_activate = &detail::action_toggle_check_b,
        },
        gui::ListItem<AppState>{
            .label = "Switch A",
            .kind = gui::ItemKind::Switch,
            .get_bool = &detail::get_switch_a,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .on_activate = &detail::action_toggle_switch_a,
        },
        gui::ListItem<AppState>{
            .label = "Progress",
            .kind = gui::ItemKind::Progress,
            .get_bool = nullptr,
            .get_u8 = &detail::get_progress_demo,
            .get_chart = nullptr,
            .on_activate = &detail::action_noop,
        },
        gui::ListItem<AppState>{
            .label = "Chart",
            .kind = gui::ItemKind::Action,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .on_activate = &detail::action_open_chart,
        },
        gui::ListItem<AppState>{
            .label = "Chart Wave",
            .kind = gui::ItemKind::Action,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .on_activate = &detail::action_open_chart_wave,
        },
        gui::ListItem<AppState>{
            .label = "Icon Mode",
            .kind = gui::ItemKind::Action,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .on_activate = &detail::action_open_icon,
        },
        gui::ListItem<AppState>{
            .label = "Widgets",
            .kind = gui::ItemKind::Action,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .on_activate = &detail::action_open_widgets,
        },
        gui::ListItem<AppState>{
            .label = "QR Demo",
            .kind = gui::ItemKind::Action,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .on_activate = &detail::action_open_qr,
        },
        gui::ListItem<AppState>{
            .label = "Settings",
            .kind = gui::ItemKind::Action,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .on_activate = &detail::action_open_settings,
        },
        gui::ListItem<AppState>{
            .label = "Long Label: Very long item name for scrolling demo",
            .kind = gui::ItemKind::Action,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .on_activate = &detail::action_noop,
        },
        gui::ListItem<AppState>{
            .label = "About",
            .kind = gui::ItemKind::Action,
            .get_bool = nullptr,
            .get_u8 = nullptr,
            .get_chart = nullptr,
            .on_activate = &detail::action_noop,
        },
    };

    inline constexpr gui::ListItem<AppState> kWidgetsItems[WidgetsPageState::item_count] = {
        gui::ListItem<AppState>{
            .label = "Controls",
            .kind = gui::ItemKind::Section,
        },
        gui::ListItem<AppState>{
            .label = "Range",
            .kind = gui::ItemKind::Range,
            .get_u8 = &detail::get_widget_range,
        },
        gui::ListItem<AppState>{
            .label = "Stepper",
            .kind = gui::ItemKind::Stepper,
            .get_u16 = &detail::get_widget_stepper,
        },
        gui::ListItem<AppState>{
            .label = "Segment",
            .kind = gui::ItemKind::Segmented,
            .get_index = &detail::get_widget_segment,
            .segments = kWidgetSegments,
            .segment_count = kWidgetSegmentCount,
        },
        gui::ListItem<AppState>{
            .label = "Inline",
            .kind = gui::ItemKind::ValueText,
            .get_value_text = &detail::get_widget_status,
        },
        gui::ListItem<AppState>{
            .label = "Feedback",
            .kind = gui::ItemKind::Section,
        },
        gui::ListItem<AppState>{
            .label = "Show Toast",
            .kind = gui::ItemKind::Action,
            .on_activate = &detail::action_show_widgets_toast,
        },
        gui::ListItem<AppState>{
            .label = "Back",
            .kind = gui::ItemKind::Action,
            .on_activate = &detail::action_back,
        },
    };

    inline const auto kSettingsItems = detail::build_settings_items();
} // namespace app








