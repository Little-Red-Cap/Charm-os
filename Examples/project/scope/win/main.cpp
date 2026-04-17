import scope.app;
import charm.core.config;
import charm.core.container;
import charm.core.event;
import charm.core.factory;
import charm.core.gui;
import charm.core.input_adapter;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.framebuffer;
import charm.widgets.waveform;
import charm.widgets.label;
import charm.widgets.button;
import input.raw_event;

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif
#include <SDL3/SDL.h>
#if defined(_WIN32)
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#endif

#include <array>
#include <cstdio>

namespace {
    constexpr int kPadding = 16;
    constexpr int kTopBar = 40;
    constexpr int kBottomBar = 80;

    struct UiHandles {
        WidgetHandle root{};
        WidgetHandle top_left{};
        WidgetHandle top_mid{};
        WidgetHandle top_right{};
        WidgetHandle waveform{};
        WidgetHandle title{};
        WidgetHandle time_div{};
        WidgetHandle volt_div{};
        WidgetHandle trigger{};
        WidgetHandle mode{};
        WidgetHandle controls{};
        WidgetHandle btn_time_dec{};
        WidgetHandle btn_time_inc{};
        WidgetHandle btn_volt_dec{};
        WidgetHandle btn_volt_inc{};
        WidgetHandle btn_win_dec{};
        WidgetHandle btn_win_inc{};
        WidgetHandle btn_edge{};
        WidgetHandle btn_trig_mode{};
        WidgetHandle btn_roll{};
        WidgetHandle trig_mode{};
        WidgetHandle trig_state{};
        WidgetHandle trig_light{};
        WidgetHandle trig_reset{};
        WidgetHandle measure{};
        WidgetHandle hint{};
    };

    struct ScopeUiContext {
        scope::App* app{nullptr};
        UiFactory* factory{nullptr};
        UiHandles handles{};
        bool dragging{false};
        bool dragging_window{false};
        int drag_origin_y{0};
        float drag_window_origin{0.0f};
        int trigger_pulse_frames{0};
        scope::TriggerState last_trigger_state{scope::TriggerState::Armed};

        std::array<char, 64> mode_text{};
        std::array<char, 64> trigger_text{};
        std::array<char, 64> trig_mode_text{};
        std::array<char, 64> trig_state_text{};
        std::array<char, 64> measure_text{};
        std::array<char, 64> time_text{};
        std::array<char, 64> volt_text{};

        void set_label(WidgetHandle h, const char* text) {
            if (!factory) return;
            if (auto* label = factory->get_label(h)) {
                label->set_text(text);
            }
        }

        void set_button_text(WidgetHandle h, const char* text) {
            if (!factory) return;
            if (auto* btn = factory->get_button(h)) {
                btn->set_text(text);
            }
        }

        void set_light(const rgba& c) {
            if (!factory) return;
            if (auto* box = factory->get_container(handles.trig_light)) {
                box->set_background(c);
            }
        }

        Rect chart_rect() const {
            if (!factory) return {};
            if (auto* waveform = factory->get_waveform(handles.waveform)) {
                return waveform->get_rect();
            }
            return {};
        }

        bool hit_chart(int x, int y) const {
            const Rect r = chart_rect();
            return x >= r.x && y >= r.y && x < (r.x + r.w) && y < (r.y + r.h);
        }

        bool hit_label(WidgetHandle h, int x, int y) const {
            if (!factory) return false;
            if (auto* label = factory->get_label(h)) {
                return label->get_rect().contains(x, y);
            }
            return false;
        }

        void update_trigger_from_y(int y) {
            if (!app) return;
            const Rect r = chart_rect();
            if (r.h < 2) return;
            const int center = r.y + r.h / 2;
            float norm = static_cast<float>(center - y) / static_cast<float>(r.h / 2);
            if (norm > 1.0f) norm = 1.0f;
            if (norm < -1.0f) norm = -1.0f;
            const auto wave = app->wave();
            const float level = norm / app->vertical_scale() + wave.offset;
            app->set_trigger_level(level);
        }

