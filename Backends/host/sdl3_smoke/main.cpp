#include "Backends/contract/raster_display.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <thread>

import charm.backend.host.sdl3;
import charm.system.clock;
import charm.system.run_loop;
import input.raw_event;
import input.raw_sink;

namespace host_sdl3 = charm::backend::host::sdl3;
namespace raster = charm::backend::contract::raster;

bool queue_sdl_input_sequence() noexcept;
bool queue_sdl_window_close_event() noexcept;
bool create_sdl_foreign_window() noexcept;
void destroy_sdl_foreign_window() noexcept;

namespace {
    struct InputCollector {
        std::array<input::RawInputEvent, 12> events{};
        std::size_t count{0};

        bool on_raw(const input::RawInputEvent& event) noexcept {
            if (count >= events.size()) {
                return false;
            }
            events[count++] = event;
            return true;
        }
    };

    struct LoopState {
        host_sdl3::Host* host{nullptr};
        input::RawSinkRef sink{};
        raster::SurfaceView surface{};
        raster::DirtyRegion render_dirty{};
        std::array<char, 4> phase_order{};
        std::size_t phase_count{0};
        host_sdl3::PumpResult pump{};
        raster::PresentResult present{};

        void record(const char phase) noexcept {
            if (phase_count < phase_order.size()) {
                phase_order[phase_count++] = phase;
            }
        }
    };

    void run_io(void* ctx,
                charm::system::ClockTick,
                charm::system::ClockTick) noexcept {
        auto* state = static_cast<LoopState*>(ctx);
        state->record('I');
        state->pump = state->host->pump_events(state->sink);
    }

    void run_update(void* ctx,
                    charm::system::ClockTick,
                    charm::system::ClockTick) noexcept {
        static_cast<LoopState*>(ctx)->record('U');
    }

    void run_render(void* ctx,
                    charm::system::ClockTick,
                    charm::system::ClockTick) noexcept {
        auto* state = static_cast<LoopState*>(ctx);
        state->record('R');
        state->present = state->host->present(state->surface, state->render_dirty);
    }

    void run_idle(void* ctx,
                  charm::system::ClockTick,
                  charm::system::ClockTick) noexcept {
        static_cast<LoopState*>(ctx)->record('D');
    }

