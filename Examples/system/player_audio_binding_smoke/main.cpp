#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <span>

import audio.player;
import audio.result;
import charm.system.clock;
import media.stream.sink;
import media.stream.source;
import media.stream.types;

namespace {
    constexpr std::size_t kPcmBytes = 3200;

    std::array<std::byte, 44 + kPcmBytes> make_wav() {
        std::array<std::byte, 44 + kPcmBytes> bytes{};
        auto put16 = [&](std::size_t at, std::uint16_t value) {
            bytes[at] = static_cast<std::byte>(value & 0xffu);
            bytes[at + 1] = static_cast<std::byte>((value >> 8) & 0xffu);
        };
        auto put32 = [&](std::size_t at, std::uint32_t value) {
            for (std::size_t i = 0; i < 4; ++i) {
                bytes[at + i] = static_cast<std::byte>((value >> (8 * i)) & 0xffu);
            }
        };
        std::memcpy(bytes.data(), "RIFF", 4);
        put32(4, static_cast<std::uint32_t>(36 + kPcmBytes));
        std::memcpy(bytes.data() + 8, "WAVEfmt ", 8);
        put32(16, 16);
        put16(20, 1);
        put16(22, 1);
        put32(24, 8000);
        put32(28, 16000);
        put16(32, 2);
        put16(34, 16);
        std::memcpy(bytes.data() + 36, "data", 4);
        put32(40, static_cast<std::uint32_t>(kPcmBytes));
        return bytes;
    }

    struct MemorySource {
        std::array<std::byte, 44 + kPcmBytes> bytes{make_wav()};
        std::size_t position{0};
        std::size_t open_count{0};
        std::size_t close_count{0};
        bool allow_open{true};
        audio::Errc open_error{audio::Errc::io_error};

        bool open(const char*) noexcept {
            ++open_count;
            position = 0;
            return allow_open;
        }
        void close() noexcept { ++close_count; }
        media::Result<std::size_t> read(std::span<std::byte> out) noexcept {
            const auto count = std::min(out.size(), bytes.size() - position);
            std::memcpy(out.data(), bytes.data() + position, count);
            position += count;
            return count;
        }
        media::Result<std::int64_t> seek(
            std::int64_t offset, media::SeekWhence whence) noexcept {
            std::int64_t base = 0;
            if (whence == media::SeekWhence::cur) base = static_cast<std::int64_t>(position);
            if (whence == media::SeekWhence::end) base = static_cast<std::int64_t>(bytes.size());
            const auto next = base + offset;
            if (next < 0 || next > static_cast<std::int64_t>(bytes.size())) {
                return audio::unexpected(audio::Errc::invalid_arg);
            }
            position = static_cast<std::size_t>(next);
            return next;
        }
        media::Result<std::int64_t> tell() noexcept {
            return static_cast<std::int64_t>(position);
        }
        media::Result<std::int64_t> size() noexcept {
            return static_cast<std::int64_t>(bytes.size());
        }
    };

    audio::AudioSourceBinding make_source_binding(MemorySource& source) noexcept {
        return audio::AudioSourceBinding{
            .ctx = &source,
            .open_fn = [](void* ctx, const char* path) noexcept
                -> audio::Result<media::StreamSourceRef> {
                auto& value = *static_cast<MemorySource*>(ctx);
                if (!value.open(path)) {
                    return audio::unexpected(value.open_error);
                }
                return media::make_stream_source_ref(value);
            },
            .close_fn = [](void* ctx) noexcept {
                static_cast<MemorySource*>(ctx)->close();
            },
        };
    }

    struct CaptureSink {
        media::FillCallback callback{nullptr};
        void* callback_user{nullptr};
        media::StreamFormat stream_format{};
        std::size_t open_count{0};
        std::size_t start_count{0};
        std::size_t stop_count{0};
        std::size_t close_count{0};
        std::size_t fill_count{0};
        bool running{false};
        audio::Errc open_error{audio::Errc::ok};
        audio::Errc start_error{audio::Errc::ok};

