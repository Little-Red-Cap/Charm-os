#include "apps/display_raster_demo/display_raster_demo_domain.hpp"
#include "host/host_world_support.hpp"

#include <cstdint>
#include <cstdio>

namespace host::world {

class MockDisplayWorld {
public:
    using Log = h747::host_support::StdoutLog;
    using Clock = h747::host_support::ManualClock;
    using Display = h747::host_support::MemoryRasterDisplay;

    MockDisplayWorld() : display_(framebuffer_.framebuffer()) {}

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
};

static_assert(charm::cap::RasterDisplayWorld<MockDisplayWorld>);

} // namespace host::world

namespace {

constexpr std::uint32_t kExpectedCiHash = 0x644909C5U;
constexpr std::uint32_t kExpectedCiPresents = 4U;

} // namespace

int main(const int argc, char** argv) {
    namespace support = h747::host_support;

    const bool ci = support::has_arg(argc, argv, "--ci");
    host::world::MockDisplayWorld world{};
    h747::apps::display_raster_demo::init(world);
    for (int i = 0; i < 3; ++i) {
        world.clock().advance(1000U);
        h747::apps::display_raster_demo::loop_once(world);
    }

    const auto hash = support::fnv1a32(world.pixels());
    const std::uint32_t presents = world.display().present_count();
    std::printf("display_raster_demo: hash=0x%08X presents=%u\n",
                static_cast<unsigned>(hash),
                static_cast<unsigned>(presents));
    const auto ppm = support::output_path((argc > 0) ? argv[0] : nullptr, "display_raster_demo.ppm");
    if (!support::write_ppm(ppm, world)) {
        std::fprintf(stderr, "failed to write display_raster_demo.ppm\n");
        return 1;
    }
    std::printf("display_raster_demo: ppm=%s\n", ppm.string().c_str());
    if (ci) {
        const bool ok = (hash == kExpectedCiHash) && (presents == kExpectedCiPresents);
        std::printf("[display-raster-host-ci] ok=%u hash=0x%08X expected_hash=0x%08X presents=%u expected_presents=%u\n",
                    ok ? 1U : 0U,
                    static_cast<unsigned>(hash),
                    static_cast<unsigned>(kExpectedCiHash),
                    static_cast<unsigned>(presents),
                    static_cast<unsigned>(kExpectedCiPresents));
        return ok ? 0 : 3;
    }
    return hash == 0U ? 2 : 0;
}
