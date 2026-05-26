#include "apps/player/player_domain.hpp"
#include "apps/player/player_input.hpp"
#include "capabilities/world.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <span>
#include <string_view>

namespace host::world {

class StdoutLog {
public:
    [[nodiscard]] charm::cap::Transfer write(const std::span<const std::byte> bytes) noexcept {
        const auto written = std::fwrite(bytes.data(), 1U, bytes.size(), stdout);
        return charm::cap::Transfer{
            charm::cap::Status::from(written == bytes.size() ? charm::cap::StatusCode::ok
                                                             : charm::cap::StatusCode::io_error),
            written,
        };
    }

    [[nodiscard]] charm::cap::Transfer write(const std::string_view text) noexcept {
        return write(std::as_bytes(std::span<const char>{text.data(), text.size()}));
    }

    [[nodiscard]] charm::cap::Status flush() noexcept {
        return std::fflush(stdout) == 0 ? charm::cap::Status::ok()
                                        : charm::cap::Status::from(charm::cap::StatusCode::io_error);
    }
};

class MockClock {
public:
    [[nodiscard]] charm::cap::Milliseconds tick_ms() const noexcept {
        return {tick_};
    }

    void delay(const charm::cap::Milliseconds duration) noexcept {
        tick_ += duration.value;
    }

    void advance(const std::uint32_t ms) noexcept {
        tick_ += ms;
    }

private:
    std::uint32_t tick_{0U};
};

class MockRasterDisplay {
public:
    explicit MockRasterDisplay(charm::cap::FrameBuffer frame) : frame_(frame) {}

    [[nodiscard]] charm::cap::DisplayMode mode() const noexcept {
        return frame_.mode();
    }

    [[nodiscard]] charm::cap::Status present(const charm::cap::SurfaceView frame,
                                             std::span<const charm::cap::Rect> dirty_rects) noexcept {
        (void)dirty_rects;
        if (!charm::cap::same_mode(frame.mode, mode()) || frame.pixels.size() != frame_.pixels().size()) {
            return charm::cap::Status::from(charm::cap::StatusCode::invalid_argument);
        }
        std::memcpy(frame_.pixels().data(), frame.pixels.data(), frame.pixels.size());
        ++present_count_;
        return charm::cap::Status::ok();
    }

    [[nodiscard]] std::uint32_t present_count() const noexcept {
        return present_count_;
    }

private:
    charm::cap::FrameBuffer frame_;
    std::uint32_t present_count_{0U};
};

class MockInput {
public:
    [[nodiscard]] charm::cap::InputFrame sample() const noexcept {
        return {};
    }
};

class MockPlayerWorld {
public:
    using Log = StdoutLog;
    using Clock = MockClock;
    using Display = MockRasterDisplay;
    using Input = MockInput;

    MockPlayerWorld() : display_(framebuffer()) {}

    [[nodiscard]] Log& log() noexcept {
        return log_;
    }

    [[nodiscard]] Clock& clock() noexcept {
        return clock_;
    }

    [[nodiscard]] Display& display() noexcept {
        return display_;
    }

    [[nodiscard]] Input& input() noexcept {
        return input_;
    }

    [[nodiscard]] charm::cap::FrameBuffer framebuffer() noexcept {
        return charm::cap::FrameBuffer{
            std::span<std::byte>{pixels_.data(), pixels_.size()},
            mode(),
        };
    }

    [[nodiscard]] static constexpr charm::cap::DisplayMode mode() noexcept {
        return charm::cap::DisplayMode{
            .extent = charm::cap::Extent2D{.width = kWidth, .height = kHeight},
            .format = charm::cap::PixelFormat::argb8888,
            .stride_bytes = kWidth * 4U,
        };
    }

    [[nodiscard]] std::span<const std::byte> pixels() const noexcept {
        return pixels_;
    }

private:
    static constexpr std::uint16_t kWidth = 180U;
    static constexpr std::uint16_t kHeight = 320U;

    std::array<std::byte, static_cast<std::size_t>(kWidth) * kHeight * 4U> pixels_{};
    Log log_{};
    Clock clock_{};
    Display display_;
    Input input_{};
};

static_assert(charm::cap::RasterDisplayWorld<MockPlayerWorld>);
static_assert(charm::cap::InputWorld<MockPlayerWorld>);
static_assert(charm::cap::RasterDisplayInputWorld<MockPlayerWorld>);

} // namespace host::world

