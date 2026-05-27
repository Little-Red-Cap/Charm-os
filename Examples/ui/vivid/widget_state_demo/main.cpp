#include <cmath>
#include <cstddef>
#include <cstdio>

import charm.core;
import charm.core.event;
import charm.widgets.arc;
import charm.widgets.checkbox;
import charm.widgets.dropdown;
import charm.widgets.progress_bar_simple;
import charm.widgets.slider;

namespace {
    constexpr float kFloatEpsilon = 0.0001f;

    struct Probe {
        int checkbox_changes{0};
        bool checkbox_old{false};
        bool checkbox_new{false};

        int dropdown_changes{0};
        int dropdown_old{0};
        int dropdown_new{0};

        int slider_changes{0};
        int slider_old{0};
        int slider_new{0};

        int progress_changes{0};
        int progress_old{0};
        int progress_new{0};

        int arc_changes{0};
        float arc_old{0.0f};
        float arc_new{0.0f};

        void on_checkbox_changed(const bool& now, const bool& old) noexcept {
            ++checkbox_changes;
            checkbox_old = old;
            checkbox_new = now;
        }

        void on_dropdown_changed(const int& now, const int& old) noexcept {
            ++dropdown_changes;
            dropdown_old = old;
            dropdown_new = now;
        }

        void on_slider_changed(const int& now, const int& old) noexcept {
            ++slider_changes;
            slider_old = old;
            slider_new = now;
        }

        void on_progress_changed(const int& now, const int& old) noexcept {
            ++progress_changes;
            progress_old = old;
            progress_new = now;
        }

        void on_arc_changed(const float& now, const float& old) noexcept {
            ++arc_changes;
            arc_old = old;
            arc_new = now;
        }
    };

    int g_checkbox_callbacks = 0;
    int g_dropdown_callbacks = 0;
    int g_slider_callbacks = 0;

    void on_checkbox_command() noexcept {
        ++g_checkbox_callbacks;
    }

    void on_dropdown_command() noexcept {
        ++g_dropdown_callbacks;
    }

    void on_slider_command() noexcept {
        ++g_slider_callbacks;
    }

    [[nodiscard]] bool nearly_equal(float lhs, float rhs) noexcept {
        return std::fabs(lhs - rhs) <= kFloatEpsilon;
    }

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    unsigned widget_state_summary_case_count{0};

    void print_widget_state_run_begin() noexcept {
        std::printf("[wst] run=widget_state_demo phase=begin\n");
    }

    void print_widget_state_run_end(bool ok) noexcept {
        std::printf("[wst] run=widget_state_demo phase=end result=%s cases=%u\n",
                    ok ? "ok" : "fail",
                    widget_state_summary_case_count);
    }

    void print_widget_state_case(const char* name) noexcept {
        ++widget_state_summary_case_count;
        std::printf("[wst] case=%s", name);
    }
}

