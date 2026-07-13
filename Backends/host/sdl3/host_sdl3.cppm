module;

#include "Backends/contract/raster_display.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

export module charm.backend.host.sdl3;

import charm.system.clock;
import input.raw_event;
import input.raw_sink;

export namespace charm::backend::host::sdl3 {
    namespace raster = contract::raster;

    enum class StatusCode : std::uint8_t {
        ok,
        invalid_argument,
        already_open,
        sdl_error,
    };

    struct Status {
        StatusCode code{StatusCode::ok};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return code == StatusCode::ok;
        }
    };

    struct Config {
        const char* title{"Charm Host"};
        int window_width{640};
        int window_height{480};
        bool hidden{false};
        bool resizable{true};
    };

    enum class PumpStatusCode : std::uint8_t {
        ok,
        not_open,
        no_sink,
        foreign_window,
        sdl_error,
    };

    struct PumpResult {
        PumpStatusCode code{PumpStatusCode::ok};
        std::size_t polled{0};
        std::size_t delivered{0};
        std::size_t rejected{0};
        std::size_t ignored{0};
        bool quit_requested{false};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return code == PumpStatusCode::ok;
        }
    };

#if CHARM_HOST_SDL3_TESTING
    struct TestRgba8 {
        std::uint8_t red{0};
        std::uint8_t green{0};
        std::uint8_t blue{0};
        std::uint8_t alpha{0};
    };
#endif

    class Host {
    public:
        Host() noexcept
            : clock_{this, charm::system::ClockOps{&Host::clock_now_ms, &Host::clock_now_us}} {}

        ~Host() {
            close();
        }

        Host(const Host&) = delete;
        Host& operator=(const Host&) = delete;
        Host(Host&&) = delete;
        Host& operator=(Host&&) = delete;

        [[nodiscard]] Status open(const Config& config = {}) noexcept {
            close();
            if (!config.title || config.window_width <= 0 || config.window_height <= 0) {
                return {StatusCode::invalid_argument};
            }
            if (active_host_ && active_host_ != this) {
                return {StatusCode::already_open};
            }

            if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
                return {StatusCode::sdl_error};
            }
            video_initialized_ = true;
            int existing_window_count = 0;
            SDL_Window* existing_window = nullptr;
            if (!query_windows(existing_window_count, existing_window)) {
                close();
                return {StatusCode::sdl_error};
            }
            (void)existing_window;
            if (existing_window_count != 0) {
                close();
                return {StatusCode::already_open};
            }

            SDL_WindowFlags flags = 0;
            if (config.hidden) {
                flags |= SDL_WINDOW_HIDDEN;
            }
            if (config.resizable) {
                flags |= SDL_WINDOW_RESIZABLE;
            }
            window_ = SDL_CreateWindow(config.title,
                                       config.window_width,
                                       config.window_height,
                                       flags);
            if (!window_) {
                close();
                return {StatusCode::sdl_error};
            }

            renderer_ = SDL_CreateRenderer(window_, nullptr);
            if (!renderer_) {
                close();
                return {StatusCode::sdl_error};
            }
            if (!SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255)) {
                close();
                return {StatusCode::sdl_error};
            }
            pointer_down_ = false;
            presented_frames_ = 0;
            active_host_ = this;
            return {};
        }

        void close() noexcept {
            destroy_texture();
            if (renderer_) {
                SDL_DestroyRenderer(renderer_);
                renderer_ = nullptr;
            }
            if (window_) {
                SDL_DestroyWindow(window_);
                window_ = nullptr;
            }
            if (video_initialized_) {
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
                video_initialized_ = false;
            }
            surface_width_ = 0;
            surface_height_ = 0;
            pointer_down_ = false;
#if CHARM_HOST_SDL3_TESTING
            SDL_free(test_output_);
            test_output_ = nullptr;
            test_output_size_ = 0;
            test_output_capacity_ = 0;
            test_output_width_ = 0;
            test_output_height_ = 0;
#endif
            if (active_host_ == this) {
                active_host_ = nullptr;
            }
        }

        [[nodiscard]] bool is_open() const noexcept {
            return window_ != nullptr && renderer_ != nullptr;
        }

        [[nodiscard]] const char* last_error() const noexcept {
            return SDL_GetError();
        }

        [[nodiscard]] charm::system::Clock& clock() noexcept {
            return clock_;
        }

        [[nodiscard]] const charm::system::Clock& clock() const noexcept {
            return clock_;
        }

        [[nodiscard]] std::uint64_t presented_frames() const noexcept {
            return presented_frames_;
        }

