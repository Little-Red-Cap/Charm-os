#include "apps/player/player_domain.hpp"
#include "apps/player/player_input.hpp"
#include "host/host_world_support.hpp"

#include <cstdint>
#include <cstdio>

namespace host::world {

class MockPlayerWorld {
public:
    using Log = h747::host_support::StdoutLog;
    using Clock = h747::host_support::ManualClock;
    using Display = h747::host_support::MemoryRasterDisplay;
    using Input = h747::host_support::NullInput;

    MockPlayerWorld() : display_(framebuffer_.framebuffer()) {}

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
        return framebuffer_.framebuffer();
    }

    [[nodiscard]] static constexpr charm::cap::DisplayMode mode() noexcept {
        return FrameBuffer::mode();
    }

    [[nodiscard]] std::span<const std::byte> pixels() const noexcept {
        return framebuffer_.pixels();
    }

private:
    using FrameBuffer = h747::host_support::HostFrameBufferStorage<180U, 320U>;

    FrameBuffer framebuffer_{};
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

} // namespace

int main(const int argc, char** argv) {
    namespace support = h747::host_support;

    const bool ci = support::has_arg(argc, argv, "--ci");
    const bool input_ok = run_input_regression();
    const bool command_ok = run_command_regression();
    host::world::MockPlayerWorld world{};
    h747::apps::player::PlayerRuntime runtime{};
    h747::apps::player::init(world, runtime);
    for (int i = 0; i < 3; ++i) {
        world.clock().advance(1000U);
        h747::apps::player::loop_once(world, runtime);
    }

    const auto hash = support::fnv1a32(world.pixels());
    const std::uint32_t presents = world.display().present_count();
    std::printf("player_host: hash=0x%08X presents=%u\n",
                static_cast<unsigned>(hash),
                static_cast<unsigned>(presents));
    const auto ppm = support::output_path((argc > 0) ? argv[0] : nullptr, "player_host.ppm");
    if (!support::write_ppm(ppm, world)) {
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
