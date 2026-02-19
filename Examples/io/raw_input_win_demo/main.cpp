#include <cstdint>
#include <optional>
#include <thread>
#include <chrono>

import hal_input;
import hal_win;
import input.sampler;
import input.raw_event;
import out.print;

namespace {
    struct ScriptedInput {
        std::uint32_t now_ms{0};
        int last_phase{-1};
    };

    bool is_down(void* ctx, input::Button b) noexcept {
        auto* s = static_cast<ScriptedInput*>(ctx);
        if (!s) return false;
        if (b == input::Button::Enter) return ((s->now_ms / 600) % 2) != 0;
        if (b == input::Button::Up) return ((s->now_ms / 1200) % 2) != 0;
        return false;
    }

    input::PointerRaw read_pointer(void* ctx) noexcept {
        auto* s = static_cast<ScriptedInput*>(ctx);
        if (!s) return input::PointerRaw{};
        const bool down = ((s->now_ms / 500) % 2) != 0;
        if (!down) return input::PointerRaw{.down = false, .x = -1, .y = -1, .id = 0};
        const auto x = static_cast<std::int16_t>((s->now_ms / 10) % 120);
        const auto y = static_cast<std::int16_t>((s->now_ms / 15) % 80);
        return input::PointerRaw{.down = true, .x = x, .y = y, .id = 0};
    }

    input::AxisRaw read_axis(void*) noexcept {
        return input::AxisRaw{};
    }

    std::optional<std::uint8_t> pop_encoder_ab(void* ctx) noexcept {
        auto* s = static_cast<ScriptedInput*>(ctx);
        if (!s) return std::nullopt;
        const int phase = static_cast<int>((s->now_ms / 100) % 4);
        if (phase == s->last_phase) return std::nullopt;
        s->last_phase = phase;
        return static_cast<std::uint8_t>(phase);
    }

    const char* event_name(input::RawInputEventType t) noexcept {
        switch (t) {
        case input::RawInputEventType::Button: return "button";
        case input::RawInputEventType::Pointer: return "pointer";
        case input::RawInputEventType::Axis: return "axis";
        case input::RawInputEventType::Encoder: return "encoder";
        default: return "none";
        }
    }
}

int main() {
    ScriptedInput state{};
    hal::RawInputDriver drv{
        .ctx = &state,
        .is_down = &is_down,
        .read_pointer = &read_pointer,
        .read_axis = &read_axis,
        .pop_encoder_ab = &pop_encoder_ab
    };
    hal::RawInputSource source{drv};
    input::RawSampler sampler{};

    (void)out::println<"[raw_input_demo] start">();
    const auto start = hal::win::Time::now();
    while (hal::win::Time::now() - start < 2000) {
        state.now_ms = static_cast<std::uint32_t>(hal::win::Time::now());
        if (auto ev = sampler.poll(source, state.now_ms)) {
            (void)out::println<"[raw] t={} type={}">(
                ev->ms,
                event_name(ev->type));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    (void)out::println<"[raw_input_demo] done">();
    return 0;
}