#if CHARM_HOST_SDL3_TESTING
        [[nodiscard]] bool read_test_output_pixel(const int x,
                                                  const int y,
                                                  TestRgba8& out) const noexcept {
            if (x < 0 || y < 0 || x >= test_output_width_ || y >= test_output_height_) {
                return false;
            }
            const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(test_output_width_)
                + static_cast<std::size_t>(x);
            if (!test_output_ || index >= test_output_size_) {
                return false;
            }
            out = test_output_[index];
            return true;
        }
#endif

        [[nodiscard]] raster::PresentResult present(const raster::SurfaceView surface,
                                                     raster::DirtyRegion dirty) noexcept {
            if (!is_open()) {
                return raster_failure(raster::StatusCode::not_ready);
            }
            if (!surface.valid()
                || surface.stride_bytes > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                return raster_failure(raster::StatusCode::invalid_argument);
            }
            if (dirty.empty()) {
                dirty = raster::full_region(surface);
            }
            auto clipped = raster::clip_region(dirty, surface);
            if (clipped.empty()) {
                return raster_failure(raster::StatusCode::invalid_argument);
            }
            const bool requires_full_upload = !texture_
                || texture_width_ != static_cast<int>(surface.width)
                || texture_height_ != static_cast<int>(surface.height)
                || texture_format_ != surface.pixel_format;
            if (!ensure_texture(surface)) {
                return raster_failure(raster::StatusCode::backend_error);
            }
            if (requires_full_upload) {
                clipped = raster::full_region(surface);
            }

            const SDL_Rect update_rect{
                clipped.x,
                clipped.y,
                clipped.width,
                clipped.height,
            };
            const auto offset = static_cast<std::size_t>(clipped.y) * surface.stride_bytes
                + static_cast<std::size_t>(clipped.x) * raster::bytes_per_pixel(surface.pixel_format);
            const auto* source = surface.pixels.data() + offset;
            if (!SDL_UpdateTexture(texture_,
                                   &update_rect,
                                   source,
                                   static_cast<int>(surface.stride_bytes))
                || !SDL_RenderClear(renderer_)
                || !SDL_RenderTexture(renderer_, texture_, nullptr, nullptr)) {
                return raster_failure(raster::StatusCode::backend_error);
            }
#if CHARM_HOST_SDL3_TESTING
            if (!capture_test_output()) {
                return raster_failure(raster::StatusCode::backend_error);
            }
#endif
            if (!SDL_RenderPresent(renderer_)) {
                return raster_failure(raster::StatusCode::backend_error);
            }

            surface_width_ = static_cast<int>(surface.width);
            surface_height_ = static_cast<int>(surface.height);
            ++presented_frames_;
            return raster::PresentResult{
                .submitted = clipped,
                .frame_index = presented_frames_,
            };
        }

        [[nodiscard]] PumpResult pump_events(const input::RawSinkRef sink) noexcept {
            if (!is_open()) {
                return {.code = PumpStatusCode::not_open};
            }
            const auto ownership = window_ownership();
            if (ownership == WindowOwnership::query_error) {
                return {.code = PumpStatusCode::sdl_error};
            }
            if (ownership == WindowOwnership::foreign) {
                return {.code = PumpStatusCode::foreign_window};
            }
            const bool has_sink = sink.fn() != nullptr;
            PumpResult result{
                .code = has_sink ? PumpStatusCode::ok : PumpStatusCode::no_sink,
            };
            SDL_Event event{};
            while (SDL_PollEvent(&event)) {
                ++result.polled;
                if (event.type == SDL_EVENT_QUIT) {
                    result.quit_requested = true;
                    continue;
                }
                if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                    if (event_belongs_to_window(event)) {
                        result.quit_requested = true;
                    } else {
                        ++result.ignored;
                    }
                    continue;
                }
                if (!event_belongs_to_window(event)) {
                    ++result.ignored;
                    continue;
                }

                input::RawInputEvent raw{};
                if (!translate_event(event, raw)) {
                    continue;
                }
                if (has_sink && sink.fn()(sink.ctx(), raw)) {
                    ++result.delivered;
                } else {
                    ++result.rejected;
                }
            }
            return result;
        }

    private:
        [[nodiscard]] static charm::system::ClockTick clock_now_ms(void*) noexcept {
            return static_cast<charm::system::ClockTick>(SDL_GetTicks());
        }

        [[nodiscard]] static charm::system::ClockTick clock_now_us(void*) noexcept {
            return static_cast<charm::system::ClockTick>(SDL_GetTicksNS() / 1000ULL);
        }

        [[nodiscard]] static raster::PresentResult raster_failure(const raster::StatusCode code) noexcept {
            return raster::PresentResult{
                .status = raster::Status{code},
            };
        }

        [[nodiscard]] static SDL_PixelFormat to_sdl_format(const raster::PixelFormat format) noexcept {
            switch (format) {
            case raster::PixelFormat::rgb565:
                return SDL_PIXELFORMAT_RGB565;
            case raster::PixelFormat::rgb888:
                return SDL_PIXELFORMAT_RGB24;
            case raster::PixelFormat::argb8888:
                return SDL_PIXELFORMAT_ARGB8888;
            }
            return SDL_PIXELFORMAT_UNKNOWN;
        }

        void destroy_texture() noexcept {
            if (texture_) {
                SDL_DestroyTexture(texture_);
                texture_ = nullptr;
            }
            texture_width_ = 0;
            texture_height_ = 0;
            texture_format_ = raster::PixelFormat::argb8888;
        }

        [[nodiscard]] bool ensure_texture(const raster::SurfaceView surface) noexcept {
            const auto width = static_cast<int>(surface.width);
            const auto height = static_cast<int>(surface.height);
            if (texture_
                && texture_width_ == width
                && texture_height_ == height
                && texture_format_ == surface.pixel_format) {
                return true;
            }

            destroy_texture();
            texture_ = SDL_CreateTexture(renderer_,
                                         to_sdl_format(surface.pixel_format),
                                         SDL_TEXTUREACCESS_STREAMING,
                                         width,
                                         height);
            if (!texture_) {
                return false;
            }
            if (!SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST)) {
                destroy_texture();
                return false;
            }
            texture_width_ = width;
            texture_height_ = height;
            texture_format_ = surface.pixel_format;
            return true;
        }

        [[nodiscard]] std::uint32_t event_time_ms(const SDL_Event& event) const noexcept {
            if (event.common.timestamp != 0U) {
                return static_cast<std::uint32_t>(event.common.timestamp / 1'000'000ULL);
            }
            return static_cast<std::uint32_t>(SDL_GetTicks());
        }

        [[nodiscard]] static std::int16_t clamp_i16(const float value) noexcept {
            if (!std::isfinite(value)) {
                return 0;
            }
            constexpr auto min = static_cast<float>(std::numeric_limits<std::int16_t>::min());
            constexpr auto max = static_cast<float>(std::numeric_limits<std::int16_t>::max());
            if (value < min) {
                return std::numeric_limits<std::int16_t>::min();
            }
            if (value > max) {
                return std::numeric_limits<std::int16_t>::max();
            }
            return static_cast<std::int16_t>(value);
        }

        [[nodiscard]] input::PointerRaw pointer_sample(const float x,
                                                       const float y,
                                                       const bool down) const noexcept {
            float mapped_x = x;
            float mapped_y = y;
            int window_width = 0;
            int window_height = 0;
            if (surface_width_ > 0 && surface_height_ > 0
                && SDL_GetWindowSize(window_, &window_width, &window_height)
                && window_width > 0 && window_height > 0) {
                mapped_x = x * static_cast<float>(surface_width_) / static_cast<float>(window_width);
                mapped_y = y * static_cast<float>(surface_height_) / static_cast<float>(window_height);
            }
            return input::PointerRaw{
                .down = down,
                .x = clamp_i16(mapped_x),
                .y = clamp_i16(mapped_y),
                .id = 0,
            };
        }

        [[nodiscard]] static bool map_button(const SDL_Keycode key, input::Button& button) noexcept {
            switch (key) {
            case SDLK_UP:
                button = input::Button::Up;
                return true;
            case SDLK_DOWN:
                button = input::Button::Down;
                return true;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                button = input::Button::Enter;
                return true;
            case SDLK_ESCAPE:
            case SDLK_BACKSPACE:
                button = input::Button::Back;
                return true;
            default:
                return false;
            }
        }

        enum class WindowOwnership : std::uint8_t {
            owned,
            foreign,
            query_error,
        };

        [[nodiscard]] static bool query_windows(int& count,
                                                SDL_Window*& only_window) noexcept {
            count = 0;
            only_window = nullptr;
            int queried_count = 0;
            SDL_Window** windows = SDL_GetWindows(&queried_count);
            if (!windows) {
                return false;
            }
            count = queried_count;
            if (queried_count == 1) {
                only_window = windows[0];
            }
            SDL_free(windows);
            return true;
        }

        [[nodiscard]] WindowOwnership window_ownership() const noexcept {
            int count = 0;
            SDL_Window* only_window = nullptr;
            if (!query_windows(count, only_window)) {
                return WindowOwnership::query_error;
            }
            return count == 1 && only_window == window_
                ? WindowOwnership::owned
                : WindowOwnership::foreign;
        }

