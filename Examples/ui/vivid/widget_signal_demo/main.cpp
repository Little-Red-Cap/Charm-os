#include <cstdio>

import charm.foundation;
import charm.core.event;
import charm.widgets.button;
import charm.widgets.list;
import charm.widgets.menu_item;

namespace {
    struct Probe {
        int primary_clicks{0};
        int secondary_clicks{0};
        int menu_clicks{0};
        int list_clicks{0};
        int list_secondary_clicks{0};

        void on_primary_click() noexcept {
            ++primary_clicks;
        }

        void on_secondary_click() noexcept {
            ++secondary_clicks;
        }

        void on_menu_click() noexcept {
            ++menu_clicks;
        }

        void on_list_click() noexcept {
            ++list_clicks;
        }

        void on_list_secondary_click() noexcept {
            ++list_secondary_clicks;
        }
    };

    int g_button_legacy_clicks = 0;
    int g_menu_legacy_clicks = 0;
    int g_list_legacy_clicks = 0;

    void on_button_legacy_click() noexcept {
        ++g_button_legacy_clicks;
    }

    void on_menu_legacy_click() noexcept {
        ++g_menu_legacy_clicks;
    }

    void on_list_legacy_click() noexcept {
        ++g_list_legacy_clicks;
    }

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }
}