        void set_clock(charm::system::Clock&) noexcept {}
        audio::Result<void> open(const media::SinkConfig& config) noexcept {
            ++open_count;
            if (open_error != audio::Errc::ok) {
                return audio::unexpected(open_error);
            }
            stream_format = config.format;
            return {};
        }
        audio::Result<void> start() noexcept {
            ++start_count;
            if (start_error != audio::Errc::ok) {
                return audio::unexpected(start_error);
            }
            running = true;
            return {};
        }
        audio::Result<void> stop() noexcept { ++stop_count; running = false; return {}; }
        void close() noexcept { ++close_count; running = false; }
        void set_fill_callback(media::FillCallback value, void* user) noexcept {
            callback = value;
            callback_user = user;
        }
        media::StreamFormat format() const noexcept { return stream_format; }
        std::uint32_t actual_period_frames() const noexcept { return 80; }
        std::uint64_t underrun_count() const noexcept { return 0; }
        bool consume_underrun_flag() noexcept { return false; }
        void clear_underrun_flag() noexcept {}
        audio::CallbackStats callback_stats() const noexcept { return {.count = fill_count}; }
        void pump_once() noexcept {
            if (!running || !callback) return;
            std::array<std::byte, 160> output{};
            (void)callback(output, callback_user);
            ++fill_count;
        }
    };

    charm::system::ClockTick now_us(void*) noexcept { return 1000; }

    bool expect(bool value, const char* message) {
        if (!value) std::printf("[player-audio-binding-smoke] fail: %s\n", message);
        return value;
    }

    bool start(audio::AudioPlayer& player) {
        if (!player.play("memory.wav")) return false;
        for (int i = 0; i < 32; ++i) {
            player.tick();
            if (player.state() == audio::PlayerState::playing) return true;
            if (player.state() == audio::PlayerState::error) return false;
        }
        return false;
    }

    bool fail(audio::AudioPlayer& player, const char* path) {
        if (!player.play(path)) return false;
        for (int i = 0; i < 32; ++i) {
            player.tick();
            if (player.state() == audio::PlayerState::error) return true;
            if (player.state() == audio::PlayerState::playing) return false;
        }
        return false;
    }
}

