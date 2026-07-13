import input.raw_event;
import player.port;
import player.port_runtime;
import player.raster;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace {
    struct FakeClock {
        player::PlayerClockTick now_us{0};

        static player::PlayerClockTick read_us(void* ctx) noexcept {
            return static_cast<FakeClock*>(ctx)->now_us;
        }
    };

    struct FakeInput {
        std::array<input::RawInputEvent, 2> events{
            input::RawInputEvent{.type = input::RawInputEventType::Pointer, .ms = 1},
            input::RawInputEvent{.type = input::RawInputEventType::Button, .ms = 2},
        };
        std::size_t next{0};

        static bool poll(void* ctx, input::RawInputEvent& out) noexcept {
            auto& self = *static_cast<FakeInput*>(ctx);
            if (self.next >= self.events.size()) {
                return false;
            }
            out = self.events[self.next++];
            return true;
        }
    };

    struct FakePlayer {
        int bootstrap_count{0};
        int input_count{0};
        int update_count{0};
        int render_count{0};
        int shutdown_count{0};
        player::PlayerClockTick last_now_us{0};
        player::PlayerClockTick last_dt_us{0};
        bool bootstrap_result{true};
        bool render_result{true};

        static bool bootstrap(void* ctx, const player::PlayerPort& port) noexcept {
            auto& self = *static_cast<FakePlayer*>(ctx);
            ++self.bootstrap_count;
            return port.valid() && self.bootstrap_result;
        }

        static void dispatch(void* ctx, const input::RawInputEvent&) noexcept {
            ++static_cast<FakePlayer*>(ctx)->input_count;
        }

        static void update(void* ctx,
                           player::PlayerClockTick now_us,
                           player::PlayerClockTick dt_us) noexcept {
            auto& self = *static_cast<FakePlayer*>(ctx);
            ++self.update_count;
            self.last_now_us = now_us;
            self.last_dt_us = dt_us;
        }

        static bool render(void* ctx,
                           const player::PlayerRasterSurface& surface,
                           const player::PlayerRasterDisplay& display) noexcept {
            ++static_cast<FakePlayer*>(ctx)->render_count;
            auto& self = *static_cast<FakePlayer*>(ctx);
            return self.render_result
                && display.present(surface, player::full_player_raster_region(surface));
        }

        static void shutdown(void* ctx) noexcept {
            ++static_cast<FakePlayer*>(ctx)->shutdown_count;
        }
    };

    bool expect(bool condition, const char* message) {
        if (!condition) {
            std::printf("[player-port-runtime-smoke] fail: %s\n", message);
        }
        return condition;
    }
}

