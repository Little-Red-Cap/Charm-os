module;

#include "Backends/contract/raster_display.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

export module player.host_sdl3_adapter;

import charm.backend.host.sdl3;
import charm.system.clock;
import charm.system.run_loop;
import input.raw_event;
import input.raw_sink;
import player.port;
import player.port_runtime;
import player.raster;

export namespace player::host_sdl3 {
    namespace backend = charm::backend::host::sdl3;
    namespace raster = charm::backend::contract::raster;

    enum class OpenCode : std::uint8_t {
        ok,
        invalid_surface,
        host_open_failed,
        runtime_bootstrap_failed,
        run_loop_failed,
    };

    class Runtime {
    public:
        Runtime() = default;
        ~Runtime() { close(); }

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;
        Runtime(Runtime&&) = delete;
        Runtime& operator=(Runtime&&) = delete;

        [[nodiscard]] OpenCode open(const backend::Config& config,
                                    PlayerRasterSurface surface,
                                    PlayerRuntimeEndpoint endpoint) {
            close();
            if (!surface.valid() || !endpoint.valid()) {
                return OpenCode::invalid_surface;
            }
            if (!host_.open(config).is_ok()) {
                return OpenCode::host_open_failed;
            }

            input_.reset();
            surface_ = surface;
            port_ = PlayerPort{
                .clock = {this, &Runtime::read_clock_us},
                .raster_surface = surface_,
                .raster_display = {this, &Runtime::present},
                .raw_input = {this, &Runtime::poll_input},
            };
            runtime_.emplace(port_, endpoint);
            if (!runtime_->bootstrap()) {
                close();
                return OpenCode::runtime_bootstrap_failed;
            }

            loop_.emplace();
            loop_->bind_clock(host_.clock());
            const bool loop_ready = loop_->add_step(charm::system::LoopPhase::io,
                                                     charm::system::SubmitProjection::event,
                                                     &Runtime::run_io,
                                                     this,
                                                     "player_host_io")
                && loop_->add_step(charm::system::LoopPhase::update,
                                   charm::system::SubmitProjection::event,
                                   &Runtime::run_update,
                                   this,
                                   "player_update")
                && loop_->add_step(charm::system::LoopPhase::render,
                                   charm::system::SubmitProjection::frame,
                                   &Runtime::run_render,
                                   this,
                                   "player_render");
            if (!loop_ready) {
                close();
                return OpenCode::run_loop_failed;
            }
            return OpenCode::ok;
        }

        void close() noexcept {
            loop_.reset();
            if (runtime_) {
                runtime_->shutdown();
                runtime_.reset();
            }
            input_.clear();
            port_ = {};
            surface_ = {};
            quit_requested_ = false;
            failed_ = false;
            host_.close();
        }

        [[nodiscard]] bool run_once() noexcept {
            if (!loop_ || !runtime_ || failed_ || quit_requested_) {
                return false;
            }
            loop_->run_once();
            return !failed_ && !quit_requested_;
        }

        [[nodiscard]] bool is_open() const noexcept { return host_.is_open(); }
        [[nodiscard]] bool quit_requested() const noexcept { return quit_requested_; }
        [[nodiscard]] bool failed() const noexcept { return failed_; }
        [[nodiscard]] std::uint64_t presented_frames() const noexcept {
            return host_.presented_frames();
        }
        [[nodiscard]] std::size_t input_received_count() const noexcept {
            return input_.received_count;
        }
        [[nodiscard]] std::size_t input_coalesced_count() const noexcept {
            return input_.coalesced_count;
        }
        [[nodiscard]] std::size_t input_dropped_count() const noexcept {
            return input_.dropped_count;
        }
        [[nodiscard]] std::size_t dispatched_input_count() const noexcept {
            return runtime_ ? runtime_->dispatched_input_count() : 0;
        }
        [[nodiscard]] const char* last_error() const noexcept { return host_.last_error(); }

    private:
        static constexpr std::size_t kInputCapacity = 256;
        static constexpr std::size_t kInputDrainBudget = 64;

        struct InputQueue {
            std::array<input::RawInputEvent, kInputCapacity> events{};
            std::size_t head{0};
            std::size_t size{0};
            std::size_t received_count{0};
            std::size_t coalesced_count{0};
            std::size_t dropped_count{0};

