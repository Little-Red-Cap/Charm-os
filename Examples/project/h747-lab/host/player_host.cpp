#include "apps/player/player_domain.hpp"
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

class MockPlayerWorld {
public:
    using Log = StdoutLog;
    using Clock = MockClock;
    using Display = MockRasterDisplay;

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
};

static_assert(charm::cap::RasterDisplayWorld<MockPlayerWorld>);

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

bool run_model_ci() noexcept {
    using h747::apps::player::PlayerCommand;
    using h747::apps::player::PlayerCommandKind;
    using h747::apps::player::PlayerBoardSnapshot;
    using h747::apps::player::PlayerRuntime;

    PlayerRuntime runtime{};
    runtime.reset();
    if (!runtime.view().playing || runtime.view().storage_ready || runtime.view().cover_ready) {
        return false;
    }

    runtime.observe_board(PlayerBoardSnapshot{
        .display_ready = true,
        .framebuffer_ready = true,
        .sdram_ready = true,
        .sdram_smoke_ok = true,
        .qspi_power_good = false,
        .qspi_jedec_ok = false,
        .qspi_read_ok = false,
    });
    if (!runtime.view().playing || runtime.view().storage_ready || runtime.view().cover_ready) {
        return false;
    }
    if (runtime.view().subtitle != "Display ready, storage probing") {
        return false;
    }

    runtime.observe_board(PlayerBoardSnapshot{
        .display_ready = true,
        .framebuffer_ready = true,
        .sdram_ready = true,
        .sdram_smoke_ok = true,
        .qspi_power_good = true,
        .qspi_jedec_ok = true,
        .qspi_read_ok = false,
    });
    if (!runtime.view().playing || !runtime.view().storage_ready || !runtime.view().cover_ready) {
        return false;
    }
    if (runtime.view().subtitle != "Display + storage ready") {
        return false;
    }

    runtime.observe_board(PlayerBoardSnapshot{
        .display_ready = true,
        .framebuffer_ready = false,
        .sdram_ready = false,
        .sdram_smoke_ok = false,
        .qspi_power_good = true,
        .qspi_jedec_ok = false,
        .qspi_read_ok = true,
    });
    if (runtime.view().playing || !runtime.view().storage_ready ||
        !runtime.view().cover_ready ||
        runtime.view().subtitle != "Display init, framebuffer pending") {
        return false;
    }

    runtime.dispatch(PlayerCommand{.kind = PlayerCommandKind::toggle_play});
    if (!runtime.view().playing) {
        return false;
    }

    runtime.dispatch(PlayerCommand{.kind = PlayerCommandKind::seek_relative, .delta_percent = 80});
    if (runtime.view().progress_percent != 100U) {
        return false;
    }

    runtime.dispatch(PlayerCommand{.kind = PlayerCommandKind::seek_relative, .delta_percent = -120});
    if (runtime.view().progress_percent != 0U) {
        return false;
    }

    runtime.dispatch(PlayerCommand{.kind = PlayerCommandKind::next_track});
    return runtime.view().progress_percent == 0U;
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
        const bool model_ok = run_model_ci();
        const bool visual_ok = (hash == kExpectedCiHash) && (presents == kExpectedCiPresents);
        const bool ok = model_ok && visual_ok;
        std::printf("[player-host-ci] ok=%u model_ok=%u visual_ok=%u hash=0x%08X expected_hash=0x%08X presents=%u expected_presents=%u\n",
                    ok ? 1U : 0U,
                    model_ok ? 1U : 0U,
                    visual_ok ? 1U : 0U,
                    static_cast<unsigned>(hash),
                    static_cast<unsigned>(kExpectedCiHash),
                    static_cast<unsigned>(presents),
                    static_cast<unsigned>(kExpectedCiPresents));
        return ok ? 0 : 3;
    }
    return hash == 0U ? 2 : 0;
}
