#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>

import audio.player;
import charm.system.clock;
import player.fixed_string;
import player.playback;
import player.playback_session;

namespace {
    charm::system::ClockTick smoke_now_us(void*) noexcept {
        using clock = std::chrono::steady_clock;
        const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(
            clock::now().time_since_epoch());
        return static_cast<charm::system::ClockTick>(ticks.count());
    }

    bool expect(bool condition, const char* message) {
        if (!condition) {
            std::printf("[player-playback-engine-smoke] fail: %s\n", message);
            return false;
        }
        return true;
    }

    bool write_bytes(std::FILE* file, const void* data, std::size_t size) {
        return std::fwrite(data, 1, size, file) == size;
    }

    bool write_fourcc(std::FILE* file, const char (&text)[5]) {
        return write_bytes(file, text, 4);
    }

    bool write_u16_le(std::FILE* file, std::uint16_t value) {
        const std::array<unsigned char, 2> bytes{
            static_cast<unsigned char>(value & 0xFFu),
            static_cast<unsigned char>((value >> 8) & 0xFFu),
        };
        return write_bytes(file, bytes.data(), bytes.size());
    }

    bool write_u32_le(std::FILE* file, std::uint32_t value) {
        const std::array<unsigned char, 4> bytes{
            static_cast<unsigned char>(value & 0xFFu),
            static_cast<unsigned char>((value >> 8) & 0xFFu),
            static_cast<unsigned char>((value >> 16) & 0xFFu),
            static_cast<unsigned char>((value >> 24) & 0xFFu),
        };
        return write_bytes(file, bytes.data(), bytes.size());
    }

    std::FILE* open_binary_write(const char* path) noexcept {
#if defined(_MSC_VER)
        std::FILE* file = nullptr;
        if (fopen_s(&file, path, "wb") != 0) {
            return nullptr;
        }
        return file;
#else
        return std::fopen(path, "wb");
#endif
    }

    bool write_test_wav(const char* path) {
        std::FILE* file = open_binary_write(path);
        if (!file) {
            return false;
        }

        constexpr std::uint32_t sample_rate = 48000;
        constexpr std::uint16_t channels = 2;
        constexpr std::uint16_t bits_per_sample = 16;
        constexpr std::uint16_t block_align = channels * (bits_per_sample / 8);
        constexpr std::uint32_t seconds = 2;
        constexpr std::uint32_t frame_count = sample_rate * seconds;
        constexpr std::uint32_t data_size = frame_count * block_align;
        constexpr std::uint32_t riff_size = 36 + data_size;

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

        ok = ok && (std::fclose(file) == 0);
        return ok;
    }

    bool tick_until(audio::AudioPlayer& player,
                    audio::PlayerState expected_a,
                    audio::PlayerState expected_b,
                    int max_ticks = 64) {
        for (int i = 0; i < max_ticks; ++i) {
            player.tick();
            const auto state = player.state();
            if (state == audio::PlayerState::error) {
                return false;
            }
            if (state == expected_a || state == expected_b) {
                return true;
            }
        }
        return false;
    }