        bool begin_drag(int x, int y, bool adjust_window) {
            if (!hit_chart(x, y)) return false;
            dragging = true;
            dragging_window = adjust_window;
            drag_origin_y = y;
            if (adjust_window && app) {
                drag_window_origin = app->trigger_window();
            }
            if (!adjust_window) {
                update_trigger_from_y(y);
            }
            return true;
        }

        void update_drag(int y) {
            if (!dragging) return;
            if (dragging_window) {
                if (!app) return;
                const Rect r = chart_rect();
                if (r.h < 2) return;
                const float range = app->vertical_scale() * 2.0f;
                const float delta_v = static_cast<float>(drag_origin_y - y) * (range / static_cast<float>(r.h));
                app->set_trigger_window(drag_window_origin + delta_v);
            } else {
                update_trigger_from_y(y);
            }
        }

        void end_drag() {
            dragging = false;
            dragging_window = false;
        }

        void toggle_mode() {
            if (!app) return;
            const auto mode = app->display_mode();
            app->set_display_mode(mode == scope::DisplayMode::Scroll
                                      ? scope::DisplayMode::TriggerHold
                                      : scope::DisplayMode::Scroll);
        }

        void toggle_edge() {
            if (!app) return;
            app->toggle_trigger_edge();
        }

        void adjust_time_scale(int delta) {
            if (!app) return;
            app->adjust_time_scale(delta);
        }

        void adjust_vertical_scale(int delta) {
            if (!app) return;
            app->adjust_vertical_scale(delta);
        }

        void adjust_trigger_window(int delta) {
            if (!app) return;
            const float step = app->volts_per_div(8) * 0.5f;
            app->adjust_trigger_window(static_cast<float>(delta) * step);
        }

        void sync_ui() {
            if (!app || !factory) return;

            const auto display = app->display_mode();
            std::snprintf(mode_text.data(), mode_text.size(),
                          "MODE %s",
                          display == scope::DisplayMode::Scroll ? "ROLL" : "HOLD");

            const auto trigger = app->trigger();
            std::snprintf(trigger_text.data(), trigger_text.size(),
                          "EDGE %s  L=%.2fV",
                          trigger.edge == scope::TriggerEdge::Rising ? "RISE" : "FALL",
                          trigger.level);

            const auto mode = app->trigger_mode();
            const char* trig_mode_name = (mode == scope::TriggerMode::Auto)
                ? "AUTO"
                : (mode == scope::TriggerMode::Normal ? "NORM" : "SGL");
            std::snprintf(trig_mode_text.data(), trig_mode_text.size(),
                          "MODE %s  WIN %.2fV", trig_mode_name, trigger.window);

            const auto state = app->trigger_state();
            if (state == scope::TriggerState::Triggered && last_trigger_state != scope::TriggerState::Triggered) {
                trigger_pulse_frames = 10;
            }
            last_trigger_state = state;
            if (trigger_pulse_frames > 0) {
                --trigger_pulse_frames;
            }
            const char* state_text = (state == scope::TriggerState::Armed)
                ? "ARMED"
                : (state == scope::TriggerState::Triggered ? "TRIG" : "HOLD");
            std::snprintf(trig_state_text.data(), trig_state_text.size(),
                          "STATE %s", state_text);

            if (state == scope::TriggerState::Armed) set_light({120, 200, 170, 255});
            else if (state == scope::TriggerState::Triggered) set_light({250, 210, 120, 255});
            else set_light({220, 120, 120, 255});
            if (trigger_pulse_frames > 0) {
                set_light({255, 240, 170, 255});
            }

            const auto meas = app->measurements();
            if (meas.freq_hz >= 1000.0f) {
                std::snprintf(measure_text.data(), measure_text.size(),
                              "Vpp: %.3f  F: %.2fk",
                              meas.vpp, meas.freq_hz / 1000.0f);
            } else {
                std::snprintf(measure_text.data(), measure_text.size(),
                              "Vpp: %.3f  F: %.1f",
                              meas.vpp, meas.freq_hz);
            }

            format_time_div();
            format_volt_div();

            set_label(handles.mode, mode_text.data());
            set_label(handles.trigger, trigger_text.data());
            set_label(handles.trig_mode, trig_mode_text.data());
            set_label(handles.trig_state, trig_state_text.data());
            set_label(handles.measure, measure_text.data());
            set_label(handles.time_div, time_text.data());
            set_label(handles.volt_div, volt_text.data());
            set_button_text(handles.btn_trig_mode, trig_mode_name);
            set_button_text(handles.btn_roll,
                            app->display_mode() == scope::DisplayMode::Scroll ? "ROLL" : "HOLD");

            if (auto* waveform = factory->get_waveform(handles.waveform)) {
                const auto wave = app->wave();
                const float range = app->vertical_scale();
                waveform->set_range(wave.offset - range, wave.offset + range);
                waveform->set_trigger_level(app->trigger().level);
                waveform->set_trigger_window(app->trigger_window());
                if (trigger_pulse_frames > 0) {
                    waveform->set_trigger_color({255, 180, 200, 255});
                } else {
                    waveform->set_trigger_color({255, 120, 140, 255});
                }
                waveform->set_samples(app->samples());
            }
        }

