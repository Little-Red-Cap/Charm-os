#include "host/host_world_support.hpp"
#include "profiles/profile_evidence.hpp"

#include <cstddef>
#include <cstdio>

namespace h747::host_profile_evidence {

class HostProfileWorld {
public:
    using Log = h747::host_support::BufferedLog;
    using Clock = h747::host_support::ManualClock;
    using Display = h747::host_support::MemoryRasterDisplay;
    using Input = h747::host_support::NullInput;

    HostProfileWorld() noexcept : clock_(42U), display_(framebuffer_.framebuffer()) {}

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

private:
    using FrameBuffer = h747::host_support::HostFrameBufferStorage<64U, 64U>;

    FrameBuffer framebuffer_{};
    Log log_{};
    Clock clock_{};
    Display display_;
    Input input_{};
};

static_assert(charm::cap::RasterDisplayInputWorld<HostProfileWorld>);

template <charm::cap::RasterDisplayInputWorld World>
void app_semantics_probe(World& world) noexcept {
    auto& log = world.log();
    (void)log.write("app:tick=");
    (void)log.write(world.clock().tick_ms().value == 42U ? "42" : "unexpected");
    (void)log.write("\n");

    auto framebuffer = world.framebuffer();
    if (!framebuffer.pixels().empty()) {
        framebuffer.pixels()[0] = std::byte{0x66U};
    }
    (void)world.input().sample();
    (void)world.display().present(framebuffer.view(), {});
}

[[nodiscard]] bool expect(bool condition, const char* message) noexcept {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

[[nodiscard]] constexpr bool has_field(const h747::profiles::EvidenceFrame& frame,
                                       const std::string_view key,
                                       const std::string_view value) noexcept {
    for (std::size_t i = 0U; i < frame.field_count; ++i) {
        if (frame.fields[i].key == key && frame.fields[i].value == value) {
            return true;
        }
    }
    return false;
}

} // namespace h747::host_profile_evidence

int main() {
    using namespace h747::host_profile_evidence;

    HostProfileWorld world{};
    app_semantics_probe(world);

    constexpr auto host = h747::profiles::host_player_profile_evidence();
    constexpr auto h747 = h747::profiles::h747_player_profile_evidence();

    if (!expect(world.log().view() == "app:tick=42\n", "app semantics do not depend on provider names")) return 1;
    if (!expect(world.display().present_count() == 1U, "app presents through capability world")) return 1;
    if (!expect(host.profile == "host_player", "host evidence reports profile")) return 1;
    if (!expect(h747.profile == "player", "h747 evidence reports profile")) return 1;
    if (!expect(host.board == "host_mock", "host evidence reports board")) return 1;
    if (!expect(h747.board == "h747_diy", "h747 evidence reports board")) return 1;
    if (!expect(host.bindings[0].provider == "host_memory_log", "host log provider identity stays in evidence")) return 1;
    if (!expect(host.bindings[2].provider == "host_framebuffer", "host display provider identity stays in evidence")) return 1;
    if (!expect(h747.bindings[0].provider == "h747_console", "h747 log provider identity stays in evidence")) return 1;
    if (!expect(h747.bindings[2].provider == "h747_raster_display_service", "h747 display provider identity stays in evidence")) return 1;
    if (!expect(host.bindings[2].capability == h747.bindings[2].capability, "host and h747 share display capability semantics")) return 1;
    if (!expect(host.bindings[2].provider != h747.bindings[2].provider, "profile selection changes provider identity")) return 1;
    if (!expect(host.bindings[2].fields[0].value == "host_player", "host evidence reports selected profile")) return 1;
    if (!expect(h747.bindings[2].fields[0].value == "player", "h747 evidence reports selected profile")) return 1;
    if (!expect(has_field(host.bindings[2], "mode", "180x320"), "host display evidence reports mode")) return 1;
    if (!expect(has_field(host.bindings[2], "format", "argb8888"), "host display evidence reports format")) return 1;
    if (!expect(has_field(host.bindings[2], "buffer_policy", "single_memory_framebuffer"), "host display evidence reports buffer policy")) return 1;
    if (!expect(has_field(h747.bindings[2], "mode", "720x1280"), "h747 display evidence reports mode")) return 1;
    if (!expect(has_field(h747.bindings[2], "format", "argb8888"), "h747 display evidence reports format")) return 1;
    if (!expect(has_field(h747.bindings[2], "buffer_policy", "double_buffer_vblank_reload"), "h747 display evidence reports buffer policy")) return 1;
    if (!expect(has_field(host.bindings[3], "source", "null_input"), "host input evidence reports source")) return 1;
    if (!expect(has_field(host.bindings[3], "pointer", "none"), "host input evidence reports pointer fact")) return 1;
    if (!expect(has_field(host.bindings[3], "encoders", "none"), "host input evidence reports encoder fact")) return 1;
    if (!expect(has_field(h747.bindings[3], "source", "h747_input_service"), "h747 input evidence reports source")) return 1;
    if (!expect(has_field(h747.bindings[3], "pointer", "gt9xx_best_effort"), "h747 input evidence reports pointer fact")) return 1;
    if (!expect(has_field(h747.bindings[3], "encoders", "dual_encoder"), "h747 input evidence reports encoder fact")) return 1;

    std::puts("[h747-host-profile-evidence-ci] ok=1");
    return 0;
}