namespace {

constexpr std::uint32_t kExpectedCiHash = 0x650DDD82U;
constexpr std::uint32_t kExpectedCiPresents = 4U;

[[nodiscard]] bool has_arg(const int argc, char** argv, const char* expected) noexcept {
    for (int i = 1; i < argc; ++i) {
        if ((argv[i] != nullptr) && (std::strcmp(argv[i], expected) == 0)) {
            return true;
        }
    }
    return false;
}

std::uint32_t fnv1a32(const std::span<const std::byte> bytes) noexcept {
    std::uint32_t hash = 2166136261U;
    for (const auto byte : bytes) {
        hash ^= static_cast<std::uint8_t>(byte);
        hash *= 16777619U;
    }
    return hash;
}

[[nodiscard]] charm::cap::InputFrame make_input_frame(const std::int16_t encoder1_delta = 0,
                                                      const bool encoder1_pressed = false,
                                                      const std::int16_t encoder2_delta = 0,
                                                      const bool encoder2_pressed = false,
                                                      const bool pointer_detected = false,
                                                      const bool pointer_down = false,
                                                      const std::uint16_t pointer_x = 0U,
                                                      const std::uint16_t pointer_y = 0U,
                                                      const std::uint16_t pointer_max_x = 0U,
                                                      const std::uint16_t pointer_max_y = 0U) noexcept {
    return charm::cap::InputFrame{
        .encoder1 = charm::cap::EncoderSample{
            .detent_delta = encoder1_delta,
            .pressed = encoder1_pressed,
        },
        .encoder2 = charm::cap::EncoderSample{
            .detent_delta = encoder2_delta,
            .pressed = encoder2_pressed,
        },
        .pointer = charm::cap::PointerSample{
            .detected = pointer_detected,
            .down = pointer_down,
            .x = pointer_x,
            .y = pointer_y,
            .max_x = pointer_max_x,
            .max_y = pointer_max_y,
        },
    };
}

[[nodiscard]] bool run_input_regression() noexcept {
    h747::apps::player::PlayerRuntime runtime{};
    runtime.reset();

    h747::apps::player::PlayerBoardSnapshot board{};
    board.display_ready = true;
    board.framebuffer_ready = true;
    board.sdram_ready = true;
    board.sdram_smoke_ok = true;
    board.qspi_power_good = true;
    board.qspi_jedec_ok = true;
    runtime.observe_board(board);

    bool ok = true;
    const auto expect = [&ok](const bool condition, const char* what) noexcept {
        if (!condition) {
            ok = false;
            std::fprintf(stderr, "player_host: input regression failed: %s\n", what);
        }
    };

    expect(runtime.view().playing, "board-ready playback should start enabled");
    expect(runtime.view().storage_ready, "board-ready storage should be enabled");
    expect(runtime.view().cover_ready, "board-ready cover should be enabled");
    expect(runtime.view().subtitle == "Display + storage ready", "board-ready subtitle");

    runtime.observe_input(make_input_frame(3));
    expect(runtime.view().progress_percent == 36U, "encoder1 detent delta should add directly");

    runtime.observe_input(make_input_frame(0, false, -2));
    expect(runtime.view().progress_percent == 26U, "encoder2 detent delta should scale by five");

    runtime.observe_input(make_input_frame(0, true));
    expect(!runtime.view().playing, "encoder1 press should toggle playback off");

    runtime.observe_input(make_input_frame());
    runtime.observe_input(make_input_frame(0, true));
    expect(runtime.view().playing, "encoder1 second press should toggle playback on");

    runtime.observe_input(make_input_frame());
    runtime.observe_input(make_input_frame(0, false, 0, true));
    expect(runtime.view().playing, "encoder2 press should clear manual pause and restore board playback");

    runtime.observe_input(make_input_frame(0,
                                          false,
                                          0,
                                          false,
                                          true,
                                          true,
                                          360U,
                                          200U,
                                          720U,
                                          1280U));
    expect(runtime.view().progress_percent == 50U, "pointer drag should map x to progress");

    return ok;
}

[[nodiscard]] bool run_command_regression() noexcept {
    using h747::apps::player::PlayerInputCommand;
    using h747::apps::player::parse_player_input_event;

    bool ok = true;
    const auto expect = [&ok](const bool condition, const char* what) noexcept {
        if (!condition) {
            ok = false;
            std::fprintf(stderr, "player_host: command regression failed: %s\n", what);
        }
    };

    const auto status = parse_player_input_event(" status ");
    expect(status.command == PlayerInputCommand::status, "status command should parse");
    expect(!status.emits_input, "status should not emit input");

    const auto toggle = parse_player_input_event("toggle");
    expect(toggle.command == PlayerInputCommand::toggle, "toggle command should parse");
    expect(toggle.emits_input, "toggle should emit input");
    expect(toggle.frame.encoder1.pressed, "toggle should map to encoder1 press edge");

    const auto next = parse_player_input_event("next");
    expect(next.command == PlayerInputCommand::next, "next command should parse");
    expect(next.emits_input, "next should emit input");
    expect(next.frame.encoder2.detent_delta > 0, "next should move progress forward");

    const auto prev = parse_player_input_event("prev");
    expect(prev.command == PlayerInputCommand::previous, "prev command should parse");
    expect(prev.emits_input, "prev should emit input");
    expect(prev.frame.encoder2.detent_delta < 0, "prev should move progress backward");

    const auto seek_forward = parse_player_input_event("seek+");
    expect(seek_forward.command == PlayerInputCommand::seek_forward, "seek+ command should parse");
    expect(seek_forward.emits_input, "seek+ should emit input");
    expect(seek_forward.frame.encoder2.detent_delta == 1, "seek+ should map to one detent");

    const auto seek_backward = parse_player_input_event("seek-");
    expect(seek_backward.command == PlayerInputCommand::seek_backward, "seek- command should parse");
    expect(seek_backward.emits_input, "seek- should emit input");
    expect(seek_backward.frame.encoder2.detent_delta == -1, "seek- should map to one negative detent");

    const auto unknown = parse_player_input_event("surprise");
    expect(unknown.command == PlayerInputCommand::unknown, "unknown command should be explicit");
    expect(!unknown.emits_input, "unknown should not emit input");

    h747::apps::player::PlayerRuntime runtime{};
    runtime.reset();

    h747::apps::player::PlayerBoardSnapshot board{};
    board.display_ready = true;
    board.framebuffer_ready = true;
    board.sdram_ready = true;
    board.sdram_smoke_ok = true;
    runtime.observe_board(board);
    (void)runtime.consume_dirty();

    runtime.observe_input(toggle.frame);
    expect(!runtime.view().playing, "parsed toggle should pause runtime");
    expect(runtime.consume_dirty(), "parsed toggle should mark runtime dirty");

    runtime.observe_input({});
    (void)runtime.consume_dirty();
    runtime.observe_input(seek_forward.frame);
    expect(runtime.view().progress_percent == 38U, "parsed seek+ should advance progress by five");
    expect(runtime.consume_dirty(), "parsed seek+ should mark runtime dirty");

    return ok;
}

bool write_ppm(const std::filesystem::path& path, host::world::MockPlayerWorld& world) {
    auto* file = std::fopen(path.string().c_str(), "wb");
    if (file == nullptr) {
        return false;
    }
    const auto mode = world.mode();
    std::fprintf(file, "P6\n%u %u\n255\n", mode.extent.width, mode.extent.height);
    const auto pixels = world.pixels();
    for (std::size_t offset = 0U; offset + 3U < pixels.size(); offset += 4U) {
        const auto argb = reinterpret_cast<const std::uint8_t*>(pixels.data() + offset);
        const std::uint8_t rgb[3] = {argb[2], argb[1], argb[0]};
        std::fwrite(rgb, 1U, sizeof(rgb), file);
    }
    std::fclose(file);
    return true;
}

std::filesystem::path output_path(const char* argv0) {
    if ((argv0 == nullptr) || (argv0[0] == '\0')) {
        return std::filesystem::current_path() / "player_host.ppm";
    }
    return std::filesystem::absolute(argv0).parent_path() / "player_host.ppm";
}

} // namespace