int main() {
    Probe probe{};

    Button button{"Apply"};
    button.set_on_click(Callback::bind<&on_button_legacy_click>());

    const auto primary =
        button.observe_click(util::delegate<>::bind<&Probe::on_primary_click>(probe));
    const auto secondary =
        button.observe_click(util::delegate<>::bind<&Probe::on_secondary_click>(probe));

    if (!expect(static_cast<bool>(primary), "connect primary button edge observer")) return 1;
    if (!expect(static_cast<bool>(secondary), "connect secondary button edge observer")) return 1;

    button.set_text("Apply Now");
    if (!expect(probe.primary_clicks == 0 && probe.secondary_clicks == 0,
                "programmatic text update does not create click edge")) {
        return 1;
    }
    if (!expect(g_button_legacy_clicks == 0, "programmatic text update stays silent for legacy callback")) return 1;

    if (!expect(button.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "button accepts in-bounds click")) {
        return 1;
    }
    if (!expect(probe.primary_clicks == 1 && probe.secondary_clicks == 1,
                "button click synchronously broadcasts edge observers")) {
        return 1;
    }
    if (!expect(g_button_legacy_clicks == 1, "button click still triggers legacy callback")) return 1;

    if (!expect(button.unobserve_click(primary.value()), "disconnect primary button edge observer")) return 1;
    if (!expect(!button.unobserve_click(primary.value()), "stale button edge token rejected")) return 1;

    if (!expect(button.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "button accepts second click")) {
        return 1;
    }
    if (!expect(probe.primary_clicks == 1, "disconnected primary observer stays silent")) return 1;
    if (!expect(probe.secondary_clicks == 2, "remaining observer still sees button edge")) return 1;
    if (!expect(g_button_legacy_clicks == 2, "legacy callback still works after observer disconnect")) return 1;

    button.set_enabled(false);
    if (!expect(!button.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "disabled button rejects click")) {
        return 1;
    }
    if (!expect(probe.secondary_clicks == 2, "disabled button does not emit hidden edge")) return 1;
    if (!expect(g_button_legacy_clicks == 2, "disabled button does not invoke legacy callback")) return 1;

    MenuItem menu{"Open"};
    menu.set_on_click(Callback::bind<&on_menu_legacy_click>());
    const auto menu_click =
        menu.observe_click(util::delegate<>::bind<&Probe::on_menu_click>(probe));
    if (!expect(static_cast<bool>(menu_click), "connect menu edge observer")) return 1;

    menu.set_text("Open File");
    if (!expect(probe.menu_clicks == 0, "menu text update does not create click edge")) return 1;
    if (!expect(g_menu_legacy_clicks == 0, "menu text update stays silent for legacy callback")) return 1;

    if (!expect(menu.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "menu accepts in-bounds click")) {
        return 1;
    }
    if (!expect(probe.menu_clicks == 1, "menu click synchronously broadcasts edge observers")) return 1;
    if (!expect(g_menu_legacy_clicks == 1, "menu click still triggers legacy callback")) return 1;

    if (!expect(menu.unobserve_click(menu_click.value()), "disconnect menu edge observer")) return 1;
    if (!expect(!menu.unobserve_click(menu_click.value()), "stale menu edge token rejected")) return 1;

    if (!expect(menu.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "menu accepts second click")) {
        return 1;
    }
    if (!expect(probe.menu_clicks == 1, "disconnected menu observer stays silent")) return 1;
    if (!expect(g_menu_legacy_clicks == 2, "menu legacy callback still works after observer disconnect")) return 1;

    menu.set_enabled(false);
    if (!expect(!menu.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "disabled menu rejects click")) {
        return 1;
    }
    if (!expect(probe.menu_clicks == 1, "disabled menu does not emit hidden edge")) return 1;
    if (!expect(g_menu_legacy_clicks == 2, "disabled menu does not invoke legacy callback")) return 1;

    ListItem list{"Alpha"};
    list.set_on_click(Callback::bind<&on_list_legacy_click>());
    const auto list_primary =
        list.observe_click(util::delegate<>::bind<&Probe::on_list_click>(probe));
    const auto list_secondary =
        list.observe_click(util::delegate<>::bind<&Probe::on_list_secondary_click>(probe));
    if (!expect(static_cast<bool>(list_primary), "connect primary list edge observer")) return 1;
    if (!expect(static_cast<bool>(list_secondary), "connect secondary list edge observer")) return 1;

    list.set_text("Alpha Prime");
    if (!expect(probe.list_clicks == 0 && probe.list_secondary_clicks == 0,
                "list text update does not create click edge")) {
        return 1;
    }
    if (!expect(g_list_legacy_clicks == 0, "list text update stays silent for legacy callback")) return 1;

    if (!expect(list.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "list accepts in-bounds click")) {
        return 1;
    }
    if (!expect(probe.list_clicks == 1 && probe.list_secondary_clicks == 1,
                "list click synchronously broadcasts edge observers")) {
        return 1;
    }
    if (!expect(g_list_legacy_clicks == 1, "list click still triggers legacy callback")) return 1;

    if (!expect(list.unobserve_click(list_primary.value()), "disconnect primary list edge observer")) return 1;
    if (!expect(!list.unobserve_click(list_primary.value()), "stale list edge token rejected")) return 1;

    if (!expect(list.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "list accepts second click")) {
        return 1;
    }
    if (!expect(probe.list_clicks == 1, "disconnected primary list observer stays silent")) return 1;
    if (!expect(probe.list_secondary_clicks == 2, "remaining list observer still sees edge")) return 1;
    if (!expect(g_list_legacy_clicks == 2, "list legacy callback still works after observer disconnect")) return 1;

    list.set_enabled(false);
    if (!expect(!list.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "disabled list rejects click")) {
        return 1;
    }
    if (!expect(probe.list_secondary_clicks == 2, "disabled list does not emit hidden edge")) return 1;
    if (!expect(g_list_legacy_clicks == 2, "disabled list does not invoke legacy callback")) return 1;

    std::printf("[button] primary=%d secondary=%d legacy=%d\n",
                probe.primary_clicks,
                probe.secondary_clicks,
                g_button_legacy_clicks);
    std::printf("[menu_item] clicks=%d legacy=%d\n",
                probe.menu_clicks,
                g_menu_legacy_clicks);
    std::printf("[list_item] primary=%d secondary=%d legacy=%d\n",
                probe.list_clicks,
                probe.list_secondary_clicks,
                g_list_legacy_clicks);
    std::puts("[widget_signal_demo] ok");
    return 0;
}
