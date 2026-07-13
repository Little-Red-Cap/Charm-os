import charm.core.config;
import charm.gfx.color;
import charm.system.clock;
import input.raw_event;
import player.controller;
import player.input;
import player.md3_port;
import player.port;
import player.port_runtime;
import player.raster;
import player.render_runtime;
import player.md3_runtime;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>

static_assert(!player::player_legacy_touch_input_enabled);

namespace {
    constexpr std::size_t kMaxBytesPerPixel = 4;
    constexpr std::size_t kRowPaddingBytes = 8;
    constexpr std::size_t kMaxPixelBytes = static_cast<std::size_t>(screen_width)
        * static_cast<std::size_t>(screen_height)
        * kMaxBytesPerPixel
        + static_cast<std::size_t>(screen_height) * kRowPaddingBytes;
    constexpr std::byte kPaddingSentinel{0xa5};

    struct RasterCase {
        player::PlayerRasterPixelFormat format{player::PlayerRasterPixelFormat::RGB888};
        std::size_t bytes_per_pixel{3};
        const char* name{"rgb888"};
        std::uint64_t expected_home_digest{0x07aa4cb2f9294d75ULL};
        std::uint64_t expected_library_digest{0xa58f2ba580a038a6ULL};
        std::uint64_t expected_now_playing_digest{0xfa25cc3ab677bb55ULL};
    };

    bool parse_arguments(int argc, char** argv, RasterCase& out, bool& record_mode) {
        for (int i = 1; i < argc; ++i) {
            const std::string_view argument{argv[i]};
            if (argument == "rgb888") {
                continue;
            }
            if (argument == "rgb565") {
                out = {player::PlayerRasterPixelFormat::RGB565,
                       2,
                       "rgb565",
                       0x63d820df64879e07ULL,
                       0x12b3af299d4f4757ULL,
                       0xf0377791050570d9ULL};
                continue;
            }
            if (argument == "argb8888") {
                out = {player::PlayerRasterPixelFormat::ARGB8888,
                       4,
                       "argb8888",
                       0xdf45cb39b0fce669ULL,
                       0x7c68954d89e456fcULL,
                       0x84f92ea4f4b7eb39ULL};
                continue;
            }
            if (argument == "--record") {
                record_mode = true;
                continue;
            }
            return false;
        }
        return true;
    }

    struct SmokeClock {
        player::PlayerClockTick now_us{1000};

        static player::PlayerClockTick read(void* ctx) noexcept {
            return static_cast<SmokeClock*>(ctx)->now_us;
        }
    };

    struct SmokeInput {
        bool emitted{false};

        static player::PlayerInputPollResult poll(
            void* ctx, input::RawInputEvent& out) noexcept {
            auto& self = *static_cast<SmokeInput*>(ctx);
            if (self.emitted) {
                return player::PlayerInputPollResult::empty;
            }
            self.emitted = true;
            out = input::RawInputEvent{
                .type = input::RawInputEventType::Pointer,
                .ms = 1,
                .pointer = input::PointerRaw{false, 16, 16, 0},
                .pointer_action = input::PointerAction::Move,
            };
            return player::PlayerInputPollResult::event;
        }
    };

    bool expect(bool condition, const char* message) {
        if (!condition) {
            std::printf("[player-md3-runtime-smoke] fail: %s\n", message);
        }
        return condition;
    }

    bool has_nonzero_pixel(std::span<const std::byte> pixels) noexcept {
        for (std::size_t i = 0; i < pixels.size(); i += 97) {
            if (pixels[i] != std::byte{}) {
                return true;
            }
        }
        return false;
    }

    std::uint64_t raster_digest(const std::byte* pixels,
                                std::size_t active_row_bytes,
                                std::size_t stride,
                                int height) noexcept {
        std::uint64_t hash = 14695981039346656037ULL;
        for (int y = 0; y < height; ++y) {
            const auto row = std::span{pixels + static_cast<std::size_t>(y) * stride,
                                       active_row_bytes};
            for (const auto value : row) {
                hash ^= static_cast<std::uint8_t>(value);
                hash *= 1099511628211ULL;
            }
        }
        return hash;
    }

