#include <cstdio>

import charm.foundation;
import charm.core.event;
import charm.widgets.button;

namespace {
    struct Probe {
        int primary_clicks{0};
        int secondary_clicks{0};

        void on_primary_click() noexcept {
            ++primary_clicks;
        }

        void on_secondary_click() noexcept {
            ++secondary_clicks;
        }
    };

    int g_legacy_clicks = 0;

    void on_legacy_click() noexcept {
        ++g_legacy_clicks;
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
    button.set_on_click(Callback::bind<&on_legacy_click>());

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
    if (!expect(g_legacy_clicks == 0, "programmatic text update stays silent for legacy callback")) return 1;

    if (!expect(button.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "button accepts in-bounds click")) {
        return 1;
    }
    if (!expect(probe.primary_clicks == 1 && probe.secondary_clicks == 1,
                "button click synchronously broadcasts edge observers")) {
        return 1;
    }
    if (!expect(g_legacy_clicks == 1, "button click still triggers legacy callback")) return 1;

    if (!expect(button.unobserve_click(primary.value()), "disconnect primary button edge observer")) return 1;
    if (!expect(!button.unobserve_click(primary.value()), "stale button edge token rejected")) return 1;

    if (!expect(button.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "button accepts second click")) {
        return 1;
    }
    if (!expect(probe.primary_clicks == 1, "disconnected primary observer stays silent")) return 1;
    if (!expect(probe.secondary_clicks == 2, "remaining observer still sees button edge")) return 1;
    if (!expect(g_legacy_clicks == 2, "legacy callback still works after observer disconnect")) return 1;

    button.set_enabled(false);
    if (!expect(!button.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "disabled button rejects click")) {
        return 1;
    }
    if (!expect(probe.secondary_clicks == 2, "disabled button does not emit hidden edge")) return 1;
    if (!expect(g_legacy_clicks == 2, "disabled button does not invoke legacy callback")) return 1;

    std::printf("[button] primary=%d secondary=%d legacy=%d\n",
                probe.primary_clicks,
                probe.secondary_clicks,
                g_legacy_clicks);
    std::puts("[widget_signal_demo] ok");
    return 0;
}