#if CHARM_HOST_SDL3_TESTING
        [[nodiscard]] bool capture_test_output() noexcept {
            SDL_Surface* output = SDL_RenderReadPixels(renderer_, nullptr);
            if (!output || output->w <= 0 || output->h <= 0) {
                SDL_DestroySurface(output);
                return false;
            }
            const auto width = static_cast<std::size_t>(output->w);
            const auto height = static_cast<std::size_t>(output->h);
            if (width > std::numeric_limits<std::size_t>::max() / height) {
                SDL_DestroySurface(output);
                return false;
            }
            const auto required = width * height;
            if (required > std::numeric_limits<std::size_t>::max() / sizeof(TestRgba8)) {
                SDL_DestroySurface(output);
                return false;
            }
            if (required > test_output_capacity_) {
                void* resized = SDL_realloc(test_output_, required * sizeof(TestRgba8));
                if (!resized) {
                    SDL_DestroySurface(output);
                    return false;
                }
                test_output_ = static_cast<TestRgba8*>(resized);
                test_output_capacity_ = required;
            }
            test_output_size_ = required;

            bool ok = true;
            for (int y = 0; y < output->h && ok; ++y) {
                for (int x = 0; x < output->w; ++x) {
                    auto& pixel = test_output_[static_cast<std::size_t>(y) * width
                                               + static_cast<std::size_t>(x)];
                    if (!SDL_ReadSurfacePixel(output,
                                              x,
                                              y,
                                              &pixel.red,
                                              &pixel.green,
                                              &pixel.blue,
                                              &pixel.alpha)) {
                        ok = false;
                        break;
                    }
                }
            }
            test_output_width_ = ok ? output->w : 0;
            test_output_height_ = ok ? output->h : 0;
            if (!ok) {
                test_output_size_ = 0;
            }
            SDL_DestroySurface(output);
            return ok;
        }
