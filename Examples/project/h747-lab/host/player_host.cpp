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

std::uint32_t fnv1a32(const std::span<const std::byte> bytes) noexcept {
    std::uint32_t hash = 2166136261U;
    for (const auto byte : bytes) {
        hash ^= static_cast<std::uint8_t>(byte);
        hash *= 16777619U;
    }
    return hash;
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
    host::world::MockPlayerWorld world{};
    h747::apps::player::PlayerRuntime runtime{};
    h747::apps::player::init(world, runtime);
    for (int i = 0; i < 3; ++i) {
        world.clock().advance(1000U);
        h747::apps::player::loop_once(world, runtime);
    }

    const auto hash = fnv1a32(world.pixels());
    std::printf("player_host: hash=0x%08X presents=%u\n",
                static_cast<unsigned>(hash),
                static_cast<unsigned>(world.display().present_count()));
    const auto ppm = output_path((argc > 0) ? argv[0] : nullptr);
    if (!write_ppm(ppm, world)) {
        std::fprintf(stderr, "failed to write player_host.ppm\n");
        return 1;
    }
    std::printf("player_host: ppm=%s\n", ppm.string().c_str());
    return hash == 0U ? 2 : 0;
}