int main() {
    charm::system::Clock clock{nullptr, {.now_us = &now_us}};
    static MemorySource first_source{};
    static MemorySource second_source{};
    static CaptureSink first_sink{};
    static CaptureSink second_sink{};
    const audio::PlayerConfig config{.capture_output = false};
    auto first = std::make_unique<audio::AudioPlayer>(
        config,
        audio::PlayerBindings{audio::make_audio_source_binding(first_source),
                              audio::make_audio_sink_binding(first_sink)},
        clock);
    auto second = std::make_unique<audio::AudioPlayer>(
        config,
        audio::PlayerBindings{audio::make_audio_source_binding(second_source),
                              audio::make_audio_sink_binding(second_sink)},
        clock);
    if (!expect(start(*first) && start(*second), "two injected players start")
        || !expect(first_source.open_count == 1 && second_source.open_count == 1,
                   "source opens remain isolated")
        || !expect(first_sink.start_count == 1 && second_sink.start_count == 1,
                   "sink starts remain isolated")) {
        return 1;
    }
    first->tick();
    second->tick();
    first->shutdown();
    second->shutdown();
    if (!expect(first_source.close_count == 1 && second_source.close_count == 1,
                 "provider-owned sources close")
        || !expect(first_sink.stop_count == 1 && second_sink.stop_count == 1
                       && first_sink.close_count == 1 && second_sink.close_count == 1,
                   "started sinks stop and close once")
        || !expect(first_sink.fill_count != 0 && second_sink.fill_count != 0,
                    "fill callbacks execute")) {
        return 1;
    }

    {
        static MemorySource failing_source{};
        static CaptureSink failing_sink{};
        failing_source.allow_open = false;
        failing_source.open_error = audio::Errc::timeout;
        auto failing = std::make_unique<audio::AudioPlayer>(
            config,
            audio::PlayerBindings{make_source_binding(failing_source),
                                  audio::make_audio_sink_binding(failing_sink)},
            clock);
        if (!expect(fail(*failing, "missing.wav"), "source open failure reached")
            || !expect(failing->last_error_stage() == audio::PlayerErrorStage::open_source
                           && failing->last_error() == audio::Errc::timeout,
                       "source error is preserved")
            || !expect(failing_source.close_count == 0,
                       "failed source open is not closed")
            || !expect(failing_sink.stop_count == 0 && failing_sink.close_count == 0,
                       "unopened sink is untouched")) {
            return 1;
        }
    }

    {
        static MemorySource unsupported_source{};
        static CaptureSink unsupported_sink{};
        unsupported_source.bytes.fill(std::byte{});
        auto unsupported = std::make_unique<audio::AudioPlayer>(
            config,
            audio::PlayerBindings{make_source_binding(unsupported_source),
                                  audio::make_audio_sink_binding(unsupported_sink)},
            clock);
        if (!expect(fail(*unsupported, "unknown.bin"), "unsupported format reached")
            || !expect(unsupported->last_error_stage()
                           == audio::PlayerErrorStage::unsupported_format,
                       "unsupported format diagnosed")
            || !expect(unsupported_source.close_count == 1,
                       "unsupported source closes immediately")
            || !expect(unsupported_sink.open_count == 0
                           && unsupported_sink.close_count == 0,
                       "unsupported format never acquires sink")) {
            return 1;
        }
    }

    {
        static MemorySource malformed_source{};
        static CaptureSink malformed_sink{};
        malformed_source.bytes.fill(std::byte{});
        auto malformed = std::make_unique<audio::AudioPlayer>(
            config,
            audio::PlayerBindings{make_source_binding(malformed_source),
                                  audio::make_audio_sink_binding(malformed_sink)},
            clock);
        if (!expect(fail(*malformed, "broken.wav"), "decoder failure reached")
            || !expect(malformed->last_error_stage() == audio::PlayerErrorStage::wav_parse,
                       "decoder failure diagnosed")
            || !expect(malformed_source.close_count == 1,
                       "decoder failure closes source immediately")) {
            return 1;
        }
    }

    {
        static MemorySource buffer_source{};
        static CaptureSink buffer_sink{};
        auto buffer_config = config;
        buffer_config.output_mode = audio::OutputMode::fixed_rate;
        buffer_config.fixed_rate = 96000;
        auto buffer_failure = std::make_unique<audio::AudioPlayer>(
            buffer_config,
            audio::PlayerBindings{make_source_binding(buffer_source),
                                  audio::make_audio_sink_binding(buffer_sink)},
            clock);
        if (!expect(fail(*buffer_failure, "buffer.wav"), "buffer failure reached")
            || !expect(buffer_failure->last_error_stage()
                           == audio::PlayerErrorStage::buffer_config,
                       "buffer failure diagnosed")
            || !expect(buffer_source.close_count == 1 && buffer_sink.open_count == 0,
                       "buffer failure releases source before sink open")) {
            return 1;
        }
    }

    {
        static MemorySource sink_open_source{};
        static CaptureSink sink_open_failure{};
        sink_open_failure.open_error = audio::Errc::timeout;
        auto open_failure = std::make_unique<audio::AudioPlayer>(
            config,
            audio::PlayerBindings{make_source_binding(sink_open_source),
                                  audio::make_audio_sink_binding(sink_open_failure)},
            clock);
        if (!expect(fail(*open_failure, "sink-open.wav"), "sink open failure reached")
            || !expect(open_failure->last_error_stage() == audio::PlayerErrorStage::sink_open
                           && open_failure->last_error() == audio::Errc::timeout,
                       "sink open error is preserved")
            || !expect(sink_open_source.close_count == 1,
                       "sink open failure closes source immediately")
            || !expect(sink_open_failure.open_count == 1
                           && sink_open_failure.stop_count == 0
                           && sink_open_failure.close_count == 0,
                       "failed sink open is not stopped or closed")) {
            return 1;
        }
    }

    {
        static MemorySource sink_start_source{};
        static CaptureSink sink_start_failure{};
        sink_start_failure.start_error = audio::Errc::timeout;
        auto start_failure = std::make_unique<audio::AudioPlayer>(
            config,
            audio::PlayerBindings{make_source_binding(sink_start_source),
                                  audio::make_audio_sink_binding(sink_start_failure)},
            clock);
        if (!expect(fail(*start_failure, "sink-start.wav"), "sink start failure reached")
            || !expect(start_failure->last_error_stage()
                           == audio::PlayerErrorStage::sink_start
                           && start_failure->last_error() == audio::Errc::timeout,
                       "sink start error is preserved")
            || !expect(sink_start_source.close_count == 1,
                       "sink start failure closes source immediately")
            || !expect(sink_start_failure.open_count == 1
                           && sink_start_failure.start_count == 1
                           && sink_start_failure.stop_count == 0
                           && sink_start_failure.close_count == 1,
                       "failed sink start closes without stop")) {
            return 1;
        }
    }

    auto legacy = std::make_unique<audio::AudioPlayer>(config, clock);
    if (!expect(legacy->state() == audio::PlayerState::idle,
                "legacy constructor compatibility")) {
        return 1;
    }
    std::puts("[player-audio-binding-smoke] ok");
    return 0;
}
