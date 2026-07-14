#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

import audio.player;
import audio.result;
import charm.core.config;
import charm.gfx.color;
import charm.system.clock;
import input.raw_event;
import media.stream.sink;
import media.stream.source;
import media.stream.types;
import player.cover_resource;
import player.md3_port;
import player.port;
import player.port_runtime;
import player.raster;
import player.storage;
import fs_core;
import fs_errno;
import fs_stream;

namespace {
    struct MountProbe {
        const char* expected_path{nullptr};
        std::size_t calls{0};
    };

    fs::Status mount_probe(void* ctx, const char* path) {
        auto& probe = *static_cast<MountProbe*>(ctx);
        ++probe.calls;
        return std::string_view{path ? path : ""} == probe.expected_path
            ? fs::Status{fs::Errc::nosys}
            : fs::Status{fs::Errc::inval};
    }

    struct AudioProbe {
        std::size_t clock_bind_count{0};
        std::size_t source_open_count{0};
        std::size_t source_close_count{0};
        std::size_t sink_open_count{0};
        std::size_t sink_start_count{0};
        std::size_t sink_stop_count{0};
        std::size_t sink_close_count{0};

        [[nodiscard]] audio::PlayerBindings bindings() noexcept {
            return audio::PlayerBindings{
                .source = {
                    .ctx = this,
                    .open_fn = [](void* ctx, const char*) noexcept
                        -> audio::Result<media::StreamSourceRef> {
                        ++static_cast<AudioProbe*>(ctx)->source_open_count;
                        return audio::unexpected(audio::Errc::not_supported);
                    },
                    .close_fn = [](void* ctx) noexcept {
                        ++static_cast<AudioProbe*>(ctx)->source_close_count;
                    },
                },
                .sink = {
                    .ctx = this,
                    .set_clock_fn = [](void* ctx, charm::system::Clock&) noexcept {
                        ++static_cast<AudioProbe*>(ctx)->clock_bind_count;
                    },
                    .open_fn = [](void* ctx, const audio::SinkConfig&) noexcept
                        -> audio::Result<void> {
                        ++static_cast<AudioProbe*>(ctx)->sink_open_count;
                        return {};
                    },
                    .start_fn = [](void* ctx) noexcept -> audio::Result<void> {
                        ++static_cast<AudioProbe*>(ctx)->sink_start_count;
                        return {};
                    },
                    .stop_fn = [](void* ctx) noexcept -> audio::Result<void> {
                        ++static_cast<AudioProbe*>(ctx)->sink_stop_count;
                        return {};
                    },
                    .close_fn = [](void* ctx) noexcept {
                        ++static_cast<AudioProbe*>(ctx)->sink_close_count;
                    },
                    .set_fill_callback_fn = [](void*, media::FillCallback, void*) noexcept {},
                    .format_fn = [](void*) noexcept -> media::StreamFormat { return {}; },
                    .period_frames_fn = [](void*) noexcept -> std::uint32_t { return 0; },
                    .underrun_count_fn = [](void*) noexcept -> std::uint64_t { return 0; },
                    .consume_underrun_fn = [](void*) noexcept -> bool { return false; },
                    .clear_underrun_fn = [](void*) noexcept {},
                    .callback_stats_fn = [](void*) noexcept -> audio::CallbackStats { return {}; },
                    .pump_fn = nullptr,
                },
            };
        }
    };

    struct ClockProbe {
        player::PlayerClockTick now_us{0};

        static player::PlayerClockTick read(void* ctx) noexcept {
            return static_cast<ClockProbe*>(ctx)->now_us;
        }
    };

    player::PlayerInputPollResult poll_empty(
        void*, input::RawInputEvent&) noexcept {
        return player::PlayerInputPollResult::empty;
    }

    bool expect(bool value, const char* message) {
        if (!value) std::printf("[player-instance-isolation-smoke] fail: %s\n", message);
        return value;
    }
}