    bool padding_unchanged(const std::byte* pixels,
                           std::size_t active_row_bytes,
                           std::size_t stride,
                           int height) noexcept {
        for (int y = 0; y < height; ++y) {
            const auto* padding = pixels + static_cast<std::size_t>(y) * stride + active_row_bytes;
            for (std::size_t i = active_row_bytes; i < stride; ++i) {
                if (padding[i - active_row_bytes] != kPaddingSentinel) {
                    return false;
                }
            }
        }
        return true;
    }

    bool verify_raster_layout(player::PlayerRasterPixelFormat format,
                              std::size_t bytes_per_pixel) {
        constexpr int width = 2;
        constexpr int height = 2;
        constexpr std::size_t padding = 3;
        const auto stride = static_cast<std::size_t>(width) * bytes_per_pixel + padding;
        std::array<std::byte, (2 * 4 + padding) * height> bytes{};
        bytes.fill(kPaddingSentinel);
        auto runtime = std::make_unique<player::PlayerRenderRuntime>(
            player::PlayerRasterSurface{bytes, width, height, stride, format});
        const rgba color{0xf8, 0x84, 0x1f, 0x7f};
        runtime->clear(color);

        std::array<std::byte, 4> expected{};
        switch (format) {
        case player::PlayerRasterPixelFormat::RGB565: {
            const std::uint16_t packed = 0xfc23;
            std::memcpy(expected.data(), &packed, sizeof(packed));
            break;
        }
        case player::PlayerRasterPixelFormat::RGB888:
            expected[0] = std::byte{0xf8};
            expected[1] = std::byte{0x84};
            expected[2] = std::byte{0x1f};
            break;
        case player::PlayerRasterPixelFormat::ARGB8888: {
            const std::uint32_t packed = 0x7ff8841f;
            std::memcpy(expected.data(), &packed, sizeof(packed));
            break;
        }
        }

        for (int y = 0; y < height; ++y) {
            const auto* row = bytes.data() + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                if (std::memcmp(row + static_cast<std::size_t>(x) * bytes_per_pixel,
                                expected.data(),
                                bytes_per_pixel) != 0) {
                    return false;
                }
            }
        }
        return padding_unchanged(bytes.data(), width * bytes_per_pixel, stride, height);
    }

    bool verify_digest(const char* page,
                       std::uint64_t actual,
                       std::uint64_t expected,
                       bool record_mode) {
        if (record_mode || actual == expected) {
            return true;
        }
        std::printf("[player-md3-runtime-smoke] digest mismatch: page=%s "
                    "actual=0x%016llx expected=0x%016llx\n",
                    page,
                    static_cast<unsigned long long>(actual),
                    static_cast<unsigned long long>(expected));
        return false;
    }
}

