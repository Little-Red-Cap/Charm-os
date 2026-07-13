#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <span>
#include <string_view>

import audio.player;
import audio.source.fs;
import charm.system.clock;
import fs_core;
import fs_errno;
import fs_ramfs;
import fs_stream;
import fs_vfs;
import player.controller;
import player.fixed_string;
import player.storage;
import player.track_probe;
import util.core;

namespace {
    template <util::usize BlockSize, util::usize MaxFiles, util::usize MaxBlocks>
    struct RamFsMount {
        fs::RamFs<BlockSize, MaxFiles, MaxBlocks> fs{};
        fs::Mount mount{};

        RamFsMount() noexcept {
            mount.ops = &ops_;
            mount.data = this;
        }

        fs::Mount* mount_point() noexcept { return &mount; }

        static fs::Status open_impl(fs::Mount* m,
                                    std::string_view path,
                                    fs::File& out,
                                    fs::OpenFlags flags) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.open(path, out, flags);
        }

        static fs::Status flush_impl(fs::Mount*) noexcept {
            return fs::Status{fs::Errc::ok};
        }

        static fs::Status unmount_impl(fs::Mount*, bool) noexcept {
            return fs::Status{fs::Errc::ok};
        }

        static fs::Status unlink_impl(fs::Mount* m, std::string_view path) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.unlink(path);
        }

        static fs::Status rename_impl(fs::Mount* m,
                                      std::string_view from,
                                      std::string_view to) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.rename(from, to);
        }

        static fs::Status truncate_impl(fs::Mount* m,
                                        std::string_view path,
                                        util::u64 size) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.truncate(path, size);
        }

        static fs::Status mkdir_impl(fs::Mount* m, std::string_view path) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.mkdir(path);
        }

        static fs::Status list_impl(fs::Mount* m,
                                    std::string_view path,
                                    void* ctx,
                                    fs::MountOps::ListFn fn) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.list(path, ctx, fn);
        }

        static fs::MountOps ops_;
    };

    template <util::usize BlockSize, util::usize MaxFiles, util::usize MaxBlocks>
    fs::MountOps RamFsMount<BlockSize, MaxFiles, MaxBlocks>::ops_{
        .open = &RamFsMount::open_impl,
        .flush = &RamFsMount::flush_impl,
        .unmount = &RamFsMount::unmount_impl,
        .unlink = &RamFsMount::unlink_impl,
        .rename = &RamFsMount::rename_impl,
        .truncate = &RamFsMount::truncate_impl,
        .mkdir = &RamFsMount::mkdir_impl,
        .list = &RamFsMount::list_impl,
    };

    using SmokeFs = RamFsMount<512, 16, 2048>;

    charm::system::ClockTick smoke_now_us(void*) noexcept {
        using clock = std::chrono::steady_clock;
        const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(
            clock::now().time_since_epoch());
        return static_cast<charm::system::ClockTick>(ticks.count());
    }

    bool expect(bool condition, const char* message) {
        if (!condition) {
            std::printf("[player-controller-playback-smoke] fail: %s\n", message);
            return false;
        }
        return true;
    }

    bool write_bytes(fs::File& file, const void* data, std::size_t size) {
        auto st = fs::write(file, std::span<const util::u8>(
            static_cast<const util::u8*>(data),
            size));
        return static_cast<bool>(st);
    }

    bool write_fourcc(fs::File& file, const char (&text)[5]) {
        return write_bytes(file, text, 4);
    }

    bool write_u16_le(fs::File& file, std::uint16_t value) {
        const std::array<unsigned char, 2> bytes{
            static_cast<unsigned char>(value & 0xFFu),
            static_cast<unsigned char>((value >> 8) & 0xFFu),
        };
        return write_bytes(file, bytes.data(), bytes.size());
    }

    bool write_u32_le(fs::File& file, std::uint32_t value) {
        const std::array<unsigned char, 4> bytes{
            static_cast<unsigned char>(value & 0xFFu),
            static_cast<unsigned char>((value >> 8) & 0xFFu),
            static_cast<unsigned char>((value >> 16) & 0xFFu),
            static_cast<unsigned char>((value >> 24) & 0xFFu),
        };
        return write_bytes(file, bytes.data(), bytes.size());
    }

    bool write_test_wav(std::string_view path, std::uint32_t seconds) {
        fs::File file{};
        auto st = fs::vfs_open(path, file, static_cast<fs::OpenFlags>(
            static_cast<unsigned>(fs::OpenFlags::write)
            | static_cast<unsigned>(fs::OpenFlags::create)
            | static_cast<unsigned>(fs::OpenFlags::trunc)));
        if (!st) {
            return false;
        }

        constexpr std::uint32_t sample_rate = 48000;
        constexpr std::uint16_t channels = 2;
        constexpr std::uint16_t bits_per_sample = 16;
        constexpr std::uint16_t block_align = channels * (bits_per_sample / 8);
        const std::uint32_t frame_count = sample_rate * seconds;
        const std::uint32_t data_size = frame_count * block_align;
        const std::uint32_t riff_size = 36 + data_size;

        bool ok = true;
        ok = ok && write_fourcc(file, "RIFF");
        ok = ok && write_u32_le(file, riff_size);
        ok = ok && write_fourcc(file, "WAVE");
        ok = ok && write_fourcc(file, "fmt ");
        ok = ok && write_u32_le(file, 16);
        ok = ok && write_u16_le(file, 1);
        ok = ok && write_u16_le(file, channels);
        ok = ok && write_u32_le(file, sample_rate);
        ok = ok && write_u32_le(file, sample_rate * block_align);
        ok = ok && write_u16_le(file, block_align);
        ok = ok && write_u16_le(file, bits_per_sample);
        ok = ok && write_fourcc(file, "data");
        ok = ok && write_u32_le(file, data_size);

        const std::array<unsigned char, 512> silence{};
        std::uint32_t remaining = data_size;
        while (ok && remaining > 0) {
            const std::size_t chunk = (remaining < silence.size())
                ? static_cast<std::size_t>(remaining)
                : silence.size();
            ok = write_bytes(file, silence.data(), chunk);
            remaining -= static_cast<std::uint32_t>(chunk);
        }

        return ok && static_cast<bool>(fs::vfs_close(file));
    }

    bool tick_audio_until(audio::AudioPlayer& player,
                          audio::PlayerState a,
                          audio::PlayerState b,
                          int max_ticks = 64) {
        for (int i = 0; i < max_ticks; ++i) {
            player.tick();
            const auto state = player.state();
            if (state == audio::PlayerState::error) {
                return false;
            }
            if (state == a || state == b) {
                return true;
            }
        }
        return false;
    }

    bool tick_audio_to_idle(audio::AudioPlayer& player, int max_ticks = 4096) {
        for (int i = 0; i < max_ticks; ++i) {
            player.tick();
            const auto state = player.state();
            if (state == audio::PlayerState::error) {
                return false;
            }
            if (state == audio::PlayerState::idle) {
                return true;
            }
        }
        return false;
    }

    bool run_smoke() {
        static SmokeFs ramfs{};
        fs::clear_mounts();
        if (!expect(static_cast<bool>(fs::add_mount("/", ramfs.mount_point())), "mount ramfs")) {
            return false;
        }
        if (!expect(static_cast<bool>(fs::vfs_mkdir("/music")), "create music dir")) {
            return false;
        }
        if (!expect(write_test_wav("/music/first.wav", 2), "write first wav")) {
            return false;
        }
        if (!expect(write_test_wav("/music/second.wav", 2), "write second wav")) {
            return false;
        }
        charm::system::Clock clock{nullptr, {.now_us = &smoke_now_us}};
        charm::system::ClockCaps::TimeSource::bind(clock);

        audio::PlayerConfig cfg{};
        cfg.capture_output = false;
        audio::FsDataSource audio_source{};
        audio::FsDataSource probe_source{};
        audio::PumpedNullAudioSink audio_sink{};
        audio::AudioPlayer audio_player{
            cfg,
            audio::PlayerBindings{
                .source = audio::make_audio_source_binding(audio_source),
                .sink = audio::make_audio_sink_binding(audio_sink),
            },
            clock};

        int probed_seconds = 0;
        const auto probe_binding = audio::make_audio_source_binding(probe_source);
        const bool probe_ok = player::probe_duration_seconds(
            probe_binding, "/music/first.wav", probed_seconds);
        if (!expect(probe_ok && probed_seconds == 2, "probe first wav duration")) {
            std::printf("[player-controller-playback-smoke] probe ok=%d seconds=%d\n",
                        probe_ok ? 1 : 0,
                        probed_seconds);
            return false;
        }

        player::StorageState storage{};
        storage.fs_ready = true;
        storage.has_tracks = true;
        storage.status.assign("Ready");
        storage.mount_status.assign("Ready");
        player::FixedString<260> first{};
        first.assign("/music/first.wav");
        player::FixedString<260> second{};
        second.assign("/music/second.wav");
        if (!expect(storage.tracks.push_back(first)
                    && storage.tracks.push_back(second),
                    "populate storage tracks")) {
            return false;
        }
        player::ensure_track_labels(storage);

        player::PlayerController controller{};
        controller.bind_track_probe_source(probe_binding);
        controller.bind_player(audio_player);
        controller.apply_storage_view(player::make_storage_view(storage), false);
        if (!expect(controller.load_track_index(0), "controller loads first track")) {
            return false;
        }
        if (!expect(controller.track_ready()
                    && controller.playback.track_ready()
                    && controller.playback.duration_ready()
                    && controller.playback.duration_sec() == 2,
                    "controller preload has ready duration")) {
            return false;
        }

        controller.start_playback();
        if (!expect(controller.is_playing()
                    && controller.last_status_text.view() == "Opening",
                    "controller start enters opening state")) {
            return false;
        }
        if (!expect(tick_audio_until(audio_player,
                                     audio::PlayerState::buffering,
                                     audio::PlayerState::playing),
                    "audio reaches buffering or playing")) {
            return false;
        }
        controller.tick_player(audio_player);
        if (!expect(controller.last_status_text.view() == "Buffering"
                    || controller.last_status_text.view() == "Playing",
                    "controller receives runtime status")) {
            return false;
        }
        if (!expect(audio_player.total_frames() == 96000, "audio reports wav frames")) {
            return false;
        }

        controller.apply_seek_pending_action(1);
        audio_player.tick();
        if (!expect(audio_player.state() != audio::PlayerState::error
                    && controller.playback.current_sec() == 1,
                    "controller seek drives audio player")) {
            return false;
        }

        controller.pause_playback();
        if (!expect(tick_audio_until(audio_player,
                                     audio::PlayerState::paused,
                                     audio::PlayerState::paused),
                    "audio pauses")) {
            return false;
        }
        controller.tick_player(audio_player);
        if (!expect(controller.is_paused()
                    && controller.last_status_text.view() == "Paused",
                    "controller remains paused")) {
            return false;
        }

        controller.resume_playback();
        if (!expect(tick_audio_until(audio_player,
                                     audio::PlayerState::buffering,
                                     audio::PlayerState::playing),
                    "audio resumes")) {
            return false;
        }
        controller.tick_player(audio_player);

        const int expected_next = controller.playback.queue_track_at(1);
        if (!expect(tick_audio_to_idle(audio_player), "audio reaches natural idle")) {
            return false;
        }
        controller.tick_player(audio_player);
        if (!expect(controller.track_index == expected_next
                    && controller.is_playing()
                    && controller.playback.track_ready(),
                    "controller advances on player run-state drop")) {
            std::printf("[player-controller-playback-smoke] advance track=%d expected=%d "
                        "playing=%d paused=%d ready=%d audio_state=%d queue=%d/%d gen=%u status=%s\n",
                        controller.track_index,
                        expected_next,
                        controller.is_playing() ? 1 : 0,
                        controller.is_paused() ? 1 : 0,
                        controller.playback.track_ready() ? 1 : 0,
                        static_cast<int>(audio_player.state()),
                        controller.playback.queue_position(),
                        controller.playback.queue_count(),
                        static_cast<unsigned>(controller.playback.queue_generation()),
                        controller.last_status_text.c_str());
            return false;
        }

        controller.stop_playback();
        audio_player.tick();
        if (!expect(audio_player.state() == audio::PlayerState::idle
                    && !controller.is_playing()
                    && !controller.is_paused(),
                    "controller stop returns audio idle")) {
            return false;
        }

        fs::clear_mounts();
        return true;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::printf("[player-controller-playback-smoke] ok\n");
    return 0;
}
