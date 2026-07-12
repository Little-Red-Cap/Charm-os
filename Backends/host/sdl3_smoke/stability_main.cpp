#include "Backends/contract/raster_display.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string_view>
#include <system_error>

import charm.backend.host.sdl3;
import input.raw_event;
import input.raw_sink;

namespace host_sdl3 = charm::backend::host::sdl3;
namespace raster = charm::backend::contract::raster;

std::size_t queue_sdl_event_burst(std::size_t count) noexcept;

namespace {
    struct Options {
        std::size_t repeat{10};
        std::size_t frames{30};
        std::size_t event_burst{32};
    };

    struct Metrics {
        std::uint64_t sessions{0};
        std::uint64_t presents{0};
        std::uint64_t polled{0};
        std::uint64_t delivered{0};
        std::uint64_t rejected{0};
        std::uint64_t ignored{0};
        std::uint64_t errors{0};
    };

    struct BurstCollector {
        std::size_t seen{0};
        std::size_t accepted{0};
        std::size_t rejected{0};
        std::size_t burst_index{0};
        bool order_ok{true};

        void begin_burst() noexcept {
            burst_index = 0;
        }

        bool on_raw(const input::RawInputEvent& event) noexcept {
            const bool expected_pressed = (burst_index % 2U) == 0U;
            order_ok = order_ok
                && event.type == input::RawInputEventType::Button
                && event.button == input::Button::Enter
                && event.pressed == expected_pressed;
            ++burst_index;
            ++seen;
            if ((seen % 17U) == 0U) {
                ++rejected;
                return false;
            }
            ++accepted;
            return true;
        }
    };