        void format_time_div() {
            if (!app) return;
            const float value = app->time_per_div(10);
            const char* unit = "s";
            float scaled = value;
            if (scaled < 1e-6f) {
                scaled *= 1e9f;
                unit = "ns";
            } else if (scaled < 1e-3f) {
                scaled *= 1e6f;
                unit = "us";
            } else if (scaled < 1.0f) {
                scaled *= 1e3f;
                unit = "ms";
            }
            std::snprintf(time_text.data(), time_text.size(), "TIME/DIV %.2f %s", scaled, unit);
        }

        void format_volt_div() {
            if (!app) return;
            const float value = app->volts_per_div(8);
            std::snprintf(volt_text.data(), volt_text.size(), "V/DIV %.2f V", value);
        }

        void on_reset_clicked() noexcept {
            if (!app) return;
            app->reset_single();
        }

        void on_time_dec() noexcept {
            adjust_time_scale(-1);
        }
        void on_time_inc() noexcept {
            adjust_time_scale(1);
        }
        void on_volt_dec() noexcept {
            adjust_vertical_scale(-1);
        }
        void on_volt_inc() noexcept {
            adjust_vertical_scale(1);
        }
        void on_win_dec() noexcept {
            adjust_trigger_window(-1);
        }
        void on_win_inc() noexcept {
            adjust_trigger_window(1);
        }
        void on_edge() noexcept {
            toggle_edge();
        }
        void on_trig_mode() noexcept {
            if (!app) return;
            app->cycle_trigger_mode();
        }
        void on_roll() noexcept {
            toggle_mode();
        }
    };

    std::optional<input::Button> map_sdl_button(SDL_Keycode key) noexcept {
        switch (key) {
        case SDLK_UP: return input::Button::Up;
        case SDLK_DOWN: return input::Button::Down;
        case SDLK_RETURN: return input::Button::Enter;
        case SDLK_ESCAPE: return input::Button::Back;
        case SDLK_BACKSPACE: return input::Button::Back;
        default:
            break;
        }
        return std::nullopt;
    }

    void dispatch_raw_event(Gui& gui, const input::RawInputEvent& ev) {
        const auto bridge = input::adapter::bridge_from_raw(ev);
        if (bridge.event) {
            gui.dispatch_event(*bridge.event);
        }
    }

