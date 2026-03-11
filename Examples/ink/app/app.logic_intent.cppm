// app.logic_intent.cppm
// Intent -> navigation semantics -> update AppState (page stack / business data / focus)
// Actions are callback-based: pages bind on_activate via static item tables.
// Constraints: zero dynamic allocation; item tables are constexpr + function pointers.

module;
#include <cstdint>
#include <optional>

export module app.logic_intent;

import app.state;
import app.pages;
import app.icons;

import gui.list_view;
import gui.ui_tree;
import gui.ui_semantics;
import gui.ui_focus_bookmark;
import gui.motion;

import gui.input;

namespace gui_input = gui::input;


namespace app::detail
{
    inline void activate_item(AppState&                      s,
                              const gui::ListItem<AppState>* items,
                              const std::int16_t             count,
                              const std::int16_t             focus) noexcept
    {
        if (!items || count <= 0) return;
        if (focus < 0 || focus >= count) return;
        auto fn = items[focus].on_activate;
        if (fn) fn(s);
    }


    inline void reduce_main(AppState& s, const std::optional<gui_input::Intent>& it) noexcept
    {
        const auto& sem = s.semantics;
        if (s.popup.content == PopupContentKind::Lamp) {
            if (it) {
                using gui_input::IntentType;
                if (it->type == IntentType::Back || it->type == IntentType::Activate) {
                    s.popup.content = PopupContentKind::None;
                    return;
                }
                if (it->type == IntentType::Adjust) {
                    const int delta = (int)it->a;
                    if (delta != 0) {
                        int v = (int)s.data.lamp_brightness + delta * 5;
                        if (v < 0) v = 0;
                        if (v > 100) v = 100;
                        s.data.lamp_brightness = (std::uint8_t)v;
                        s.data.lamp_on = (s.data.lamp_brightness > 0);
                    }
                    return;
                }
            }
            return;
        }

        if (sem.activation.kind == gui::ui::ActivationKind::Back) {
            s.main_page.expand_valid = false;
            s.request_quit = true;
            return;
        }

        if (sem.activation.kind == gui::ui::ActivationKind::Activate) {
            const std::int16_t focus = sem.focus.index;
            activate_item(s, kMainItems, MainPageState::item_count, focus);
            s.main_page.last_activate_ms = s.now_ms;
        }
    }

    inline void reduce_battery(AppState& s, const std::optional<gui_input::Intent>& it) noexcept
    {
        const auto& sem = s.semantics;
        if (sem.activation.kind == gui::ui::ActivationKind::Back) {
            // ????????????????????
            std::int16_t idx = sem.focus.index;
            if (idx < 0 || idx >= SettingsPageState::item_count) idx = 0;
            s.settings_page.focus_bk.index = idx;
            s.main_page.expand_valid = false;
            (void)s.pages.pop();
            // ???????????????? settings ? index
            const auto domain_id = gui::ui::fnv1a("main_list");
            s.semantics.focus.domain_id = domain_id;
            s.semantics.focus.count = MainPageState::item_count;
            std::int16_t midx = s.main_page.focus_bk.index;
            if (midx < 0 || midx >= s.semantics.focus.count) midx = 0;
            s.semantics.focus.index = midx;
            s.semantics.focus.target_id = gui::ui::list_id(domain_id, (std::uint16_t)(midx + 1));
            s.semantics.capture.kind = gui::ui::CaptureKind::None;
            s.semantics.capture.owner_id = gui::ui::kNullId;
            return;
        }

        if (it && it->type == gui_input::IntentType::Adjust) {
            const int delta = (int)it->a;
            if (delta != 0) {
                int v = (int)s.data.battery + delta * (int)s.battery_page.step;
                if (v < 0) v = 0;
                if (v > 100) v = 100;
                s.data.battery = (std::uint8_t)v;
            }
        }

        if (sem.activation.kind == gui::ui::ActivationKind::Activate) {
            s.battery_page.step = (s.battery_page.step == 1) ? 5 : 1;
        }
    }

    inline void reduce_chart(AppState& s) noexcept
    {
        const auto& sem = s.semantics;
        if (sem.activation.kind == gui::ui::ActivationKind::Back) {
            s.main_page.expand_valid = false;
            (void)s.pages.pop();
            detail::restore_focus(s, s.pages.current());
            return;
        }
        if (sem.activation.kind == gui::ui::ActivationKind::Activate) {
            s.chart_page.last_activate_ms = s.now_ms;
        }
    }