    [[nodiscard]] bool parse_size(const std::string_view text,
                                  const std::size_t min,
                                  const std::size_t max,
                                  std::size_t& value) noexcept {
        std::size_t parsed = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size()
            || parsed < min || parsed > max) {
            return false;
        }
        value = parsed;
        return true;
    }

    [[nodiscard]] bool parse_options(const int argc, char** argv, Options& options) noexcept {
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i] ? argv[i] : "";
            constexpr std::string_view repeat_prefix{"--repeat="};
            constexpr std::string_view frames_prefix{"--frames="};
            constexpr std::string_view burst_prefix{"--event-burst="};
            if (arg.starts_with(repeat_prefix)) {
                if (!parse_size(arg.substr(repeat_prefix.size()), 1U, 10'000U, options.repeat)) {
                    return false;
                }
            } else if (arg.starts_with(frames_prefix)) {
                if (!parse_size(arg.substr(frames_prefix.size()), 1U, 1'000'000U, options.frames)) {
                    return false;
                }
            } else if (arg.starts_with(burst_prefix)) {
                if (!parse_size(arg.substr(burst_prefix.size()), 0U, 100'000U,
                                options.event_burst)) {
                    return false;
                }
            } else {
                return false;
            }
        }
        return true;
    }

    void report_error(Metrics& metrics,
                      const std::size_t session,
                      const std::size_t frame,
                      const char* message) noexcept {
        ++metrics.errors;
        std::fprintf(stderr,
                     "[host-sdl3-stability][ERR] session=%zu frame=%zu %s\n",
                     session,
                     frame,
                     message);
    }

    [[nodiscard]] bool run_session(const Options& options,
                                   const std::size_t session,
                                   Metrics& metrics) noexcept {
        constexpr std::size_t width = 32U;
        constexpr std::size_t height = 24U;
        std::array<std::uint32_t, width * height> pixels{};
        pixels.fill(0xFF000000U);
        const raster::SurfaceView surface{
            .pixels = std::as_bytes(std::span{pixels}),
            .width = static_cast<std::uint32_t>(width),
            .height = static_cast<std::uint32_t>(height),
            .stride_bytes = width * sizeof(std::uint32_t),
            .pixel_format = raster::PixelFormat::argb8888,
        };

        host_sdl3::Host host{};
        const auto opened = host.open(host_sdl3::Config{
            .title = "Charm Host SDL3 stability",
            .window_width = 64,
            .window_height = 48,
            .hidden = true,
            .resizable = false,
        });
        if (!opened.is_ok()) {
            report_error(metrics, session, 0, host.last_error());
            return false;
        }

        const auto initial = host.present(surface, {});
        if (!initial.is_ok() || initial.frame_index != 1U) {
            report_error(metrics, session, 0, "initial present failed");
            host.close();
            return false;
        }
        ++metrics.presents;

        BurstCollector collector{};
        const auto sink = input::RawSinkRef::bind(collector);
        for (std::size_t frame = 0; frame < options.frames; ++frame) {
            collector.begin_burst();
            const auto seen_before = collector.seen;
            const auto accepted_before = collector.accepted;
            const auto rejected_before = collector.rejected;
            const auto pushed = queue_sdl_event_burst(options.event_burst);
            if (pushed != options.event_burst) {
                report_error(metrics, session, frame, "SDL event queue rejected the burst");
                host.close();
                return false;
            }

            const auto pump = host.pump_events(sink);
            metrics.polled += pump.polled;
            metrics.delivered += pump.delivered;
            metrics.rejected += pump.rejected;
            metrics.ignored += pump.ignored;
            const auto seen_delta = collector.seen - seen_before;
            const auto accepted_delta = collector.accepted - accepted_before;
            const auto rejected_delta = collector.rejected - rejected_before;
            if (!pump.is_ok() || pump.quit_requested
                || seen_delta != options.event_burst
                || accepted_delta != pump.delivered
                || rejected_delta != pump.rejected
                || pump.delivered + pump.rejected != options.event_burst
                || !collector.order_ok) {
                report_error(metrics, session, frame, "event drain invariant failed");
                host.close();
                return false;
            }

            const auto x = (frame * 7U + session) % width;
            const auto y = (frame * 11U + session) % height;
            const auto red = static_cast<std::uint8_t>((frame * 37U + session * 3U) & 0xFFU);
            const auto green = static_cast<std::uint8_t>((frame * 19U + session * 5U) & 0xFFU);
            const auto blue = static_cast<std::uint8_t>((frame * 11U + session * 7U) & 0xFFU);
            pixels[y * width + x] = 0xFF000000U
                | (static_cast<std::uint32_t>(red) << 16U)
                | (static_cast<std::uint32_t>(green) << 8U)
                | static_cast<std::uint32_t>(blue);
            const auto present = host.present(surface,
                                              raster::DirtyRegion{
                                                  .x = static_cast<std::int32_t>(x),
                                                  .y = static_cast<std::int32_t>(y),
                                                  .width = 1,
                                                  .height = 1,
                                              });
            if (!present.is_ok() || present.frame_index != frame + 2U
                || present.submitted.width != 1 || present.submitted.height != 1) {
                report_error(metrics, session, frame, "partial present invariant failed");
                host.close();
                return false;
            }
            host_sdl3::TestRgba8 output{};
            const auto output_x = static_cast<int>(x * 2U + 1U);
            const auto output_y = static_cast<int>(y * 2U + 1U);
            if (!host.read_test_output_pixel(output_x, output_y, output)
                || output.red != red || output.green != green || output.blue != blue
                || output.alpha != 255U) {
                report_error(metrics, session, frame, "partial present pixel readback failed");
                host.close();
                return false;
            }
            ++metrics.presents;
        }

        if (host.presented_frames() != options.frames + 1U) {
            report_error(metrics, session, options.frames, "presented frame total mismatch");
            host.close();
            return false;
        }
        host.close();
        if (host.is_open()) {
            report_error(metrics, session, options.frames, "close did not release the host");
            return false;
        }
        ++metrics.sessions;
        return true;
    }
}

int main(const int argc, char** argv) {
    Options options{};
    if (!parse_options(argc, argv, options)) {
        std::fprintf(stderr,
                     "usage: backends-host-sdl3-stability-smoke "
                     "[--repeat=N] [--frames=N] [--event-burst=N]\n");
        return 2;
    }

    Metrics metrics{};
    const auto started = std::chrono::steady_clock::now();
    for (std::size_t session = 0; session < options.repeat; ++session) {
        if (!run_session(options, session, metrics)) {
            break;
        }
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    std::printf("[host-sdl3-stability] repeat=%zu frames=%zu event_burst=%zu "
                "sessions=%llu presents=%llu polled=%llu delivered=%llu "
                "rejected=%llu ignored=%llu errors=%llu elapsed_ms=%llu\n",
                options.repeat,
                options.frames,
                options.event_burst,
                static_cast<unsigned long long>(metrics.sessions),
                static_cast<unsigned long long>(metrics.presents),
                static_cast<unsigned long long>(metrics.polled),
                static_cast<unsigned long long>(metrics.delivered),
                static_cast<unsigned long long>(metrics.rejected),
                static_cast<unsigned long long>(metrics.ignored),
                static_cast<unsigned long long>(metrics.errors),
                static_cast<unsigned long long>(elapsed.count()));
    if (metrics.errors != 0U || metrics.sessions != options.repeat) {
        return 1;
    }
    std::puts("[backends-host-sdl3-stability-smoke] ok");
    return 0;
}