int main(const int argc, char** argv) {
    const bool ci = has_arg(argc, argv, "--ci");
    const bool input_ok = run_input_regression();
    const bool command_ok = run_command_regression();
    host::world::MockPlayerWorld world{};
    h747::apps::player::PlayerRuntime runtime{};
    h747::apps::player::init(world, runtime);
    for (int i = 0; i < 3; ++i) {
        world.clock().advance(1000U);
        h747::apps::player::loop_once(world, runtime);
    }

    const auto hash = fnv1a32(world.pixels());
    const std::uint32_t presents = world.display().present_count();
    std::printf("player_host: hash=0x%08X presents=%u\n",
                static_cast<unsigned>(hash),
                static_cast<unsigned>(presents));
    const auto ppm = output_path((argc > 0) ? argv[0] : nullptr);
    if (!write_ppm(ppm, world)) {
        std::fprintf(stderr, "failed to write player_host.ppm\n");
        return 1;
    }
    std::printf("player_host: ppm=%s\n", ppm.string().c_str());
    if (ci) {
        const bool ok = input_ok && command_ok && (hash == kExpectedCiHash) &&
                        (presents == kExpectedCiPresents);
        std::printf("[player-host-ci] ok=%u input_ok=%u command_ok=%u hash=0x%08X expected_hash=0x%08X presents=%u expected_presents=%u\n",
                    ok ? 1U : 0U,
                    input_ok ? 1U : 0U,
                    command_ok ? 1U : 0U,
                    static_cast<unsigned>(hash),
                    static_cast<unsigned>(kExpectedCiHash),
                    static_cast<unsigned>(presents),
                    static_cast<unsigned>(kExpectedCiPresents));
        return ok ? 0 : 3;
    }
    return hash == 0U ? 2 : 0;
}