    inline void reduce_icon(AppState& s, const std::optional<gui_input::Intent>& it) noexcept
    {
        const auto& sem = s.semantics;
        if (it && it->type == gui_input::IntentType::Adjust) {
            const int delta = (int)it->a;
            if (delta != 0) {
                const std::int16_t count = app::icons::icon_count();
                if (count > 0) {
                    int idx = (int)s.icon_page.index + delta;
                    while (idx < 0) idx += count;
                    while (idx >= count) idx -= count;
                    s.icon_page.index = (std::int16_t)idx;
                    s.icon_page.last_ms = s.now_ms;
                }
            }
            return;
        }
        if (sem.activation.kind == gui::ui::ActivationKind::Back) {
            s.main_page.expand_valid = false;
            (void)s.pages.pop();
            detail::restore_focus(s, s.pages.current());
            return;
        }
        if (sem.activation.kind == gui::ui::ActivationKind::Activate) {
            s.main_page.expand_valid = false;
            (void)s.pages.pop();
            detail::restore_focus(s, s.pages.current());
        }
    }

    inline void reduce_settings(AppState& s, const std::optional<gui_input::Intent>& it) noexcept
    {
        const auto& sem = s.semantics;
        const bool popup_open = (sem.capture.kind == gui::ui::CaptureKind::Popup &&
                                 sem.capture.owner_id == kPopupSettingsOwner);

        if ((sem.activation.kind == gui::ui::ActivationKind::Back ||
             sem.activation.kind == gui::ui::ActivationKind::Activate) &&
            sem.activation.target_id == kPopupSettingsOwner) {
            s.popup.content = PopupContentKind::None;
            return;
        }

        if (sem.activation.kind == gui::ui::ActivationKind::Back) {
            s.main_page.expand_valid = false;
            (void)s.pages.pop();
            detail::restore_focus(s, s.pages.current());
            return;
        }

        if (popup_open && s.popup.content == PopupContentKind::Setting) {
            // ESCAPE_HATCH: popup needs adjustable amount; current Activation does not carry delta.
            // TODO: move to Activation v2 or a dedicated adjust intent payload.
            if (sem.activation.kind == gui::ui::ActivationKind::Submit && it &&
                it->type == gui_input::IntentType::Adjust) {
                const int delta = (int)it->a;
                if (delta != 0) {
                    constexpr int kStep = 10;
                    constexpr int kMin = 40;
                    constexpr int kMax = 400;
                    auto clamp = [](int v, int lo, int hi) {
                        if (v < lo) return lo;
                        if (v > hi) return hi;
                        return v;
                    };
                if (s.popup.setting == SettingField::ListSpeed) {
                    const int v = clamp((int)s.ui.anim.list.base_ms + delta * kStep, kMin, kMax);
                    s.ui.anim.list.base_ms = (std::uint16_t)v;
                    s.ui.anim_preset = gui::motion::AnimPreset::Custom;
                } else if (s.popup.setting == SettingField::WinSpeed) {
                    const int v = clamp((int)s.ui.anim.win.base_ms + delta * kStep, kMin, kMax);
                    s.ui.anim.win.base_ms = (std::uint16_t)v;
                    s.ui.anim_preset = gui::motion::AnimPreset::Custom;
                } else if (s.popup.setting == SettingField::SpotSpeed) {
                    const int v = clamp((int)s.ui.anim.spot.base_ms + delta * kStep, kMin, kMax);
                    s.ui.anim.spot.base_ms = (std::uint16_t)v;
                    s.ui.anim_preset = gui::motion::AnimPreset::Custom;
                } else if (s.popup.setting == SettingField::SpringOmega) {
                    const int cur = (int)(s.ui.anim.spring_omega * 100.0f + 0.5f);
                    const int v = clamp(cur + delta * 5, 100, 6000);
                    s.ui.anim.spring_omega = (float)v / 100.0f;
                    s.ui.anim.spring_override = true;
                    s.ui.anim.spring_preset = gui::motion::SpringPreset::Custom;
                } else if (s.popup.setting == SettingField::SpringZeta) {
                    const int cur = (int)(s.ui.anim.spring_zeta * 100.0f + 0.5f);
                    const int v = clamp(cur + delta * 5, 0, 200);
                    s.ui.anim.spring_zeta = (float)v / 100.0f;
                    s.ui.anim.spring_override = true;
                    s.ui.anim.spring_preset = gui::motion::SpringPreset::Custom;
                }
                }
            }
            return;
        }

        if (sem.activation.kind == gui::ui::ActivationKind::Activate) {
            const std::int16_t focus = sem.focus.index;
            activate_item(s, kSettingsItems.data(), SettingsPageState::item_count, focus);
            s.settings_page.last_activate_ms = s.now_ms;
        }
    }