            [[nodiscard]] bool try_coalesce(const input::RawInputEvent& event) noexcept {
                if (size == 0) {
                    return false;
                }
                auto& previous = events[(head + size - 1) % events.size()];
                if (event.type == input::RawInputEventType::Pointer
                    && previous.type == input::RawInputEventType::Pointer
                    && event.pointer_action == input::PointerAction::Move
                    && previous.pointer_action == input::PointerAction::Move
                    && event.pointer.id == previous.pointer.id) {
                    previous = event;
                    return true;
                }
                if (event.type == input::RawInputEventType::Axis
                    && previous.type == input::RawInputEventType::Axis) {
                    previous = event;
                    return true;
                }
                if (event.type == input::RawInputEventType::Encoder
                    && previous.type == input::RawInputEventType::Encoder) {
                    const auto accumulated = static_cast<int>(previous.encoder_delta)
                        + static_cast<int>(event.encoder_delta);
                    previous.encoder_delta = static_cast<std::int16_t>(std::clamp(
                        accumulated,
                        static_cast<int>(std::numeric_limits<std::int16_t>::min()),
                        static_cast<int>(std::numeric_limits<std::int16_t>::max())));
                    previous.ms = event.ms;
                    return true;
                }
                return false;
            }

            [[nodiscard]] bool on_raw(const input::RawInputEvent& event) noexcept {
                ++received_count;
                if (try_coalesce(event)) {
                    ++coalesced_count;
                    return true;
                }
                if (size == events.size()) {
                    ++dropped_count;
                    return true;
                }
                events[(head + size) % events.size()] = event;
                ++size;
                return true;
            }

            [[nodiscard]] bool poll(input::RawInputEvent& event) noexcept {
                if (size == 0) {
                    return false;
                }
                event = events[head];
                head = (head + 1) % events.size();
                --size;
                return true;
            }

            void clear() noexcept {
                head = 0;
                size = 0;
            }

            void reset() noexcept {
                clear();
                received_count = 0;
                coalesced_count = 0;
                dropped_count = 0;
            }
        };

        [[nodiscard]] static PlayerClockTick read_clock_us(void* ctx) noexcept {
            const auto* self = static_cast<const Runtime*>(ctx);
            return self ? self->host_.clock().now_us() : 0;
        }

        [[nodiscard]] static PlayerInputPollResult poll_input(
            void* ctx, input::RawInputEvent& event) noexcept {
            auto* self = static_cast<Runtime*>(ctx);
            if (!self) {
                return PlayerInputPollResult::failed;
            }
            return self->input_.poll(event)
                ? PlayerInputPollResult::event
                : PlayerInputPollResult::empty;
        }

        [[nodiscard]] static raster::PixelFormat to_host_format(
            PlayerRasterPixelFormat format) noexcept {
            switch (format) {
            case PlayerRasterPixelFormat::RGB565: return raster::PixelFormat::rgb565;
            case PlayerRasterPixelFormat::RGB888: return raster::PixelFormat::rgb888;
            case PlayerRasterPixelFormat::ARGB8888: return raster::PixelFormat::argb8888;
            }
            return raster::PixelFormat::argb8888;
        }

        [[nodiscard]] static bool present(void* ctx,
                                          const PlayerRasterSurface& surface,
                                          PlayerRasterRegion dirty) noexcept {
            auto* self = static_cast<Runtime*>(ctx);
            if (!self || !surface.valid()) {
                return false;
            }
            const auto required = surface.required_size_bytes();
            const raster::SurfaceView host_surface{
                .pixels = std::span<const std::byte>{surface.pixels.first(required)},
                .width = static_cast<std::uint32_t>(surface.width),
                .height = static_cast<std::uint32_t>(surface.height),
                .stride_bytes = surface.stride_bytes,
                .pixel_format = to_host_format(surface.pixel_format),
            };
            return self->host_.present(host_surface,
                                       raster::DirtyRegion{dirty.x, dirty.y, dirty.w, dirty.h})
                .is_ok();
        }

        static void run_io(void* ctx,
                           charm::system::ClockTick,
                           charm::system::ClockTick) noexcept {
            auto* self = static_cast<Runtime*>(ctx);
            if (!self) {
                return;
            }
            const auto result = self->host_.pump_events(input::RawSinkRef::bind(self->input_));
            self->quit_requested_ = result.quit_requested;
            self->failed_ = !result.is_ok();
        }

        static void run_update(void* ctx,
                               charm::system::ClockTick now_us,
                               charm::system::ClockTick dt_us) noexcept {
            auto* self = static_cast<Runtime*>(ctx);
            if (!self || self->failed_ || self->quit_requested_ || !self->runtime_) {
                return;
            }
            self->failed_ = !self->runtime_->update_frame(
                now_us, dt_us, kInputDrainBudget);
        }

        static void run_render(void* ctx,
                               charm::system::ClockTick,
                               charm::system::ClockTick) noexcept {
            auto* self = static_cast<Runtime*>(ctx);
            if (!self || self->failed_ || self->quit_requested_ || !self->runtime_) {
                return;
            }
            self->failed_ = !self->runtime_->render_frame();
        }

        backend::Host host_{};
        InputQueue input_{};
        PlayerRasterSurface surface_{};
        PlayerPort port_{};
        std::optional<PlayerPortRuntime> runtime_{};
        std::optional<charm::system::RunLoop<3>> loop_{};
        bool quit_requested_{false};
        bool failed_{false};
    };
}