    bool expect(const bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_smoke() {
        host_sdl3::Host host{};
        const auto open = host.open(host_sdl3::Config{
            .title = "Charm Host SDL3 smoke",
            .window_width = 64,
            .window_height = 48,
            .hidden = true,
            .resizable = false,
        });
        if (!open.is_ok()) {
            std::fprintf(stderr, "[ERR] SDL host open failed: %s\n", host.last_error());
            return false;
        }
        host_sdl3::Host second_host{};
        const auto second_open = second_host.open(host_sdl3::Config{
            .title = "Charm Host SDL3 second instance",
            .window_width = 32,
            .window_height = 24,
            .hidden = true,
            .resizable = false,
        });

        std::array<std::uint32_t, 8U * 6U> pixels{};
        pixels.fill(0xFF000000U);
        const raster::SurfaceView surface{
            .pixels = std::as_bytes(std::span{pixels}),
            .width = 8,
            .height = 6,
            .stride_bytes = 8U * 4U,
            .pixel_format = raster::PixelFormat::argb8888,
        };
        InputCollector collector{};
        LoopState state{
            .host = &host,
            .sink = input::RawSinkRef::bind(collector),
            .surface = surface,
            .render_dirty = {2, 1, 3, 2},
        };
        const auto initial_present = host.present(surface, {1, 1, 2, 2});
        pixels[1U * 8U + 2U] = 0xFFFF0000U;
        pixels[0] = 0xFF00FF00U;

        const bool foreign_created = create_sdl_foreign_window();
        const auto foreign_pump = host.pump_events(state.sink);
        destroy_sdl_foreign_window();

        charm::system::RunLoop<4> loop{};
        loop.bind_clock(host.clock());
        const bool added = loop.add_step(charm::system::LoopPhase::io,
                                         charm::system::SubmitProjection::event,
                                         &run_io,
                                         &state,
                                         "host_sdl3_io")
            && loop.add_step(charm::system::LoopPhase::update,
                             charm::system::SubmitProjection::event,
                             &run_update,
                             &state,
                             "host_sdl3_update")
            && loop.add_step(charm::system::LoopPhase::render,
                             charm::system::SubmitProjection::frame,
                             &run_render,
                             &state,
                             "host_sdl3_render")
            && loop.add_step(charm::system::LoopPhase::idle,
                             charm::system::SubmitProjection::demand,
                             &run_idle,
                             &state,
                             "host_sdl3_idle");

        const auto before = host.clock().now_us();
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
        const auto after = host.clock().now_us();
        const bool queued = queue_sdl_input_sequence();
        loop.run_once();

        std::printf("[host-sdl3] pump polled=%zu delivered=%zu rejected=%zu quit=%d "
                    "ignored=%zu pointer=(%d,%d) frames=%llu\n",
                    state.pump.polled,
                    state.pump.delivered,
                    state.pump.rejected,
                    state.pump.quit_requested ? 1 : 0,
                    state.pump.ignored,
                    collector.events[0].pointer.x,
                    collector.events[0].pointer.y,
                    static_cast<unsigned long long>(host.presented_frames()));

        bool ok = true;
        ok &= expect(added, "run loop should accept the four host phases");
        ok &= expect(second_open.code == host_sdl3::StatusCode::already_open,
                     "only one Host instance may own the SDL event queue");
        ok &= expect(foreign_created
                         && foreign_pump.code == host_sdl3::PumpStatusCode::foreign_window
                         && foreign_pump.polled == 0U,
                     "foreign SDL window should stop the pump before it consumes the global queue");
        ok &= expect(after > before, "host clock should be monotonic and advance");
        ok &= expect(initial_present.is_ok()
                         && initial_present.frame_index == 1U
                         && initial_present.submitted.width == 8
                         && initial_present.submitted.height == 6,
                     "initial raster frame should upload the full new texture and establish input extent");
        ok &= expect(queued, "SDL events should enter the single event queue");
        ok &= expect(state.phase_order == std::array<char, 4>{'I', 'U', 'R', 'D'},
                     "run loop should execute io/update/render/idle in order");
        ok &= expect(state.pump.is_ok()
                         && state.pump.polled >= 7U
                         && state.pump.delivered == 6U
                         && state.pump.rejected == 0U
                         && state.pump.quit_requested,
                     "single SDL pump should drain and classify the queued events");
        ok &= expect(collector.count == 6U, "raw input sink should receive six translated events");
        ok &= expect(collector.events[0].type == input::RawInputEventType::Pointer
                         && collector.events[0].pointer_action == input::PointerAction::Down
                         && collector.events[0].pointer.down
                         && collector.events[0].pointer.x == 4
                         && collector.events[0].pointer.y == 3,
                     "pointer down should map from window to raster coordinates");
        ok &= expect(collector.events[1].type == input::RawInputEventType::Pointer
                         && collector.events[1].pointer_action == input::PointerAction::Move
                         && collector.events[1].pointer.down,
                     "pointer motion should retain pressed state");
        ok &= expect(collector.events[2].type == input::RawInputEventType::Button
                         && collector.events[2].button == input::Button::Enter
                         && collector.events[2].pressed,
                     "SDL Enter key down should become raw Enter button down");
        ok &= expect(collector.events[3].type == input::RawInputEventType::Encoder
                         && collector.events[3].encoder_delta == -2,
                     "SDL wheel should become raw encoder delta");
        ok &= expect(collector.events[4].type == input::RawInputEventType::Button
                         && !collector.events[4].pressed,
                     "SDL Enter key up should become raw Enter button up");
        ok &= expect(collector.events[5].type == input::RawInputEventType::Pointer
                         && collector.events[5].pointer_action == input::PointerAction::Up
                         && !collector.events[5].pointer.down,
                     "pointer up should clear pressed state");
        ok &= expect(state.present.is_ok()
                         && state.present.frame_index == 2U
                         && state.present.submitted.x == 2
                         && state.present.submitted.y == 1
                         && state.present.submitted.width == 3
                         && state.present.submitted.height == 2
                         && host.presented_frames() == 2U,
                     "render phase should execute an existing-texture partial update");

        host_sdl3::TestRgba8 dirty_pixel{};
        host_sdl3::TestRgba8 outside_pixel{};
        const bool dirty_read = host.read_test_output_pixel(20, 12, dirty_pixel);
        const bool outside_read = host.read_test_output_pixel(4, 4, outside_pixel);
        std::printf("[host-sdl3] readback dirty=%d rgba=(%u,%u,%u,%u) "
                    "outside=%d rgba=(%u,%u,%u,%u)\n",
                    dirty_read ? 1 : 0,
                    dirty_pixel.red,
                    dirty_pixel.green,
                    dirty_pixel.blue,
                    dirty_pixel.alpha,
                    outside_read ? 1 : 0,
                    outside_pixel.red,
                    outside_pixel.green,
                    outside_pixel.blue,
                    outside_pixel.alpha);
        ok &= expect(dirty_read
                         && dirty_pixel.red == 255U
                         && dirty_pixel.green == 0U
                         && dirty_pixel.blue == 0U
                         && dirty_pixel.alpha == 255U,
                     "partial upload should update the requested source pixel to red");
        ok &= expect(outside_read
                         && outside_pixel.red == 0U
                         && outside_pixel.green == 0U
                         && outside_pixel.blue == 0U
                         && outside_pixel.alpha == 255U,
                     "partial upload should preserve a non-dirty black source pixel");

        constexpr std::size_t rgb565_stride_pixels = 10U;
        std::array<std::uint16_t, rgb565_stride_pixels * 6U> rgb565_pixels{};
        rgb565_pixels[1U * rgb565_stride_pixels + 2U] = 0xF800U;
        const raster::SurfaceView rgb565_surface{
            .pixels = std::as_bytes(std::span{rgb565_pixels}),
            .width = 8,
            .height = 6,
            .stride_bytes = rgb565_stride_pixels * 2U,
            .pixel_format = raster::PixelFormat::rgb565,
        };
        const auto format_change = host.present(rgb565_surface, {1, 1, 2, 2});
        host_sdl3::TestRgba8 rgb565_pixel{};
        const bool rgb565_read = host.read_test_output_pixel(20, 12, rgb565_pixel);
        ok &= expect(format_change.is_ok()
                         && format_change.frame_index == 3U
                         && format_change.submitted.width == 8
                         && format_change.submitted.height == 6
                         && rgb565_read
                         && rgb565_pixel.red == 255U
                         && rgb565_pixel.green == 0U
                         && rgb565_pixel.blue == 0U
                         && rgb565_pixel.alpha == 255U,
                     "RGB565 should recreate the texture and preserve packed red pixels");

        constexpr std::size_t rgb888_stride_bytes = 8U * 3U + 5U;
        std::array<std::byte, rgb888_stride_bytes * 6U> rgb888_pixels{};
        const auto rgb888_red = 1U * rgb888_stride_bytes + 2U * 3U;
        rgb888_pixels[rgb888_red] = std::byte{0xFFU};
        const raster::SurfaceView rgb888_surface{
            .pixels = rgb888_pixels,
            .width = 8,
            .height = 6,
            .stride_bytes = rgb888_stride_bytes,
            .pixel_format = raster::PixelFormat::rgb888,
        };
        const auto rgb888_present = host.present(rgb888_surface, {2, 1, 1, 1});
        host_sdl3::TestRgba8 rgb888_pixel{};
        const bool rgb888_read = host.read_test_output_pixel(20, 12, rgb888_pixel);
        ok &= expect(rgb888_present.is_ok()
                         && rgb888_present.frame_index == 4U
                         && rgb888_present.submitted.width == 8
                         && rgb888_present.submitted.height == 6
                         && rgb888_read
                         && rgb888_pixel.red == 255U
                         && rgb888_pixel.green == 0U
                         && rgb888_pixel.blue == 0U
                         && rgb888_pixel.alpha == 255U,
                     "RGB888 should recreate the texture and preserve byte-ordered red pixels");

        host.close();
        ok &= expect(!host.is_open(), "host close should release the SDL runtime");
        const auto closed_pump = host.pump_events(state.sink);
        const auto closed_present = host.present(surface, {});
        ok &= expect(closed_pump.code == host_sdl3::PumpStatusCode::not_open,
                     "closed host should reject event pumping as not open");
        ok &= expect(closed_present.status.code == raster::StatusCode::not_ready,
                     "closed host should reject raster presentation as not ready");

        const auto reopened = host.open(host_sdl3::Config{
            .title = "Charm Host SDL3 reopen smoke",
            .window_width = 32,
            .window_height = 24,
            .hidden = true,
            .resizable = false,
        });
        ok &= expect(reopened.is_ok(), "host should reopen after a complete close");
        ok &= expect(queue_sdl_window_close_event(),
                     "window close request should enter the reopened SDL queue");
        const auto no_sink = host.pump_events({});
        ok &= expect(no_sink.code == host_sdl3::PumpStatusCode::no_sink
                         && no_sink.quit_requested
                         && no_sink.polled >= 1U,
                     "missing input sink should not prevent draining quit events");
        host.close();
        return ok;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::puts("[backends-host-sdl3-smoke] ok");
    return 0;
}