    inline void reduce_widgets(AppState& s, const std::optional<gui_input::Intent>& it) noexcept
    {
        auto& sem = s.semantics;
        if (sem.activation.kind == gui::ui::ActivationKind::Back) {
            s.main_page.expand_valid = false;
            (void)s.pages.pop();
            detail::restore_focus(s, s.pages.current());
            return;
        }

        const std::int16_t count = WidgetsPageState::item_count;
        const std::int16_t focus = sem.focus.index;
        if (focus >= 0 && focus < count) {
            if (kWidgetsItems[focus].kind == gui::ItemKind::Section) {
                const int dir = (sem.focus.last_dir != 0) ? sem.focus.last_dir : 1;
                int idx = focus;
                for (int i = 0; i < count; ++i) {
                    idx += dir;
                    if (idx < 0) idx = count - 1;
                    if (idx >= count) idx = 0;
                    if (kWidgetsItems[idx].kind != gui::ItemKind::Section) {
                        sem.focus.index = (std::int16_t)idx;
                        sem.focus.target_id = gui::ui::list_id(sem.focus.domain_id, (std::uint16_t)(idx + 1));
                        break;
                    }
                }
            }
        }

        if (it && it->type == gui_input::IntentType::Adjust) {
            const int delta = (int)it->a;
            const std::int16_t idx = sem.focus.index;
            if (idx >= 0 && idx < count && delta != 0) {
                const auto kind = kWidgetsItems[idx].kind;
                if (kind == gui::ItemKind::Range) {
                    int v = (int)s.widgets_page.range_value + delta * 2;
                    if (v < 0) v = 0;
                    if (v > 100) v = 100;
                    s.widgets_page.range_value = (std::uint8_t)v;
                    return;
                }
                if (kind == gui::ItemKind::Stepper) {
                    int v = (int)s.widgets_page.stepper_value + delta;
                    if (v < 0) v = 0;
                    if (v > 999) v = 999;
                    s.widgets_page.stepper_value = (std::uint16_t)v;
                    return;
                }
                if (kind == gui::ItemKind::Segmented) {
                    int v = (int)s.widgets_page.segmented_index + delta;
                    const int count_seg = (int)kWidgetSegmentCount;
                    if (count_seg > 0) {
                        while (v < 0) v += count_seg;
                        while (v >= count_seg) v -= count_seg;
                        s.widgets_page.segmented_index = (std::uint8_t)v;
                    }
                    return;
                }
            }
        }

        if (sem.activation.kind == gui::ui::ActivationKind::Activate) {
            const std::int16_t idx = sem.focus.index;
            activate_item(s, kWidgetsItems, WidgetsPageState::item_count, idx);
            s.widgets_page.last_activate_ms = s.now_ms;
        }
    }
} // namespace app::detail


