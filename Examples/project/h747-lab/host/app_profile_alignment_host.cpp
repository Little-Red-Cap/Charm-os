#include "apps/player/player_domain.hpp"
#include "host/host_world_support.hpp"
#include "profiles/profile_evidence.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace h747::host_app_profile_alignment {

class AlignmentWorld {
public:
    using Log = h747::host_support::BufferedLogStorage<256U>;
    using Clock = h747::host_support::ManualClock;
    using Display = h747::host_support::MemoryRasterDisplay;
    using Input = h747::host_support::NullInput;

    AlignmentWorld() noexcept : display_(framebuffer_.framebuffer()) {}

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

static_assert(charm::cap::RasterDisplayInputWorld<AlignmentWorld>);

struct AppRunFacts {
    std::uint32_t hash{};
    std::uint32_t presents{};
    charm::cap::DisplayMode mode{};
    charm::cap::InputFrame input{};
    std::string_view log{};
};

template <charm::cap::RasterDisplayInputWorld World>
void run_player_app(World& world) noexcept {
    h747::apps::player::PlayerRuntime runtime{};
    h747::apps::player::init(world, runtime);
    for (int i = 0; i < 3; ++i) {
        world.clock().advance(1000U);
        h747::apps::player::loop_once(world, runtime);
    }
}

[[nodiscard]] AppRunFacts collect_app_facts(AlignmentWorld& world) noexcept {
    return AppRunFacts{
        .hash = h747::host_support::fnv1a32(world.pixels()),
        .presents = world.display().present_count(),
        .mode = AlignmentWorld::mode(),
        .input = world.input().sample(),
        .log = world.log().view(),
    };
}

[[nodiscard]] constexpr std::string_view field_value(const h747::profiles::EvidenceFrame& frame,
                                                     const std::string_view key) noexcept {
    for (std::size_t i = 0U; i < frame.field_count; ++i) {
        if (frame.fields[i].key == key) {
            return frame.fields[i].value;
        }
    }
    return {};
}

[[nodiscard]] constexpr bool has_explicit_binding(const h747::profiles::EvidenceFrame& frame) noexcept {
    return field_value(frame, "selection") == "explicit_binding";
}

[[nodiscard]] bool contains(const std::string_view haystack, const std::string_view needle) noexcept {
    return haystack.find(needle) != std::string_view::npos;
}

[[nodiscard]] bool provider_names_are_evidence_only(
    const std::string_view app_log,
    const h747::profiles::ProfileEvidence& host,
    const h747::profiles::ProfileEvidence& h747) noexcept {
    for (std::size_t i = 0U; i < host.bindings.size(); ++i) {
        if (contains(app_log, host.bindings[i].provider) ||
            contains(app_log, h747.bindings[i].provider)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr bool host_display_facts_align_with_app(
    const AppRunFacts& app,
    const h747::profiles::EvidenceFrame& display) noexcept {
    return display.capability == "RasterDisplay.primary_display" &&
           has_explicit_binding(display) &&
           field_value(display, "mode") == "180x320" &&
           field_value(display, "format") == "argb8888" &&
           field_value(display, "buffer_policy") == "single_memory_framebuffer" &&
           app.mode.extent.width == 180U &&
           app.mode.extent.height == 320U &&
           app.mode.format == charm::cap::PixelFormat::argb8888;
}

[[nodiscard]] constexpr bool host_input_facts_align_with_app(
    const AppRunFacts& app,
    const h747::profiles::EvidenceFrame& input) noexcept {
    return input.capability == "Input.primary_input" &&
           has_explicit_binding(input) &&
           field_value(input, "source") == "null_input" &&
           field_value(input, "pointer") == "none" &&
           field_value(input, "encoders") == "none" &&
           app.input.encoder1.detent_delta == 0 &&
           !app.input.encoder1.pressed &&
           app.input.encoder2.detent_delta == 0 &&
           !app.input.encoder2.pressed &&
           !app.input.pointer.detected &&
           !app.input.pointer.down;
}

[[nodiscard]] constexpr bool h747_facts_share_app_semantics(
    const h747::profiles::ProfileEvidence& host,
    const h747::profiles::ProfileEvidence& h747) noexcept {
    const auto& host_display = host.bindings[2];
    const auto& h747_display = h747.bindings[2];
    const auto& host_input = host.bindings[3];
    const auto& h747_input = h747.bindings[3];

    return host_display.capability == h747_display.capability &&
           host_input.capability == h747_input.capability &&
           host_display.provider != h747_display.provider &&
           host_input.provider != h747_input.provider &&
           has_explicit_binding(h747_display) &&
           has_explicit_binding(h747_input) &&
           field_value(h747_display, "mode") == "720x1280" &&
           field_value(h747_display, "format") == field_value(host_display, "format") &&
           field_value(h747_display, "buffer_policy") == "double_buffer_vblank_reload" &&
           field_value(h747_input, "source") == "h747_input_service" &&
           field_value(h747_input, "pointer") != field_value(host_input, "pointer") &&
           field_value(h747_input, "encoders") != field_value(host_input, "encoders");
}

[[nodiscard]] bool expect(const bool condition, const char* message) noexcept {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

} // namespace h747::host_app_profile_alignment

int main() {
    using namespace h747::host_app_profile_alignment;

    constexpr std::uint32_t kExpectedHash = 0x650DDD82U;
    constexpr std::uint32_t kExpectedPresents = 4U;

    AlignmentWorld world{};
    run_player_app(world);
    const auto app = collect_app_facts(world);

    constexpr auto host = h747::profiles::host_player_profile_evidence();
    constexpr auto h747 = h747::profiles::h747_player_profile_evidence();

    static_assert(host.bindings[0].capability == "TextSink.log");
    static_assert(host.bindings[1].capability == "Clock.monotonic_time");
    static_assert(host.bindings[2].capability == "RasterDisplay.primary_display");
    static_assert(host.bindings[3].capability == "Input.primary_input");
    static_assert(h747_facts_share_app_semantics(host, h747));

    if (!expect(app.hash == kExpectedHash, "player app framebuffer hash changed")) return 1;
    if (!expect(app.presents == kExpectedPresents, "player app present count changed")) return 1;
    if (!expect(host.profile == "host_player", "host evidence profile changed")) return 1;
    if (!expect(host.board == "host_mock", "host evidence board changed")) return 1;
    if (!expect(host_display_facts_align_with_app(app, host.bindings[2]),
                "host display evidence no longer explains app display facts")) return 1;
    if (!expect(host_input_facts_align_with_app(app, host.bindings[3]),
                "host input evidence no longer explains app input facts")) return 1;
    if (!expect(h747_facts_share_app_semantics(host, h747),
                "h747 evidence no longer shares app capability semantics")) return 1;
    if (!expect(provider_names_are_evidence_only(app.log, host, h747),
                "provider identity leaked into app log/runtime surface")) return 1;

    std::printf("[h747-host-app-profile-alignment-ci] ok=1 hash=0x%08X presents=%u mode=%ux%u format=argb8888\n",
                static_cast<unsigned>(app.hash),
                static_cast<unsigned>(app.presents),
                static_cast<unsigned>(app.mode.extent.width),
                static_cast<unsigned>(app.mode.extent.height));
    return 0;
}