    bool run_smoke() {
        constexpr const char* wav_path = "player_playback_engine_smoke.wav";
        constexpr const char* next_wav_path = "player_playback_engine_smoke_next.wav";
        constexpr const char* third_wav_path = "player_playback_engine_smoke_third.wav";
        if (!expect(write_test_wav(wav_path), "create wav fixture")) {
            return false;
        }
        if (!expect(write_test_wav(next_wav_path), "create next wav fixture")) {
            return false;
        }
        if (!expect(write_test_wav(third_wav_path), "create third wav fixture")) {
            return false;
        }

        charm::system::Clock clock{nullptr, {.now_us = &smoke_now_us}};
        charm::system::ClockCaps::TimeSource::bind(clock);

        audio::PlayerConfig config{};
        config.capture_output = false;
        audio::AudioPlayer audio_player(config, clock);

        player::PlaybackEngine engine{};
        player::FixedString<128> status{};

        if (!expect(!engine.apply_action(player::PlaybackAction::start, 0, status),
                    "start without player must fail")) {
            return false;
        }
        if (!expect(status.view() == "No player", "no-player status")) {
            return false;
        }

        engine.set_player(audio_player);
        if (!expect(!engine.apply_action(player::PlaybackAction::start, 0, status),
                    "start without track must fail")) {
            return false;
        }
        if (!expect(status.view() == "No track", "no-track status")) {
            return false;
        }

        engine.set_track_path(wav_path);
        engine.set_track_ready(false);
        if (!expect(!engine.apply_action(player::PlaybackAction::start, 0, status),
                    "start with not-ready track must fail")) {
            return false;
        }
        if (!expect(status.view() == "Track not ready", "not-ready status")) {
            return false;
        }

        engine.set_track_ready(true);
        if (!expect(engine.apply_action(player::PlaybackAction::start, 0, status),
                    "start ready track")) {
            return false;
        }
        if (!expect(status.view() == "Opening", "start status")) {
            return false;
        }
        if (!expect(engine.playing() && !engine.paused(), "engine playing after start")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::buffering,
                               audio::PlayerState::playing),
                    "audio reaches buffering/playing")) {
            return false;
        }
        if (!expect(audio_player.total_frames() == 96000, "wav total frames")) {
            return false;
        }
        if (!expect(engine.update_duration_from_player(), "duration from player")) {
            return false;
        }
        if (!expect(engine.duration_ready() && engine.duration_sec() == 2, "duration evidence")) {
            return false;
        }
        engine.set_current_sec(9);
        if (!expect(engine.current_sec() == 2, "engine current clamps to duration")) {
            return false;
        }
        engine.set_current_sec(-3);
        if (!expect(engine.current_sec() == 0, "engine current clamps negative position")) {
            return false;
        }
        if (!expect(!engine.apply_action(player::PlaybackAction::seek, -1, status),
                    "negative seek rejected")) {
            return false;
        }
        if (!expect(status.view() == "Seek target invalid", "negative seek status")) {
            return false;
        }
        if (!expect(engine.current_sec() == 0, "negative seek does not move current")) {
            return false;
        }
        if (!expect(engine.apply_action(player::PlaybackAction::seek, 99, status),
                    "seek beyond duration clamps")) {
            return false;
        }
        audio_player.tick();
        if (!expect(audio_player.state() != audio::PlayerState::error, "clamped seek does not error")) {
            return false;
        }
        if (!expect(engine.current_sec() == 2, "seek beyond duration updates clamped current")) {
            return false;
        }

        if (!expect(engine.apply_action(player::PlaybackAction::pause, 0, status), "pause")) {
            return false;
        }
        if (!expect(engine.paused() && !engine.playing(), "engine paused")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::paused,
                               audio::PlayerState::paused),
                    "audio paused")) {
            return false;
        }

        if (!expect(engine.apply_action(player::PlaybackAction::seek, 1, status), "seek while paused")) {
            return false;
        }
        audio_player.tick();
        if (!expect(audio_player.state() != audio::PlayerState::error, "seek does not error")) {
            return false;
        }
        if (!expect(engine.current_sec() == 1, "seek updates engine position")) {
            return false;
        }

        if (!expect(engine.apply_action(player::PlaybackAction::resume, 0, status), "resume")) {
            return false;
        }
        if (!expect(engine.playing() && !engine.paused(), "engine playing after resume")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::buffering,
                               audio::PlayerState::playing),
                    "audio resumes")) {
            return false;
        }

        if (!expect(engine.apply_action(player::PlaybackAction::toggle, 0, status), "toggle pauses")) {
            return false;
        }
        if (!expect(engine.paused() && !engine.playing(), "toggle pause state")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::paused,
                               audio::PlayerState::paused),
                    "audio paused after toggle")) {
            return false;
        }

        if (!expect(engine.apply_action(player::PlaybackAction::toggle, 0, status), "toggle resumes")) {
            return false;
        }
        if (!expect(engine.playing() && !engine.paused(), "toggle resume state")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::buffering,
                               audio::PlayerState::playing),
                    "audio resumed after toggle")) {
            return false;
        }

        if (!expect(engine.apply_action(player::PlaybackAction::stop, 0, status), "stop")) {
            return false;
        }
        audio_player.tick();
        if (!expect(!engine.playing() && !engine.paused(), "engine stopped")) {
            return false;
        }
        if (!expect(audio_player.state() == audio::PlayerState::idle, "audio idle after stop")) {
            return false;
        }

        player::PlaybackSession<4> catalog_session{};
        if (!expect(catalog_session.set_track_catalog_size(3), "session catalog size")) {
            return false;
        }
        if (!expect(!catalog_session.set_track_catalog_size(5), "session rejects oversized catalog")) {
            return false;
        }
        if (!expect(catalog_session.set_track_slot(2, third_wav_path, true),
                    "session set sparse track slot")) {
            return false;
        }
        if (!expect(catalog_session.load_track_index(2, &status),
                    "session loads catalog slot")) {
            return false;
        }
        if (!expect(catalog_session.rebuild_full_queue(2), "session catalog full queue")) {
            return false;
        }
        auto catalog_snap = catalog_session.snapshot();
        if (!expect(catalog_snap.track_index == 2
                    && catalog_snap.track_count == 3
                    && catalog_snap.queue_count == 3
                    && catalog_snap.queue_position == 2,
                    "session catalog snapshot")) {
            return false;
        }
        if (!expect(catalog_session.queue_count() == 3
                    && catalog_session.queue_position() == 2
                    && catalog_session.queue_generation() == catalog_snap.queue_generation,
                    "session catalog queue accessors")) {
            return false;
        }
        catalog_session.set_duration_from_probe(42);
        if (!expect(catalog_session.resolved_duration_sec() == 42,
                    "session resolved duration from probe")) {
            return false;
        }
        catalog_session.set_current_sec(17);
        catalog_session.set_duration_from_probe_preserving_current(60);
        if (!expect(catalog_session.resolved_duration_sec() == 60
                    && catalog_session.current_sec() == 17,
                    "session duration update preserves current second")) {
            return false;
        }
        catalog_session.set_current_sec(999);
        if (!expect(catalog_session.current_sec() == 60,
                    "session current clamps to known duration")) {
            return false;
        }
        catalog_session.set_current_sec(-5);
        if (!expect(catalog_session.current_sec() == 0,
                    "session current clamps negative position")) {
            return false;
        }
        catalog_session.set_current_sec(55);
        catalog_session.set_duration_from_probe_preserving_current(20);
        if (!expect(catalog_session.resolved_duration_sec() == 20
                    && catalog_session.current_sec() == 20,
                    "session duration shrink clamps current second")) {
            return false;
        }
        catalog_session.reset_duration();
        if (!expect(!catalog_session.duration_ready()
                    && catalog_session.current_sec() == 0,
                    "session duration reset clears current second")) {
            return false;
        }
        catalog_session.clear_loaded_track();
        auto cleared_catalog_snap = catalog_session.snapshot();
        if (!expect(cleared_catalog_snap.track_index == -1
                    && cleared_catalog_snap.track_count == 3
                    && cleared_catalog_snap.queue_count == 3
                    && cleared_catalog_snap.queue_position == -1
                    && !cleared_catalog_snap.track_ready
                    && catalog_session.resolved_duration_sec() == 0
                    && catalog_session.current_sec() == 0,
                    "session clear loaded track preserves catalog queue")) {
            return false;
        }
        player::PlaybackSession<2> unavailable_session{};
        if (!expect(unavailable_session.set_track_slot(0, wav_path, false),
                    "session set unavailable track slot")) {
            return false;
        }
        (void)unavailable_session.load_track_index(0, &status);
        unavailable_session.set_duration_from_probe(42);
        if (!expect(unavailable_session.resolved_duration_sec() == 0,
                    "session hides duration for unavailable track")) {
            return false;
        }
        player::PlaybackSession<2> empty_slot_session{};
        if (!expect(empty_slot_session.set_track_slot(0, "", true),
                    "session accepts empty catalog slot")) {
            return false;
        }
        if (!expect(!empty_slot_session.load_track_index(0, &status),
                    "session rejects empty catalog slot")) {
            return false;
        }
        if (!expect(status.view() == "No track", "session empty slot status")) {
            return false;
        }
        auto empty_slot_snap = empty_slot_session.snapshot();
        if (!expect(empty_slot_snap.track_index == -1
                    && !empty_slot_snap.track_ready
                    && empty_slot_snap.track_path == nullptr,
                    "session empty slot does not become current track")) {
            return false;
        }
        player::PlaybackSession<4> shrink_session{};
        if (!expect(shrink_session.set_track_slot(2, third_wav_path, true),
                    "session shrink fixture slot")) {
            return false;
        }
        if (!expect(shrink_session.load_track_index(2, &status),
                    "session shrink loads current track")) {
            return false;
        }
        shrink_session.set_duration_from_probe(71);
        shrink_session.set_current_sec(19);
        if (!expect(shrink_session.set_track_catalog_size(1),
                    "session shrink catalog")) {
            return false;
        }
        auto shrink_snap = shrink_session.snapshot();
        if (!expect(shrink_snap.track_index == -1
                    && shrink_snap.track_count == 1
                    && shrink_snap.queue_count == 0
                    && shrink_snap.queue_position == -1
                    && !shrink_snap.track_ready
                    && shrink_snap.track_path == nullptr
                    && shrink_session.resolved_duration_sec() == 0
                    && shrink_session.current_sec() == 0,
                    "session shrink clears invalid current track state")) {
            return false;
        }

        player::PlaybackSession<4> session{};
        session.bind_player(audio_player);
        if (!expect(!session.start(status), "session start without track must fail")) {
            return false;
        }
        if (!expect(status.view() == "No track", "session no-track status")) {
            return false;
        }
        if (!expect(session.add_track(wav_path, true), "session add first track")) {
            return false;
        }
        if (!expect(session.add_track(next_wav_path, true), "session add next track")) {
            return false;
        }
        if (!expect(session.add_track(third_wav_path, true), "session add third track")) {
            return false;
        }
        if (!expect(session.available_track_count() == 3, "session track count")) {
            return false;
        }
        if (!expect(session.current_track_index() == 0, "session auto-loads first track")) {
            return false;
        }
        if (!expect(session.start(status), "session start")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::buffering,
                               audio::PlayerState::playing),
                    "session audio starts")) {
            return false;
        }
        if (!expect(session.next_track(status), "session next while playing")) {
            return false;
        }
        if (!expect(session.current_track_index() == 1, "session next index")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::buffering,
                               audio::PlayerState::playing),
                    "session next resumes playback")) {
            return false;
        }
        if (!expect(session.previous_track(status), "session previous while playing")) {
            return false;
        }
        if (!expect(session.current_track_index() == 0, "session previous index")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::buffering,
                               audio::PlayerState::playing),
                    "session previous resumes playback")) {
            return false;
        }
        if (!expect(session.pause(status), "session pause")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::paused,
                               audio::PlayerState::paused),
                    "session audio paused")) {
            return false;
        }
        if (!expect(session.select_track_index(1, status), "session select while paused")) {
            return false;
        }
        if (!expect(session.current_track_index() == 1, "session select index")) {
            return false;
        }
        if (!expect(session.engine().paused() && !session.engine().playing(),
                    "session select preserves paused state")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::paused,
                               audio::PlayerState::paused),
                    "session select keeps audio paused")) {
            return false;
        }
        player::PlaybackSession<4> failed_switch_session{};
        failed_switch_session.bind_player(audio_player);
        if (!expect(failed_switch_session.set_track_slot(0, wav_path, true)
                    && failed_switch_session.set_track_slot(1, next_wav_path, true),
                    "session failed-switch fixture slots")) {
            return false;
        }
        if (!expect(failed_switch_session.select_track_index(
                        1, player::PlaybackResumeMode::Paused, status),
                    "session failed-switch starts paused track")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::paused,
                               audio::PlayerState::paused),
                    "session failed-switch audio paused")) {
            return false;
        }
        if (!expect(failed_switch_session.set_track_slot(3, "", true),
                    "session sets empty slot while paused")) {
            return false;
        }
        if (!expect(!failed_switch_session.select_track_index(3, status),
                    "session rejects empty target without disrupting current track")) {
            return false;
        }
        if (!expect(status.view() == "No track"
                    && failed_switch_session.current_track_index() == 1
                    && failed_switch_session.engine().paused()
                    && !failed_switch_session.engine().playing()
                    && failed_switch_session.track_ready(),
                    "session empty target preserves current paused track")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::paused,
                               audio::PlayerState::paused),
                    "session empty target keeps audio paused")) {
            return false;
        }
        if (!expect(failed_switch_session.set_track_slot(3, third_wav_path, false),
                    "session sets unavailable slot while paused")) {
            return false;
        }
        if (!expect(!failed_switch_session.select_track_index(3, status),
                    "session unavailable target reports not ready")) {
            return false;
        }
        audio_player.tick();
        if (!expect(status.view() == "Track not ready"
                    && failed_switch_session.current_track_index() == 3
                    && !failed_switch_session.track_ready()
                    && !failed_switch_session.engine().paused()
                    && !failed_switch_session.engine().playing()
                    && audio_player.state() == audio::PlayerState::idle,
                    "session unavailable target becomes current stopped track")) {
            return false;
        }
        const auto mode1 = session.cycle_playback_mode();
        const auto mode2 = session.cycle_playback_mode();
        const auto mode3 = session.cycle_playback_mode();
        if (!expect(mode1 == player::PlaybackMode::RepeatOne
                    && mode2 == player::PlaybackMode::Shuffle
                    && mode3 == player::PlaybackMode::Sequential,
                    "session play mode cycle")) {
            return false;
        }
        const std::array<int, 2> filtered_queue{{1, 0}};
        if (!expect(session.set_queue_from_order(filtered_queue, 1), "session filtered queue")) {
            return false;
        }
        auto filtered_snap = session.snapshot();
        if (!expect(filtered_snap.queue_count == 2 && filtered_snap.queue_position == 0,
                    "session filtered queue position")) {
            return false;
        }
        if (!expect(session.queue_position_for_track_index(1) == 0
                    && session.queue_position_for_track_index(0) == 1,
                    "session filtered queue track positions")) {
            return false;
        }
        if (!expect(session.track_for_queue_delta(1) == 0,
                    "session filtered queue delta lookup")) {
            return false;
        }
        if (!expect(session.next_track(status), "session manual next uses filtered queue")) {
            return false;
        }
        if (!expect(session.current_track_index() == 0, "session filtered queue next index")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::paused,
                               audio::PlayerState::paused),
                    "session filtered queue keeps paused state")) {
            return false;
        }
        if (!expect(session.select_track_index(1, status), "session reselect queue head")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::paused,
                               audio::PlayerState::paused),
                    "session reselect keeps paused state")) {
            return false;
        }
        session.set_playback_mode(player::PlaybackMode::RepeatOne);
        if (!expect(session.advance_track(status), "session repeat-one advance")) {
            return false;
        }
        if (!expect(session.current_track_index() == 1, "session repeat-one keeps current track")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::paused,
                               audio::PlayerState::paused),
                    "session repeat-one keeps paused state")) {
            return false;
        }
        session.set_playback_mode(player::PlaybackMode::Sequential);
        if (!expect(session.advance_track(status), "session sequential advance")) {
            return false;
        }
        if (!expect(session.current_track_index() == 0, "session sequential follows queue")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::paused,
                               audio::PlayerState::paused),
                    "session sequential keeps paused state")) {
            return false;
        }
        if (!expect(!session.advance_track(status), "session sequential auto advance stops at queue end")) {
            return false;
        }
        if (!expect(status.view() == "No track"
                    && session.current_track_index() == 0,
                    "session sequential end keeps current track")) {
            return false;
        }
        player::PlaybackSession<4> skip_session{};
        if (!expect(skip_session.set_track_slot(0, wav_path, true)
                    && skip_session.set_track_slot(1, next_wav_path, false)
                    && skip_session.set_track_slot(2, third_wav_path, true),
                    "session skip fixture slots")) {
            return false;
        }
        const std::array<int, 3> skip_queue{{0, 1, 2}};
        if (!expect(skip_session.set_queue_from_order(skip_queue, 0),
                    "session skip queue")) {
            return false;
        }
        if (!expect(skip_session.select_track_index(0, player::PlaybackResumeMode::Stopped, status),
                    "session skip select first track")) {
            return false;
        }
        if (!expect(skip_session.track_for_queue_delta(1) == 1,
                    "session manual queue delta can target unavailable track")) {
            return false;
        }
        skip_session.set_playback_mode(player::PlaybackMode::Sequential);
        if (!expect(skip_session.resolve_track_for_playback_mode() == 2,
                    "session sequential skips unavailable queue item")) {
            return false;
        }
        if (!expect(skip_session.load_track_index(2, &status),
                    "session skip loads next playable track")) {
            return false;
        }
        skip_session.set_playback_mode(player::PlaybackMode::RepeatOne);
        if (!expect(skip_session.resolve_track_for_playback_mode() == 2,
                    "session repeat-one accepts playable current track")) {
            return false;
        }
        (void)skip_session.set_track_slot(2, third_wav_path, false);
        if (!expect(skip_session.resolve_track_for_playback_mode() == -1,
                    "session repeat-one rejects unavailable current track")) {
            return false;
        }
        skip_session.set_playback_mode(player::PlaybackMode::Shuffle);
        if (!expect(skip_session.resolve_track_for_playback_mode() == 0,
                    "session shuffle skips unavailable tracks")) {
            return false;
        }
        if (!expect(session.select_track_index(1, status), "session reselect before shuffle")) {
            return false;
        }
        session.set_shuffle_seed(1);
        session.set_playback_mode(player::PlaybackMode::Shuffle);
        if (!expect(session.advance_track(status), "session shuffle advance")) {
            return false;
        }
        if (!expect(session.current_track_index() == 0, "session shuffle avoids current track")) {
            return false;
        }
        if (!expect(tick_until(audio_player,
                               audio::PlayerState::paused,
                               audio::PlayerState::paused),
                    "session shuffle keeps paused state")) {
            return false;
        }
        const auto snap = session.snapshot();
        if (!expect(snap.track_index == 0
                    && snap.track_count == 3
                    && snap.queue_count == 2
                    && snap.track_ready
                    && snap.paused,
                    "session snapshot")) {
            return false;
        }
        if (!expect(session.stop(status), "session stop")) {
            return false;
        }
        audio_player.tick();
        if (!expect(audio_player.state() == audio::PlayerState::idle, "session audio idle after stop")) {
            return false;
        }

        std::remove(wav_path);
        std::remove(next_wav_path);
        std::remove(third_wav_path);
        return true;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::printf("[player-playback-engine-smoke] ok\n");
    return 0;
}