export namespace app
{
    // remove inline
    // NOTE: Do not mark this as inline.
    // GCC -fmodules-ts (arm-none-eabi) can fail to load module bindings
    // for large/complex inline exports ("Bad file data"), while non-inline
    // works reliably. Keep it non-inline until toolchain support improves.
    void apply_intent(AppState& s, const std::optional<gui_input::Intent>& it) noexcept
    {
        if (s.request_quit) return;

        auto& sem = s.semantics;
        if (s.pages.current() == PageId::Main) {
            sem.model_id = gui::ui::fnv1a("main_root");
            sem.nav.kind = gui::ui::NavKind::List;
            sem.nav.wrap = gui::ui::NavWrap::Ring;
            const auto domain_id = gui::ui::fnv1a("main_list");
            sem.focus.domain_id = domain_id;
            sem.focus.count = MainPageState::item_count;
            if (sem.focus.index < 0 || sem.focus.index >= sem.focus.count) {
                gui::ui::restore_focus(sem, s.main_page.focus_bk, domain_id, MainPageState::item_count, 0);
            }
            if (sem.focus.target_id == gui::ui::kNullId) {
                sem.focus.target_id = gui::ui::list_id(domain_id, (std::uint16_t)(sem.focus.index + 1));
            }
            if (s.popup.content == PopupContentKind::Lamp) {
                sem.capture.kind = gui::ui::CaptureKind::Popup;
                sem.capture.owner_id = kPopupLampOwner;
            } else {
                sem.capture.kind = gui::ui::CaptureKind::None;
                sem.capture.owner_id = gui::ui::kNullId;
            }
        } else if (s.pages.current() == PageId::Settings) {
            sem.model_id = gui::ui::fnv1a("settings_root");
            sem.nav.kind = gui::ui::NavKind::List;
            sem.nav.wrap = gui::ui::NavWrap::Ring;
            const auto domain_id = gui::ui::fnv1a("settings_list");
            sem.focus.domain_id = domain_id;
            sem.focus.count = SettingsPageState::item_count;
            if (sem.focus.index < 0 || sem.focus.index >= sem.focus.count) {
                gui::ui::restore_focus(sem, s.settings_page.focus_bk, domain_id, SettingsPageState::item_count, 0);
            }
            if (sem.focus.target_id == gui::ui::kNullId) {
                sem.focus.target_id = gui::ui::list_id(domain_id, (std::uint16_t)(sem.focus.index + 1));
            }
            if (s.popup.content == PopupContentKind::Setting) {
                sem.capture.kind = gui::ui::CaptureKind::Popup;
                sem.capture.owner_id = kPopupSettingsOwner;
            } else {
                sem.capture.kind = gui::ui::CaptureKind::None;
                sem.capture.owner_id = gui::ui::kNullId;
            }
        } else if (s.pages.current() == PageId::Battery) {
            sem.model_id = gui::ui::fnv1a("battery_root");
            sem.nav.kind = gui::ui::NavKind::Free;
            sem.nav.wrap = gui::ui::NavWrap::Clamp;
            sem.focus.domain_id = gui::ui::kNullId;
            sem.focus.index = -1;
            sem.focus.count = 0;
            sem.focus.target_id = gui::ui::kNullId;
            sem.capture.kind = gui::ui::CaptureKind::None;
            sem.capture.owner_id = gui::ui::kNullId;
        } else if (s.pages.current() == PageId::Chart) {
            sem.model_id = gui::ui::fnv1a("chart_root");
            sem.nav.kind = gui::ui::NavKind::Free;
            sem.nav.wrap = gui::ui::NavWrap::Clamp;
            sem.focus.domain_id = gui::ui::kNullId;
            sem.focus.index = -1;
            sem.focus.count = 0;
            sem.focus.target_id = gui::ui::kNullId;
            sem.capture.kind = gui::ui::CaptureKind::None;
            sem.capture.owner_id = gui::ui::kNullId;
        } else if (s.pages.current() == PageId::ChartWave) {
            sem.model_id = gui::ui::fnv1a("chart_wave_root");
            sem.nav.kind = gui::ui::NavKind::Free;
            sem.nav.wrap = gui::ui::NavWrap::Clamp;
            sem.focus.domain_id = gui::ui::kNullId;
            sem.focus.index = -1;
            sem.focus.count = 0;
            sem.focus.target_id = gui::ui::kNullId;
            sem.capture.kind = gui::ui::CaptureKind::None;
            sem.capture.owner_id = gui::ui::kNullId;
        } else if (s.pages.current() == PageId::Icon) {
            sem.model_id = gui::ui::fnv1a("icon_root");
            sem.nav.kind = gui::ui::NavKind::Free;
            sem.nav.wrap = gui::ui::NavWrap::Clamp;
            sem.focus.domain_id = gui::ui::kNullId;
            sem.focus.index = -1;
            sem.focus.count = 0;
            sem.focus.target_id = gui::ui::kNullId;
            sem.capture.kind = gui::ui::CaptureKind::None;
            sem.capture.owner_id = gui::ui::kNullId;
        } else if (s.pages.current() == PageId::Widgets) {
            sem.model_id = gui::ui::fnv1a("widgets_root");
            sem.nav.kind = gui::ui::NavKind::List;
            sem.nav.wrap = gui::ui::NavWrap::Ring;
            const auto domain_id = gui::ui::fnv1a("widgets_list");
            sem.focus.domain_id = domain_id;
            sem.focus.count = WidgetsPageState::item_count;
            if (sem.focus.index < 0 || sem.focus.index >= sem.focus.count) {
                gui::ui::restore_focus(sem, s.widgets_page.focus_bk, domain_id, WidgetsPageState::item_count, 0);
            }
            if (sem.focus.target_id == gui::ui::kNullId) {
                sem.focus.target_id = gui::ui::list_id(domain_id, (std::uint16_t)(sem.focus.index + 1));
            }
            sem.capture.kind = gui::ui::CaptureKind::None;
            sem.capture.owner_id = gui::ui::kNullId;
        } else {
            sem.capture.kind = gui::ui::CaptureKind::None;
            sem.capture.owner_id = gui::ui::kNullId;
        }

        sem = gui::ui::reduce_semantics(sem, it);

        if (s.pages.current() == PageId::Main) {
            detail::reduce_main(s, it);
            return;
        }
        if (s.pages.current() == PageId::Settings) {
            detail::reduce_settings(s, it);
            return;
        }
        switch (s.pages.current()) {
        case PageId::Battery:
            detail::reduce_battery(s, it);
            break;
        case PageId::Chart:
        case PageId::ChartWave:
            detail::reduce_chart(s);
            break;
        case PageId::Icon:
            detail::reduce_icon(s, it);
            break;
        case PageId::Widgets:
            detail::reduce_widgets(s, it);
            break;
        default:
            break;
        }
    }

    void pump_input(AppState& s, const std::uint32_t now_ms) noexcept
    {
        if (!s.input_policy.poll) return;
        while (auto it = s.input_policy.poll_intent(now_ms)) {
            apply_intent(s, it);
            if (s.request_quit) return;
        }
    }
} // namespace app