#endif

        [[nodiscard]] bool event_belongs_to_window(const SDL_Event& event) const noexcept {
            SDL_WindowID event_window = 0;
            switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                event_window = event.motion.windowID;
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                event_window = event.button.windowID;
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                event_window = event.wheel.windowID;
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                event_window = event.key.windowID;
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                event_window = event.window.windowID;
                break;
            default:
                return true;
            }
            return event_window == 0U
                || (window_ && event_window == SDL_GetWindowID(window_));
        }

        [[nodiscard]] bool translate_event(const SDL_Event& event,
                                           input::RawInputEvent& out) noexcept {
            out.ms = event_time_ms(event);
            switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                out.type = input::RawInputEventType::Pointer;
                out.pointer = pointer_sample(event.motion.x, event.motion.y, pointer_down_);
                out.pointer_action = input::PointerAction::Move;
                return true;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button != SDL_BUTTON_LEFT) {
                    return false;
                }
                pointer_down_ = true;
                out.type = input::RawInputEventType::Pointer;
                out.pointer = pointer_sample(event.button.x, event.button.y, true);
                out.pointer_action = input::PointerAction::Down;
                return true;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button != SDL_BUTTON_LEFT) {
                    return false;
                }
                pointer_down_ = false;
                out.type = input::RawInputEventType::Pointer;
                out.pointer = pointer_sample(event.button.x, event.button.y, false);
                out.pointer_action = input::PointerAction::Up;
                return true;
            case SDL_EVENT_MOUSE_WHEEL: {
                const float wheel_y = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                    ? -event.wheel.y
                    : event.wheel.y;
                if (!std::isfinite(wheel_y)) {
                    return false;
                }
                auto delta = clamp_i16(wheel_y);
                if (delta == 0 && wheel_y != 0.0F) {
                    delta = wheel_y > 0.0F ? 1 : -1;
                }
                out.type = input::RawInputEventType::Encoder;
                out.encoder_delta = delta;
                return true;
            }
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                if (!map_button(event.key.key, out.button)) {
                    return false;
                }
                out.type = input::RawInputEventType::Button;
                out.pressed = event.type == SDL_EVENT_KEY_DOWN;
                return true;
            default:
                return false;
            }
        }

        SDL_Window* window_{nullptr};
        SDL_Renderer* renderer_{nullptr};
        SDL_Texture* texture_{nullptr};
        bool video_initialized_{false};
        bool pointer_down_{false};
        int texture_width_{0};
        int texture_height_{0};
        raster::PixelFormat texture_format_{raster::PixelFormat::argb8888};
        int surface_width_{0};
        int surface_height_{0};
        std::uint64_t presented_frames_{0};
        charm::system::Clock clock_{};
        inline static Host* active_host_{nullptr};
#if CHARM_HOST_SDL3_TESTING
        TestRgba8* test_output_{nullptr};
        std::size_t test_output_size_{0};
        std::size_t test_output_capacity_{0};
        int test_output_width_{0};
        int test_output_height_{0};
#endif
    };

    static_assert(raster::RasterDisplay::satisfied_by<Host>);
}