int main() {
    print_widget_state_run_begin();

    Probe probe{};

    Checkbox checkbox{"Enable"};
    checkbox.set_on_change(Callback::bind<&on_checkbox_command>());
    const auto checkbox_conn =
        checkbox.observe_checked(util::delegate<const bool&, const bool&>::bind<&Probe::on_checkbox_changed>(probe));
    if (!expect(static_cast<bool>(checkbox_conn), "connect checkbox observe_checked")) return 1;
    if (!expect(!checkbox.is_checked(), "checkbox starts unchecked")) return 1;

    checkbox.set_checked(true);
    if (!expect(checkbox.is_checked(), "checkbox programmatic set updates truth")) return 1;
    if (!expect(probe.checkbox_changes == 1, "checkbox observe sees programmatic truth change")) return 1;
    if (!expect(!probe.checkbox_old && probe.checkbox_new, "checkbox observe reports old/new")) return 1;
    if (!expect(g_checkbox_callbacks == 0, "checkbox programmatic set stays silent for legacy callback")) return 1;

    checkbox.set_checked(true);
    if (!expect(probe.checkbox_changes == 1, "checkbox suppresses unchanged truth set")) return 1;
    if (!expect(g_checkbox_callbacks == 0, "checkbox unchanged truth set stays silent")) return 1;

    if (!expect(checkbox.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "checkbox click toggles")) return 1;
    if (!expect(!checkbox.is_checked(), "checkbox click updates truth")) return 1;
    if (!expect(probe.checkbox_changes == 2, "checkbox observe sees interactive change")) return 1;
    if (!expect(g_checkbox_callbacks == 1, "checkbox click triggers legacy callback")) return 1;

    Dropdown dropdown{};
    dropdown.add_option("Option 2");
    dropdown.set_on_change(Callback::bind<&on_dropdown_command>());
    const auto dropdown_conn =
        dropdown.observe_selected(util::delegate<const int&, const int&>::bind<&Probe::on_dropdown_changed>(probe));
    if (!expect(static_cast<bool>(dropdown_conn), "connect dropdown observe_selected")) return 1;
    if (!expect(dropdown.selected() == 0, "dropdown starts with first option selected")) return 1;

    dropdown.set_selected(0);
    if (!expect(dropdown.selected() == 0, "dropdown keeps same truth on same selection request")) return 1;
    if (!expect(probe.dropdown_changes == 0, "dropdown observe suppresses same selection request")) return 1;
    if (!expect(g_dropdown_callbacks == 1, "dropdown legacy callback still fires for valid selection command")) return 1;

    dropdown.set_selected(1);
    if (!expect(dropdown.selected() == 1, "dropdown updates selected truth")) return 1;
    if (!expect(probe.dropdown_changes == 1, "dropdown observe sees real selected change")) return 1;
    if (!expect(probe.dropdown_old == 0 && probe.dropdown_new == 1, "dropdown observe reports old/new")) return 1;
    if (!expect(g_dropdown_callbacks == 2, "dropdown legacy callback also fires for real change")) return 1;

    Slider slider{};
    slider.set_range(0, 100);
    slider.set_on_change(Callback::bind<&on_slider_command>());
    const auto slider_conn =
        slider.observe_value(util::delegate<const int&, const int&>::bind<&Probe::on_slider_changed>(probe));
    if (!expect(static_cast<bool>(slider_conn), "connect slider observe_value")) return 1;

    slider.set_value(42);
    if (!expect(slider.value() == 42, "slider updates value truth")) return 1;
    if (!expect(probe.slider_changes == 1, "slider observe sees real value change")) return 1;
    if (!expect(probe.slider_old == 0 && probe.slider_new == 42, "slider observe reports old/new")) return 1;
    if (!expect(g_slider_callbacks == 1, "slider real value change triggers legacy callback")) return 1;

    slider.set_value(42);
    if (!expect(probe.slider_changes == 1, "slider suppresses unchanged value set")) return 1;
    if (!expect(g_slider_callbacks == 1, "slider unchanged value set stays silent")) return 1;

    slider.set_range(0, 10);
    if (!expect(slider.value() == 10, "slider range clamp updates truth")) return 1;
    if (!expect(probe.slider_changes == 2, "slider observe sees range clamp truth change")) return 1;
    if (!expect(probe.slider_old == 42 && probe.slider_new == 10, "slider observe reports range clamp old/new")) return 1;
    if (!expect(g_slider_callbacks == 1, "slider range clamp stays silent for legacy callback")) return 1;

    ProgressBarSimple progress{};
    const auto progress_conn =
        progress.observe_value(util::delegate<const int&, const int&>::bind<&Probe::on_progress_changed>(probe));
    if (!expect(static_cast<bool>(progress_conn), "connect progress observe_value")) return 1;

    progress.set_value(25);
    if (!expect(progress.value() == 25, "progress updates value truth")) return 1;
    if (!expect(probe.progress_changes == 1, "progress observe sees value change")) return 1;
    if (!expect(probe.progress_old == 0 && probe.progress_new == 25, "progress observe reports old/new")) return 1;

    progress.set_range(50, 100);
    if (!expect(progress.value() == 50, "progress range clamp updates truth")) return 1;
    if (!expect(probe.progress_changes == 2, "progress observe sees clamp change")) return 1;
    if (!expect(probe.progress_old == 25 && probe.progress_new == 50, "progress observe reports clamp old/new")) return 1;

    Arc arc{};
    const auto arc_conn =
        arc.observe_value(util::delegate<const float&, const float&>::bind<&Probe::on_arc_changed>(probe));
    if (!expect(static_cast<bool>(arc_conn), "connect arc observe_value")) return 1;

    arc.set_value(0.25f);
    if (!expect(nearly_equal(arc.value(), 0.25f), "arc updates float truth")) return 1;
    if (!expect(probe.arc_changes == 1, "arc observe sees float truth change")) return 1;
    if (!expect(nearly_equal(probe.arc_old, 1.0f) && nearly_equal(probe.arc_new, 0.25f),
                "arc observe reports float old/new")) {
        return 1;
    }

    if (!expect(arc.unobserve_value(arc_conn.value()), "arc disconnects observe token")) return 1;
    arc.set_value(0.75f);
    if (!expect(nearly_equal(arc.value(), 0.75f), "arc still updates truth after disconnect")) return 1;
    if (!expect(probe.arc_changes == 1, "arc disconnected observer stays silent")) return 1;

    print_widget_state_case("checkbox_state");
    std::printf(" changes=%d callbacks=%d now=%d\n",
                probe.checkbox_changes,
                g_checkbox_callbacks,
                checkbox.is_checked() ? 1 : 0);
    print_widget_state_case("dropdown_state");
    std::printf(" changes=%d callbacks=%d selected=%d\n",
                probe.dropdown_changes,
                g_dropdown_callbacks,
                dropdown.selected());
    print_widget_state_case("slider_state");
    std::printf(" changes=%d callbacks=%d value=%d\n",
                probe.slider_changes,
                g_slider_callbacks,
                slider.value());
    print_widget_state_case("progress_state");
    std::printf(" changes=%d value=%d\n", probe.progress_changes, progress.value());
    print_widget_state_case("arc_state");
    std::printf(" changes=%d value=%.2f\n", probe.arc_changes, static_cast<double>(arc.value()));
    print_widget_state_run_end(true);
    std::puts("[widget_state_demo] ok");
    return 0;
}