    bool dispatch_sdl_event(Gui& gui, ScopeUiContext& ctx, const SDL_Event& evt) {
        switch (evt.type) {
        case SDL_EVENT_MOUSE_MOTION:
            if (ctx.dragging) {
                ctx.update_drag(evt.motion.y);
                return true;
            }
            {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Pointer;
                raw.ms = SDL_GetTicks();
                raw.pointer = input::PointerRaw{false,
                                               static_cast<std::int16_t>(evt.motion.x),
                                               static_cast<std::int16_t>(evt.motion.y),
                                               0};
                raw.pointer_action = input::PointerAction::Move;
                dispatch_raw_event(gui, raw);
            }
            return true;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (evt.button.button == SDL_BUTTON_LEFT) {
                if (ctx.hit_label(ctx.handles.trig_mode, evt.button.x, evt.button.y)) {
                    if (ctx.app) ctx.app->cycle_trigger_mode();
                    return true;
                }
                const SDL_Keymod mods = SDL_GetModState();
                const bool adjust_window = (mods & SDL_KMOD_ALT) != 0;
                if (ctx.begin_drag(evt.button.x, evt.button.y, adjust_window)) return true;
            }
            if (evt.button.button == SDL_BUTTON_RIGHT) {
                if (ctx.hit_chart(evt.button.x, evt.button.y)) {
                    ctx.toggle_mode();
                    return true;
                }
            }
            {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Pointer;
                raw.ms = SDL_GetTicks();
                raw.pointer = input::PointerRaw{true,
                                               static_cast<std::int16_t>(evt.button.x),
                                               static_cast<std::int16_t>(evt.button.y),
                                               0};
                raw.pointer_action = input::PointerAction::Down;
                dispatch_raw_event(gui, raw);
            }
            return true;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (evt.button.button == SDL_BUTTON_LEFT && ctx.dragging) {
                ctx.end_drag();
                return true;
            }
            {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Pointer;
                raw.ms = SDL_GetTicks();
                raw.pointer = input::PointerRaw{false,
                                               static_cast<std::int16_t>(evt.button.x),
                                               static_cast<std::int16_t>(evt.button.y),
                                               0};
                raw.pointer_action = input::PointerAction::Up;
                dispatch_raw_event(gui, raw);
            }
            return true;
        case SDL_EVENT_MOUSE_WHEEL: {
            const SDL_Keymod mods = SDL_GetModState();
            const int step = (evt.wheel.y > 0) ? 1 : (evt.wheel.y < 0 ? -1 : 0);
            if (step == 0) {
                gui.dispatch_event(Event::wheel(evt.wheel.x, evt.wheel.y, evt.wheel.y));
                return true;
            }
            if ((mods & SDL_KMOD_ALT) != 0) {
                ctx.adjust_trigger_window(step);
            } else if ((mods & SDL_KMOD_SHIFT) != 0) {
                ctx.adjust_vertical_scale(step);
            } else {
                ctx.adjust_time_scale(step);
            }
            gui.dispatch_event(Event::wheel(evt.wheel.x, evt.wheel.y, evt.wheel.y));
            return true;
        }
        case SDL_EVENT_KEY_DOWN:
            if (evt.key.key == SDLK_M) {
                ctx.toggle_mode();
                return true;
            }
            if (evt.key.key == SDLK_E) {
                ctx.toggle_edge();
                return true;
            }
            if (evt.key.key == SDLK_T) {
                if (ctx.app) ctx.app->cycle_trigger_mode();
                return true;
            }
            if (auto b = map_sdl_button(evt.key.key)) {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Button;
                raw.ms = SDL_GetTicks();
                raw.button = *b;
                raw.pressed = true;
                dispatch_raw_event(gui, raw);
            }
            return true;
        case SDL_EVENT_KEY_UP:
            if (auto b = map_sdl_button(evt.key.key)) {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Button;
                raw.ms = SDL_GetTicks();
                raw.button = *b;
                raw.pressed = false;
                dispatch_raw_event(gui, raw);
            }
            return true;
        default:
            return false;
        }
    }