int main() {
    constexpr int width = 8;
    constexpr int height = 4;
    constexpr std::size_t stride = width * 3;
    std::array<std::byte, stride * height> pixels{};

    FakeClock fake_clock{};
    FakeInput input_source{};
    player::PlayerMemoryRasterDisplayState display_state{};
    FakePlayer fake_player{};

    const player::PlayerRasterSurface surface{
        pixels.data(),
        width,
        height,
        stride,
        player::PlayerRasterPixelFormat::RGB888,
    };
    const player::PlayerPort port{
        .clock = {&fake_clock, &FakeClock::read_us},
        .raster_surface = surface,
        .raster_display = player::make_player_memory_raster_display(display_state),
        .raw_input = {&input_source, &FakeInput::poll},
    };
    const player::PlayerRuntimeEndpoint endpoint{
        .ctx = &fake_player,
        .bootstrap_fn = &FakePlayer::bootstrap,
        .dispatch_raw_input_fn = &FakePlayer::dispatch,
        .update_fn = &FakePlayer::update,
        .render_fn = &FakePlayer::render,
        .shutdown_fn = &FakePlayer::shutdown,
    };

    player::PlayerPortRuntime runtime{port, endpoint};
    if (!expect(runtime.bootstrap(), "bootstrap")
        || !expect(runtime.state() == player::PlayerPortRuntimeState::Running, "running state")
        || !expect(runtime.frame(1), "first frame")
        || !expect(runtime.dispatched_input_count() == 1, "input budget")
        || !expect(fake_player.last_now_us == 0 && fake_player.last_dt_us == 0,
                   "zero-origin first timestamp")) {
        return 1;
    }

    fake_clock.now_us = 16000;
    if (!expect(runtime.frame(), "second frame")
        || !expect(runtime.dispatched_input_count() == 2, "remaining input")
        || !expect(fake_player.last_now_us == 16000 && fake_player.last_dt_us == 16000,
                   "zero-origin frame delta")
        || !expect(display_state.present_count == 2, "raster presents")) {
        return 1;
    }

    runtime.shutdown();
    runtime.shutdown();
    if (!expect(runtime.state() == player::PlayerPortRuntimeState::Stopped, "stopped state")
        || !expect(fake_player.bootstrap_count == 1, "single bootstrap")
        || !expect(fake_player.update_count == 2 && fake_player.render_count == 2, "frame lifecycle")
        || !expect(fake_player.shutdown_count == 1, "idempotent shutdown")
        || !expect(!runtime.frame(), "frame rejected after shutdown")) {
        return 1;
    }

    FakePlayer invalid_player{};
    auto invalid_endpoint = endpoint;
    invalid_endpoint.ctx = &invalid_player;
    auto invalid_port = port;
    invalid_port.clock = {};
    player::PlayerPortRuntime invalid_runtime{invalid_port, invalid_endpoint};
    if (!expect(!invalid_runtime.bootstrap(), "invalid port rejected")
        || !expect(invalid_runtime.state() == player::PlayerPortRuntimeState::Failed,
                   "invalid port failed state")) {
        return 1;
    }
    invalid_runtime.shutdown();
    if (!expect(invalid_player.bootstrap_count == 0, "invalid port skips endpoint bootstrap")
        || !expect(invalid_player.shutdown_count == 0, "invalid port skips endpoint shutdown")) {
        return 1;
    }

    FakePlayer bootstrap_failure_player{.bootstrap_result = false};
    auto bootstrap_failure_endpoint = endpoint;
    bootstrap_failure_endpoint.ctx = &bootstrap_failure_player;
    player::PlayerPortRuntime bootstrap_failure_runtime{port, bootstrap_failure_endpoint};
    if (!expect(!bootstrap_failure_runtime.bootstrap(), "endpoint bootstrap failure")
        || !expect(bootstrap_failure_runtime.state() == player::PlayerPortRuntimeState::Failed,
                   "bootstrap failure state")) {
        return 1;
    }
    bootstrap_failure_runtime.shutdown();
    bootstrap_failure_runtime.shutdown();
    if (!expect(bootstrap_failure_player.bootstrap_count == 1, "failed bootstrap attempted once")
        || !expect(bootstrap_failure_player.shutdown_count == 1,
                   "failed bootstrap shutdown once")) {
        return 1;
    }

    FakePlayer render_failure_player{.render_result = false};
    auto render_failure_endpoint = endpoint;
    render_failure_endpoint.ctx = &render_failure_player;
    player::PlayerPortRuntime render_failure_runtime{port, render_failure_endpoint};
    if (!expect(render_failure_runtime.bootstrap(), "render failure bootstrap")
        || !expect(!render_failure_runtime.frame(2000, 1000), "render failure rejected")
        || !expect(render_failure_runtime.state() == player::PlayerPortRuntimeState::Failed,
                   "render failure state")
        || !expect(!render_failure_runtime.frame(3000, 1000),
                   "frame rejected after render failure")) {
        return 1;
    }
    render_failure_runtime.shutdown();
    render_failure_runtime.shutdown();
    if (!expect(render_failure_player.update_count == 1, "render failure update once")
        || !expect(render_failure_player.render_count == 1, "render failure render once")
        || !expect(render_failure_player.shutdown_count == 1, "render failure shutdown once")) {
        return 1;
    }

    player::PlayerMemoryRasterDisplayState clipped_display_state{};
    const auto clipped_display = player::make_player_memory_raster_display(clipped_display_state);
    if (!expect(clipped_display.present(surface, {-2, -1, 5, 3}), "clipped present")
        || !expect(clipped_display_state.present_count == 1, "clipped present callback")
        || !expect(clipped_display_state.last_dirty.x == 0
                       && clipped_display_state.last_dirty.y == 0
                       && clipped_display_state.last_dirty.w == 3
                       && clipped_display_state.last_dirty.h == 2,
                   "half-open dirty region")
        || !expect(clipped_display.present(
                       surface,
                       {std::numeric_limits<int>::max(), 0, 8, 1}),
                   "overflow-safe empty present")
        || !expect(clipped_display_state.present_count == 1,
                   "empty dirty region skips callback")) {
        return 1;
    }

    FakeClock regressing_clock{.now_us = 100};
    FakePlayer regressing_player{};
    auto regressing_port = port;
    regressing_port.clock = {&regressing_clock, &FakeClock::read_us};
    auto regressing_endpoint = endpoint;
    regressing_endpoint.ctx = &regressing_player;
    player::PlayerPortRuntime regressing_runtime{regressing_port, regressing_endpoint};
    if (!expect(regressing_runtime.bootstrap(), "regressing clock bootstrap")
        || !expect(regressing_runtime.frame(), "regressing clock first frame")) {
        return 1;
    }
    regressing_clock.now_us = 99;
    if (!expect(!regressing_runtime.frame(), "regressing clock rejected")
        || !expect(regressing_runtime.state() == player::PlayerPortRuntimeState::Failed,
                   "regressing clock failed state")
        || !expect(regressing_player.update_count == 1,
                   "regressing clock rejected before update")) {
        return 1;
    }
    regressing_runtime.shutdown();

    FakePlayer split_phase_player{};
    auto split_phase_endpoint = endpoint;
    split_phase_endpoint.ctx = &split_phase_player;
    player::PlayerPortRuntime split_phase_runtime{port, split_phase_endpoint};
    if (!expect(split_phase_runtime.bootstrap(), "split phase bootstrap")
        || !expect(split_phase_runtime.update_frame(5000, 0), "split phase update")
        || !expect(split_phase_player.update_count == 1
                       && split_phase_player.render_count == 0,
                   "update phase excludes render")
        || !expect(!split_phase_runtime.update_frame(6000, 1000),
                   "second update rejected before render")
        || !expect(split_phase_runtime.render_frame(), "split phase render")
        || !expect(split_phase_player.render_count == 1, "render phase isolated")
        || !expect(!split_phase_runtime.render_frame(), "duplicate render rejected")) {
        return 1;
    }
    split_phase_runtime.shutdown();

    std::printf("[player-port-runtime-smoke] ok\n");
    return 0;
}