int main(int argc, char** argv) {
    RasterCase raster_case{};
    bool record_mode = false;
    if (!parse_arguments(argc, argv, raster_case, record_mode)) {
        std::printf("[player-md3-runtime-smoke] fail: unsupported argument\n");
        return 2;
    }

    const auto active_row_bytes = static_cast<std::size_t>(screen_width)
        * raster_case.bytes_per_pixel;
    const auto stride = active_row_bytes + kRowPaddingBytes;
    const auto pixel_bytes = stride * static_cast<std::size_t>(screen_height);
    static std::array<std::byte, kMaxPixelBytes> pixels{};
    pixels.fill(kPaddingSentinel);
    SmokeClock clock{};
    SmokeInput input{};
    player::PlayerMemoryRasterDisplayState display{};

    player::PlayerMd3RuntimeConfig<player::PlayerPage> config{
        .start_page = player::PlayerPage::Home,
        .initial_track_index = 0,
        .auto_start = false,
        .clear_color = rgba{9, 12, 16, 255},
    };
    static player::PlayerMd3PortApplication app{config};
    const player::PlayerPort port{
        .clock = {&clock, &SmokeClock::read},
        .raster_surface = {
            std::span<std::byte>{pixels}.first(pixel_bytes),
            screen_width,
            screen_height,
            stride,
            raster_case.format,
        },
        .raster_display = player::make_player_memory_raster_display(display),
        .raw_input = {&input, &SmokeInput::poll},
    };
    player::PlayerPortRuntime runtime{port, app.endpoint()};

    if (!expect(runtime.bootstrap(), "real MD3 bootstrap")
        || !expect(charm::system::ClockCaps::TimeSource::bound() == nullptr,
                   "Port clock remains instance-local")
        || !expect(app.runtime() != nullptr, "runtime materialized")
        || !expect(static_cast<bool>(app.controller().handles.root), "MD3 root bound")
        || !expect(!app.has_track(), "no storage remains a valid UI state")) {
        return 1;
    }

    clock.now_us = 17000;
    if (!expect(runtime.frame(17000, 16000), "real MD3 frame")
        || !expect(runtime.dispatched_input_count() == 1, "raw input dispatched")
        || !expect(display.present_count == 1, "raster presented")
        || !expect(display.last_surface.valid(), "presented surface valid")
        || !expect(display.last_surface.pixels.data() == pixels.data(), "borrowed raster identity")
        || !expect(display.last_surface.stride_bytes == stride, "borrowed raster stride")
        || !expect(display.last_surface.pixel_format == raster_case.format,
                   "borrowed raster format")
        || !expect(display.last_dirty.x == 0 && display.last_dirty.y == 0
                       && display.last_dirty.w == screen_width
                       && display.last_dirty.h == screen_height,
                   "top-left full dirty region")
        || !expect(has_nonzero_pixel(std::span{pixels.data(), pixel_bytes}),
                   "MD3 rendered pixels")
        || !expect(padding_unchanged(
                       pixels.data(), active_row_bytes, stride, screen_height),
                   "row padding preserved")
        || !expect(verify_raster_layout(raster_case.format, raster_case.bytes_per_pixel),
                   "raster byte layout")) {
        return 1;
    }

    const auto home_digest = raster_digest(
        pixels.data(), active_row_bytes, stride, screen_height);
    if (!expect(verify_digest("home",
                              home_digest,
                              raster_case.expected_home_digest,
                              record_mode),
                "home raster digest baseline")) {
        return 1;
    }

    app.controller().set_page(player::PlayerPage::Library);
    clock.now_us = 33000;
    if (!expect(runtime.frame(33000, 16000), "Library frame")
        || !expect(display.present_count == 2, "Library raster presented")
        || !expect(padding_unchanged(
                       pixels.data(), active_row_bytes, stride, screen_height),
                   "Library row padding preserved")) {
        return 1;
    }
    const auto library_digest = raster_digest(
        pixels.data(), active_row_bytes, stride, screen_height);
    if (!expect(verify_digest("library",
                              library_digest,
                              raster_case.expected_library_digest,
                              record_mode),
                "Library raster digest baseline")) {
        return 1;
    }

    app.controller().set_page(player::PlayerPage::NowPlaying);
    clock.now_us = 49000;
    if (!expect(runtime.frame(49000, 16000), "Now Playing frame")
        || !expect(display.present_count == 3, "Now Playing raster presented")
        || !expect(padding_unchanged(
                       pixels.data(), active_row_bytes, stride, screen_height),
                   "Now Playing row padding preserved")) {
        return 1;
    }
    const auto now_playing_digest = raster_digest(
        pixels.data(), active_row_bytes, stride, screen_height);
    if (!expect(verify_digest("now_playing",
                              now_playing_digest,
                              raster_case.expected_now_playing_digest,
                              record_mode),
                "Now Playing raster digest baseline")) {
        return 1;
    }

    runtime.shutdown();
    if (!expect(app.runtime() == nullptr, "runtime released")
        || !expect(runtime.state() == player::PlayerPortRuntimeState::Stopped, "runtime stopped")) {
        return 1;
    }

    std::printf("[player-md3-runtime-smoke] ok format=%s "
                "home=0x%016llx library=0x%016llx now_playing=0x%016llx\n",
                raster_case.name,
                static_cast<unsigned long long>(home_digest),
                static_cast<unsigned long long>(library_digest),
                static_cast<unsigned long long>(now_playing_digest));
    return 0;
}