    UiHandles build_ui(UiFactory& factory, ScopeUiContext& ctx) {
        auto anchor_pos = [](auto* obj, int x, int y) {
            if (!obj) return;
            obj->set_pos(x, y);
            obj->set_anchor(x, y, -1, -1);
        };
        auto anchor_rect = [](auto* obj, const Rect& r) {
            if (!obj) return;
            obj->set_rect(r);
            obj->set_anchor(r.x, r.y, -1, -1);
        };

        UiHandles h{};
        h.root = factory.create_container();
        if (auto* root = factory.get_container(h.root)) {
            root->set_rect({0, 0, screen_width, screen_height});
            root->set_background({6, 10, 18, 255});
        }

        h.top_left = factory.create_container();
        if (auto* bar = factory.get_container(h.top_left)) {
            anchor_rect(bar, {kPadding, kPadding, 170, kTopBar - 4});
            bar->set_background({14, 20, 34, 255});
        }

        h.top_mid = factory.create_container();
        if (auto* bar = factory.get_container(h.top_mid)) {
            anchor_rect(bar, {kPadding + 180, kPadding, 240, kTopBar - 4});
            bar->set_background({14, 20, 34, 255});
        }

        h.top_right = factory.create_container();
        if (auto* bar = factory.get_container(h.top_right)) {
            const int right_w = screen_width - (kPadding * 2 + 430);
            anchor_rect(bar, {kPadding + 430, kPadding, right_w, kTopBar - 4});
            bar->set_background({14, 20, 34, 255});
        }

        h.waveform = factory.create_waveform();
        if (auto* waveform = factory.get_waveform(h.waveform)) {
            const int chart_x = kPadding;
            const int chart_y = kPadding + kTopBar;
            const int chart_w = screen_width - kPadding * 2;
            const int chart_h = screen_height - (kPadding * 2 + kTopBar + kBottomBar);
            anchor_rect(waveform, {chart_x, chart_y, chart_w, chart_h});
            waveform->set_grid_div(10, 8);
            waveform->set_grid_color({22, 38, 54, 255});
            waveform->set_trace_color({90, 220, 255, 255});
            waveform->set_glow(true);
            waveform->set_glow_color({90, 220, 255, 70});
            waveform->set_trigger_color({255, 120, 140, 255});
            waveform->set_trigger_fill(true);
            waveform->set_trigger_fill_color({255, 120, 140, 26});
            waveform->set_center_color({40, 80, 110, 255});
        }

        h.title = factory.create_label("Scope / Sim");
        if (auto* title = factory.get_label(h.title)) {
            title->set_color({226, 240, 255, 255});
            anchor_pos(title, kPadding + 10, kPadding + 10);
        }

        h.time_div = factory.create_label("TIME/DIV 10.00 us");
        if (auto* time_div = factory.get_label(h.time_div)) {
            time_div->set_color({150, 180, 210, 255});
            anchor_pos(time_div, kPadding + 190, kPadding + 10);
        }

        h.volt_div = factory.create_label("V/DIV 0.50 V");
        if (auto* volt_div = factory.get_label(h.volt_div)) {
            volt_div->set_color({150, 180, 210, 255});
            anchor_pos(volt_div, kPadding + 190, kPadding + 24);
        }

        h.trigger = factory.create_label("EDGE RISE  L=0.00V");
        if (auto* trigger = factory.get_label(h.trigger)) {
            trigger->set_color({150, 180, 210, 255});
            anchor_pos(trigger, kPadding + 440, kPadding + 10);
        }

        h.mode = factory.create_label("MODE ROLL");
        if (auto* mode = factory.get_label(h.mode)) {
            mode->set_color({150, 180, 210, 255});
            anchor_pos(mode, kPadding + 440, kPadding + 24);
        }

        h.trig_mode = factory.create_label("MODE AUTO  WIN 0.10V");
        if (auto* trig_mode = factory.get_label(h.trig_mode)) {
            trig_mode->set_color({150, 180, 210, 255});
            anchor_pos(trig_mode, kPadding + 610, kPadding + 10);
        }

        h.trig_state = factory.create_label("STATE ARMED");
        if (auto* trig_state = factory.get_label(h.trig_state)) {
            trig_state->set_color({150, 180, 210, 255});
            anchor_pos(trig_state, kPadding + 610, kPadding + 24);
        }

        h.trig_light = factory.create_container();
        if (auto* light = factory.get_container(h.trig_light)) {
            anchor_rect(light, {kPadding + 578, kPadding + 14, 12, 12});
            light->set_background({120, 200, 170, 255});
        }

        h.trig_reset = factory.create_button("ARM");
        if (auto* reset = factory.get_button(h.trig_reset)) {
            reset->set_size(48, 20);
            reset->set_on_click(Callback::bind<&ScopeUiContext::on_reset_clicked>(ctx));
            reset->set_pos(kPadding + 578, kPadding + 32);
        }

        h.measure = factory.create_label("Vpp: 0.00  F: 0.0");
        if (auto* measure = factory.get_label(h.measure)) {
            measure->set_color({200, 228, 255, 255});
            anchor_pos(measure, kPadding, screen_height - kBottomBar + 6);
        }

        h.hint = factory.create_label("LMB drag=Trig  Alt+Drag=Win  RMB=Mode  Wheel=Time  Shift+Wheel=V  Alt+Wheel=Win  T=TrigMode  E=Edge");
        if (auto* hint = factory.get_label(h.hint)) {
            hint->set_color({110, 140, 170, 255});
            anchor_pos(hint, kPadding, screen_height - kBottomBar + 46);
        }

        h.controls = factory.create_container();
        if (auto* controls = factory.get_container(h.controls)) {
            const int w = 320;
            const int hgt = 60;
            const int x = screen_width - kPadding - w;
            const int y = screen_height - kBottomBar + 6;
            anchor_rect(controls, {x, y, w, hgt});
            controls->set_grid_layout(5, 56, 24, 6, 6);
        }

        h.btn_time_dec = factory.create_button("T-");
        if (auto* btn = factory.get_button(h.btn_time_dec)) {
            btn->set_size(56, 24);
            btn->set_on_click(Callback::bind<&ScopeUiContext::on_time_dec>(ctx));
        }
        h.btn_time_inc = factory.create_button("T+");
        if (auto* btn = factory.get_button(h.btn_time_inc)) {
            btn->set_size(56, 24);
            btn->set_on_click(Callback::bind<&ScopeUiContext::on_time_inc>(ctx));
        }
        h.btn_volt_dec = factory.create_button("V-");
        if (auto* btn = factory.get_button(h.btn_volt_dec)) {
            btn->set_size(56, 24);
            btn->set_on_click(Callback::bind<&ScopeUiContext::on_volt_dec>(ctx));
        }
        h.btn_volt_inc = factory.create_button("V+");
        if (auto* btn = factory.get_button(h.btn_volt_inc)) {
            btn->set_size(56, 24);
            btn->set_on_click(Callback::bind<&ScopeUiContext::on_volt_inc>(ctx));
        }
        h.btn_win_dec = factory.create_button("W-");
        if (auto* btn = factory.get_button(h.btn_win_dec)) {
            btn->set_size(56, 24);
            btn->set_on_click(Callback::bind<&ScopeUiContext::on_win_dec>(ctx));
        }
        h.btn_win_inc = factory.create_button("W+");
        if (auto* btn = factory.get_button(h.btn_win_inc)) {
            btn->set_size(56, 24);
            btn->set_on_click(Callback::bind<&ScopeUiContext::on_win_inc>(ctx));
        }
        h.btn_edge = factory.create_button("EDGE");
        if (auto* btn = factory.get_button(h.btn_edge)) {
            btn->set_size(56, 24);
            btn->set_on_click(Callback::bind<&ScopeUiContext::on_edge>(ctx));
        }
        h.btn_trig_mode = factory.create_button("AUTO");
        if (auto* btn = factory.get_button(h.btn_trig_mode)) {
            btn->set_size(56, 24);
            btn->set_on_click(Callback::bind<&ScopeUiContext::on_trig_mode>(ctx));
        }
        h.btn_roll = factory.create_button("ROLL");
        if (auto* btn = factory.get_button(h.btn_roll)) {
            btn->set_size(56, 24);
            btn->set_on_click(Callback::bind<&ScopeUiContext::on_roll>(ctx));
        }

        factory.link(h.root, h.top_left);
        factory.link(h.root, h.top_mid);
        factory.link(h.root, h.top_right);
        factory.link(h.root, h.waveform);
        factory.link(h.root, h.title);
        factory.link(h.root, h.time_div);
        factory.link(h.root, h.volt_div);
        factory.link(h.root, h.trigger);
        factory.link(h.root, h.mode);
        factory.link(h.root, h.trig_mode);
        factory.link(h.root, h.trig_state);
        factory.link(h.root, h.trig_light);
        factory.link(h.root, h.trig_reset);
        factory.link(h.root, h.measure);
        factory.link(h.root, h.controls);
        factory.link(h.root, h.hint);

        factory.link(h.controls, h.btn_time_dec);
        factory.link(h.controls, h.btn_time_inc);
        factory.link(h.controls, h.btn_volt_dec);
        factory.link(h.controls, h.btn_volt_inc);
        factory.link(h.controls, h.btn_win_dec);
        factory.link(h.controls, h.btn_win_inc);
        factory.link(h.controls, h.btn_edge);
        factory.link(h.controls, h.btn_trig_mode);
        factory.link(h.controls, h.btn_roll);

        return h;
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Charm Scope", screen_width, screen_height, 0);
    if (!window) {
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                             screen_width, screen_height);
    if (!texture) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    static DefaultFrameBuffer fb;
    static DefaultCanvas canvas(fb);
    static UiFactory factory;
    scope::AppConfig config{};
    config.sample_rate_hz = 1000000.0f;
    config.frame_samples = 1024;
    config.wave.amplitude = 1.0f;
    config.wave.offset = 0.0f;
    config.wave.frequency_hz = 100000.0f;
    config.wave.noise = 0.02f;
    config.trigger.level = 0.0f;
    config.trigger.edge = scope::TriggerEdge::Rising;
    config.trigger.window = 0.1f;
    config.trigger.mode = scope::TriggerMode::Auto;
    config.trigger.auto_timeout_ms = 500.0f;
    config.display_mode = scope::DisplayMode::Scroll;

    scope::App app(config);

    auto& theme = Theme::instance();
    ThemePreset preset{};
    preset.has_label = true;
    preset.label = theme.get<Label>();
    preset.label.colors.font_color = {220, 224, 240, 255};
    apply_theme_preset(preset);

    StylePatch waveform_patch{};
    waveform_patch.has_bg_color = true;
    waveform_patch.bg_color = {10, 16, 28, 255};
    waveform_patch.has_border_color = true;
    waveform_patch.border_color = {40, 70, 100, 255};
    waveform_patch.has_font_color = true;
    waveform_patch.font_color = {90, 220, 255, 255};
    waveform_patch.has_padding = true;
    waveform_patch.padding = 6;
    theme.patch<Waveform>(waveform_patch);

    ScopeUiContext ctx{};
    ctx.app = &app;
    ctx.factory = &factory;
    ctx.handles = build_ui(factory, ctx);

    static Gui gui(canvas, factory, ctx.handles.root);
    gui.set_dirty_tracking(true);

    bool running = true;
    while (running) {
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
            dispatch_sdl_event(gui, ctx, evt);
        }

        app.tick();
        ctx.sync_ui();

        canvas.clear({12, 14, 20, 255});
        gui.render();

        SDL_UpdateTexture(texture, nullptr, fb.data(), screen_width * 3);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