int main() {
    static MountProbe first_mount{"first"};
    static MountProbe second_mount{"second"};
    static AudioProbe first_audio{};
    static AudioProbe second_audio{};

    static constexpr std::array<std::uint32_t, 1> first_pixels{0xff112233u};
    static constexpr std::array<std::uint32_t, 1> second_pixels{0xff445566u};
    static const std::array<player::PlayerCoverResourceRecord, 1> first_records{{
        {"/first", player::CoverResourceKind::FolderFile, "first-cover", first_pixels, 1, 1},
    }};
    static const std::array<player::PlayerCoverResourceRecord, 1> second_records{{
        {"/second", player::CoverResourceKind::FolderFile, "second-cover", second_pixels, 1, 1},
    }};
    static const player::PlayerCoverResourceRecordTableView first_table{first_records};
    static const player::PlayerCoverResourceRecordTableView second_table{second_records};
    const auto first_cover = player::make_cover_resource_record_table_binding(first_table);
    const auto second_cover = player::make_cover_resource_record_table_binding(second_table);
    player::CoverResourceView first_view{};
    player::CoverResourceView second_view{};
    if (!expect(player::resolve_cover_resource(
                    first_cover, {"/first", player::CoverResourceKind::FolderFile, {}}, first_view),
                "first cover resolves")
        || !expect(player::resolve_cover_resource(
                    second_cover, {"/second", player::CoverResourceKind::FolderFile, {}}, second_view),
                "second cover resolves")
        || !expect(first_view.key == "first-cover" && second_view.key == "second-cover",
                   "cover results remain instance-local")
        || !expect(!player::resolve_cover_resource(
                    first_cover, {"/second", player::CoverResourceKind::FolderFile, {}}, first_view),
                   "cross-instance cover lookup rejected")) {
        return 1;
    }

    constexpr std::size_t kPixelBytes = static_cast<std::size_t>(screen_width)
        * static_cast<std::size_t>(screen_height) * 4u;
    static std::array<std::byte, kPixelBytes> first_raster{};
    static std::array<std::byte, kPixelBytes> second_raster{};
    ClockProbe first_clock{};
    ClockProbe second_clock{};
    player::PlayerMemoryRasterDisplayState first_display{};
    player::PlayerMemoryRasterDisplayState second_display{};

    player::PlayerMd3RuntimeConfig<player::PlayerPage> first_config{
        .audio_bindings = first_audio.bindings(),
        .storage_binding = {&first_mount, &mount_probe, "first"},
        .cover_resource_provider = first_cover,
        .start_page = player::PlayerPage::Home,
        .clear_color = rgba{9, 12, 16, 255},
    };
    player::PlayerMd3RuntimeConfig<player::PlayerPage> second_config{
        .audio_bindings = second_audio.bindings(),
        .storage_binding = {&second_mount, &mount_probe, "second"},
        .cover_resource_provider = second_cover,
        .start_page = player::PlayerPage::Library,
        .clear_color = rgba{16, 12, 9, 255},
    };
    static player::PlayerMd3PortApplication first_app{first_config};
    static player::PlayerMd3PortApplication second_app{second_config};

    const player::PlayerPort first_port{
        .clock = {&first_clock, &ClockProbe::read},
        .raster_surface = {first_raster,
                           screen_width,
                           screen_height,
                           static_cast<std::size_t>(screen_width) * 4u,
                           player::PlayerRasterPixelFormat::ARGB8888},
        .raster_display = player::make_player_memory_raster_display(first_display),
        .raw_input = {nullptr, &poll_empty},
    };
    const player::PlayerPort second_port{
        .clock = {&second_clock, &ClockProbe::read},
        .raster_surface = {second_raster,
                           screen_width,
                           screen_height,
                           static_cast<std::size_t>(screen_width) * 4u,
                           player::PlayerRasterPixelFormat::ARGB8888},
        .raster_display = player::make_player_memory_raster_display(second_display),
        .raw_input = {nullptr, &poll_empty},
    };
    player::PlayerPortRuntime first_runtime{first_port, first_app.endpoint()};
    player::PlayerPortRuntime second_runtime{second_port, second_app.endpoint()};

    if (!expect(first_runtime.bootstrap() && second_runtime.bootstrap(),
                "two complete Player apps bootstrap")
        || !expect(first_app.ready() && second_app.ready(),
                   "both Player apps remain materialized")
        || !expect(first_mount.calls == 1 && second_mount.calls == 1,
                   "each complete app scans its storage once")
        || !expect(first_audio.clock_bind_count == 1 && second_audio.clock_bind_count == 1,
                   "audio bindings remain instance-local")) {
        return 1;
    }

    if (!expect(first_runtime.frame(1000, 0), "first app initial frame")
        || !expect(second_runtime.frame(2000, 0), "second app initial frame")
        || !expect(first_app.set_page(player::PlayerPage::NowPlaying),
                   "first app page changes independently")
        || !expect(first_runtime.frame(3000, 2000), "first app interleaved frame")
        || !expect(second_runtime.frame(4000, 2000), "second app interleaved frame")
        || !expect(first_display.present_count == 2 && second_display.present_count == 2,
                   "display counters remain instance-local")
        || !expect(first_mount.calls == 1 && second_mount.calls == 1,
                   "interleaved frames do not rescan storage")) {
        return 1;
    }

    first_runtime.shutdown();
    if (!expect(!first_app.ready() && second_app.ready(),
                "shutting down one app leaves the other alive")
        || !expect(second_runtime.frame(6000, 2000),
                   "second app continues after first shutdown")
        || !expect(second_display.present_count == 3,
                   "surviving app continues presenting")) {
        return 1;
    }
    second_runtime.shutdown();
    if (!expect(!second_app.ready(), "second app shuts down independently")
        || !expect(first_audio.source_open_count == 0 && second_audio.source_open_count == 0
                       && first_audio.sink_open_count == 0 && second_audio.sink_open_count == 0,
                   "idle apps do not acquire media resources")) {
        return 1;
    }

    std::puts("[player-instance-isolation-smoke] ok");
    return 0;
}
