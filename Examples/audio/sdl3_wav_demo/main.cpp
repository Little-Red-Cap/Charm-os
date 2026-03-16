import charm.domain;
import charm.system.clock;
import platform.win.time_source;
import audio.fifo;
import audio.format;
import audio.pull_sim;
import audio.pump;
import audio.sink.sdl3;
import audio.tone;
import media.stream.sink;
import media.stream.types;

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using audio::AudioPlayer;
using audio::OutputMode;
using audio::PcmFifo;
using audio::PlayerConfig;
using audio::PlayerProfile;
using audio::PlayerSnapshot;
using audio::PlayerState;
using audio::SampleType;
using audio::AudioPullSimulator;
using audio::Sdl3AudioSink;
using audio::SineTone;

static charm::system::ClockTick now_us(void*) noexcept {
    return platform::win::SteadyClock::now();
}

struct Profile {
    const char* name;
    PlayerProfile profile;
};

static std::uint32_t parse_u32(const char* value, std::uint32_t fallback) {
    if (!value || !*value) return fallback;
    char* end = nullptr;
    const auto parsed = std::strtoul(value, &end, 10);
    if (!end || *end != '\0') return fallback;
    return static_cast<std::uint32_t>(parsed);
}

static double parse_f64(const char* value, double fallback) {
    if (!value || !*value) return fallback;
    char* end = nullptr;
    const auto parsed = std::strtod(value, &end);
    if (!end || *end != '\0') return fallback;
    return parsed;
}

static double bytes_to_ms(std::size_t bytes, const audio::AudioFormat& fmt) {
    const double frames = static_cast<double>(bytes) / static_cast<double>(fmt.frame_size());
    return frames * 1000.0 / static_cast<double>(fmt.rate);
}

static void print_usage() {
    std::printf("usage: sdl3-wav-demo [--tone[=HZ]] [--pull-sim] [--pull-jitter-ms N]\n");
    std::printf("                      [--pull-jitter-seed N] [--pull-jitter-pattern uniform|burst]\n");
    std::printf("                      [--tone-gain G] [--tone-fifo-ms N] [--tone-period-frames N]\n");
    std::printf("                      [--profile lowlat|stable] [--seconds N] [--stress[=ms]] [--fixed-rate N] [--force-mono 1|2]\n");
    std::printf("                      [--reconfig-at SEC] [--reconfig-fixed-rate N] [--reconfig-fade-in MS] [--fail-reconfig-open]\n");
    std::printf("                      <file.wav|file.flac|file.mp3>\n");
}

struct ToneConfig {
    std::uint32_t duration_sec{10};
    std::uint32_t fifo_ms{300};
    std::uint32_t period_frames{0};
    float freq_hz{440.0f};
    float gain{0.2f};
    std::uint32_t pull_jitter_ms{0};
    std::uint32_t pull_jitter_seed{0};
    std::string pull_jitter_pattern{"uniform"};
};

static int run_pull_sim(const ToneConfig& cfg, charm::system::Clock& clock) {
    audio::AudioFormat fmt{};
    fmt.rate = 48000;
    fmt.channels = 2;
    fmt.sample_type = SampleType::s16;
    fmt.interleaved = true;

    const std::size_t fifo_bytes = (static_cast<std::size_t>(fmt.rate) * fmt.frame_size() * cfg.fifo_ms) / 1000;
    if (fifo_bytes == 0) {
        std::printf("[sim] invalid fifo size\n");
        return 1;
    }

    PcmFifo fifo(fifo_bytes);
    SineTone tone{};
    tone.set_freq_hz(cfg.freq_hz);
    tone.set_gain(cfg.gain);

    const std::uint32_t period = cfg.period_frames != 0 ? cfg.period_frames : (fmt.rate / 100);
    const std::size_t chunk_frames = period;
    const std::size_t chunk_bytes = chunk_frames * fmt.frame_size();
    std::vector<std::byte> scratch(chunk_bytes);

    audio::AudioPump pump{};
    pump.bind(fifo, fmt);

    AudioPullSimulator sim{};
    sim.set_clock(clock);
    sim.set_jitter_ms(cfg.pull_jitter_ms);
    if (cfg.pull_jitter_seed != 0) {
        sim.set_jitter_seed(cfg.pull_jitter_seed);
    }
    if (cfg.pull_jitter_pattern == "burst") {
        sim.set_jitter_pattern(audio::JitterPattern::burst);
    } else {
        sim.set_jitter_pattern(audio::JitterPattern::uniform);
    }

    media::StreamFormat stream_fmt{};
    stream_fmt.kind = media::StreamKind::audio;
    stream_fmt.rate = fmt.rate;
    stream_fmt.channels = fmt.channels;
    stream_fmt.bits_per_sample = 16;

    media::SinkConfig sink_cfg{};
    sink_cfg.format = stream_fmt;
    sink_cfg.period_frames = period;
    if (!sim.open(sink_cfg)) {
        std::printf("[sim] open failed\n");
        return 1;
    }

    sim.set_fill_callback(pump.fill_callback(), &pump);
    if (!sim.start()) {
        std::printf("[sim] start failed\n");
        return 1;
    }

    const std::size_t prefill = fifo.capacity_bytes() / 2;
    while (fifo.size_bytes() < prefill) {
        if (audio::write_tone_fifo(fifo, tone, fmt, scratch) == 0) break;
    }

    const auto start_time = std::chrono::steady_clock::now();
    auto last_log = start_time;
    auto next_tick = start_time + std::chrono::microseconds(sim.next_jitter_us());
    const auto period_us = static_cast<std::uint64_t>(
        (static_cast<double>(period) * 1000000.0) / static_cast<double>(fmt.rate));
    if (period_us == 0) {
        std::printf("[sim] invalid period\n");
        return 1;
    }
    const auto period_duration = std::chrono::microseconds(period_us);

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (cfg.duration_sec > 0 &&
            std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() >= cfg.duration_sec) {
            break;
        }

        while (fifo.producer_free_bytes() >= scratch.size()) {
            if (audio::write_tone_fifo(fifo, tone, fmt, scratch) == 0) break;
        }

        while (now >= next_tick) {
            sim.step_once();
            next_tick += period_duration + std::chrono::microseconds(sim.next_jitter_us());
        }

        if (now - last_log >= std::chrono::seconds(1)) {
            const auto stats = pump.snapshot();
            const auto sim_stats = sim.callback_stats();
            const double water_min_ms = stats.has_water ? bytes_to_ms(stats.water_min, fmt) : 0.0;
            const double water_max_ms = stats.has_water ? bytes_to_ms(stats.water_max, fmt) : 0.0;
            const double water_now_ms = bytes_to_ms(fifo.size_bytes(), fmt);
            std::printf("[sim] cb=%llu underrun=%llu water(ms)=%.0f now=%.0f..%.0f dt(ms)=%.2f/%.2f/%.2f jitter_ms=%u\n",
                static_cast<unsigned long long>(stats.callback_count),
                static_cast<unsigned long long>(stats.underrun_count),
                water_now_ms,
                water_min_ms,
                water_max_ms,
                sim_stats.dt_min_ns ? (static_cast<double>(sim_stats.dt_min_ns) / 1e6) : 0.0,
                sim_stats.dt_avg_ms,
                sim_stats.dt_max_ns ? (static_cast<double>(sim_stats.dt_max_ns) / 1e6) : 0.0,
                cfg.pull_jitter_ms);
            last_log = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    sim.stop();
    sim.close();
    return 0;
}

static int run_tone_demo(const ToneConfig& cfg, charm::system::Clock& clock) {
    audio::AudioFormat fmt{};
    fmt.rate = 48000;
    fmt.channels = 2;
    fmt.sample_type = SampleType::s16;
    fmt.interleaved = true;

    const std::size_t fifo_bytes = (static_cast<std::size_t>(fmt.rate) * fmt.frame_size() * cfg.fifo_ms) / 1000;
    if (fifo_bytes == 0) {
        std::printf("[tone] invalid fifo size\n");
        return 1;
    }

    PcmFifo fifo(fifo_bytes);
    SineTone tone{};
    tone.set_freq_hz(cfg.freq_hz);
    tone.set_gain(cfg.gain);

    const std::uint32_t period = cfg.period_frames != 0 ? cfg.period_frames : (fmt.rate / 100);
    const std::size_t chunk_frames = period;
    const std::size_t chunk_bytes = chunk_frames * fmt.frame_size();
    std::vector<std::byte> scratch(chunk_bytes);

    audio::AudioPump pump{};
    pump.bind(fifo, fmt);

    Sdl3AudioSink sink{};
    sink.set_clock(clock);

    media::StreamFormat stream_fmt{};
    stream_fmt.kind = media::StreamKind::audio;
    stream_fmt.rate = fmt.rate;
    stream_fmt.channels = fmt.channels;
    stream_fmt.bits_per_sample = 16;

    media::SinkConfig sink_cfg{};
    sink_cfg.format = stream_fmt;
    sink_cfg.period_frames = period;
    if (!sink.open(sink_cfg)) {
        std::printf("[tone] sink open failed\n");
        return 1;
    }

    sink.set_fill_callback(pump.fill_callback(), &pump);
    if (!sink.start()) {
        std::printf("[tone] sink start failed\n");
        sink.close();
        return 1;
    }

    const std::size_t prefill = fifo.capacity_bytes() / 2;
    while (fifo.size_bytes() < prefill) {
        if (audio::write_tone_fifo(fifo, tone, fmt, scratch) == 0) break;
    }

    const auto start_time = std::chrono::steady_clock::now();
    auto last_log = start_time;
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (cfg.duration_sec > 0 &&
            std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() >= cfg.duration_sec) {
            break;
        }

        while (fifo.free_bytes() >= scratch.size()) {
            if (audio::write_tone_fifo(fifo, tone, fmt, scratch) == 0) break;
        }

        if (now - last_log >= std::chrono::seconds(1)) {
            const auto stats = pump.snapshot();
            const double water_min_ms = stats.has_water ? bytes_to_ms(stats.water_min, fmt) : 0.0;
            const double water_max_ms = stats.has_water ? bytes_to_ms(stats.water_max, fmt) : 0.0;
            const double water_now_ms = bytes_to_ms(fifo.size_bytes(), fmt);
            std::printf("[tone] cb=%llu underrun=%llu water(ms)=%.0f now=%.0f..%.0f\n",
                static_cast<unsigned long long>(stats.callback_count),
                static_cast<unsigned long long>(stats.underrun_count),
                water_now_ms,
                water_min_ms,
                water_max_ms);
            last_log = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    sink.stop();
    sink.close();
    return 0;
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    const Profile lowlat{"lowlat", PlayerProfile{40, 120, 4, 200}};
    const Profile stable{"stable", PlayerProfile{40, 150, 10, 300}};
    const Profile* profile = &stable;
    std::uint32_t duration_sec = 0;
    std::uint32_t stress_ms = 0;
    std::uint32_t fixed_rate = 0;
    std::uint32_t force_mono = 0;
    double reconfig_at = -1.0;
    std::uint32_t reconfig_fixed_rate = 0;
    std::uint32_t reconfig_fade_in_ms = 0;
    bool reconfig_fail_open = false;
    bool use_tone = false;
    bool use_pull_sim = false;
    ToneConfig tone_cfg{};

    std::string path{};
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--tone") {
            use_tone = true;
        } else if (arg == "--pull-sim") {
            use_pull_sim = true;
            use_tone = true;
        } else if (arg == "--pull-jitter-ms" && i + 1 < argc) {
            tone_cfg.pull_jitter_ms = parse_u32(argv[++i], tone_cfg.pull_jitter_ms);
        } else if (arg.rfind("--pull-jitter-ms=", 0) == 0) {
            tone_cfg.pull_jitter_ms = parse_u32(arg.c_str() + std::strlen("--pull-jitter-ms="), tone_cfg.pull_jitter_ms);
        } else if (arg == "--pull-jitter-seed" && i + 1 < argc) {
            tone_cfg.pull_jitter_seed = parse_u32(argv[++i], tone_cfg.pull_jitter_seed);
        } else if (arg.rfind("--pull-jitter-seed=", 0) == 0) {
            tone_cfg.pull_jitter_seed = parse_u32(arg.c_str() + std::strlen("--pull-jitter-seed="), tone_cfg.pull_jitter_seed);
        } else if (arg == "--pull-jitter-pattern" && i + 1 < argc) {
            tone_cfg.pull_jitter_pattern = argv[++i];
        } else if (arg.rfind("--pull-jitter-pattern=", 0) == 0) {
            tone_cfg.pull_jitter_pattern = arg.substr(std::strlen("--pull-jitter-pattern="));
        } else if (arg.rfind("--tone=", 0) == 0) {
            use_tone = true;
            tone_cfg.freq_hz = static_cast<float>(parse_f64(arg.c_str() + std::strlen("--tone="), tone_cfg.freq_hz));
        } else if (arg == "--tone-gain" && i + 1 < argc) {
            tone_cfg.gain = static_cast<float>(parse_f64(argv[++i], tone_cfg.gain));
        } else if (arg.rfind("--tone-gain=", 0) == 0) {
            tone_cfg.gain = static_cast<float>(parse_f64(arg.c_str() + std::strlen("--tone-gain="), tone_cfg.gain));
        } else if (arg == "--tone-fifo-ms" && i + 1 < argc) {
            tone_cfg.fifo_ms = parse_u32(argv[++i], tone_cfg.fifo_ms);
        } else if (arg.rfind("--tone-fifo-ms=", 0) == 0) {
            tone_cfg.fifo_ms = parse_u32(arg.c_str() + std::strlen("--tone-fifo-ms="), tone_cfg.fifo_ms);
        } else if (arg == "--tone-period-frames" && i + 1 < argc) {
            tone_cfg.period_frames = parse_u32(argv[++i], tone_cfg.period_frames);
        } else if (arg.rfind("--tone-period-frames=", 0) == 0) {
            tone_cfg.period_frames = parse_u32(arg.c_str() + std::strlen("--tone-period-frames="), tone_cfg.period_frames);
        } else if (arg == "--profile" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "lowlat") profile = &lowlat;
            else if (value == "stable") profile = &stable;
        } else if (arg.rfind("--profile=", 0) == 0) {
            const std::string value = arg.substr(std::strlen("--profile="));
            if (value == "lowlat") profile = &lowlat;
            else if (value == "stable") profile = &stable;
        } else if ((arg == "--seconds" || arg == "--duration") && i + 1 < argc) {
            duration_sec = parse_u32(argv[++i], 0);
        } else if (arg.rfind("--seconds=", 0) == 0) {
            duration_sec = parse_u32(arg.c_str() + std::strlen("--seconds="), 0);
        } else if (arg.rfind("--duration=", 0) == 0) {
            duration_sec = parse_u32(arg.c_str() + std::strlen("--duration="), 0);
        } else if (arg == "--stress") {
            stress_ms = 5;
        } else if (arg.rfind("--stress=", 0) == 0) {
            stress_ms = parse_u32(arg.c_str() + std::strlen("--stress="), 5);
        } else if (arg == "--fixed-rate" && i + 1 < argc) {
            fixed_rate = parse_u32(argv[++i], 0);
        } else if (arg.rfind("--fixed-rate=", 0) == 0) {
            fixed_rate = parse_u32(arg.c_str() + std::strlen("--fixed-rate="), 0);
        } else if (arg == "--force-mono" && i + 1 < argc) {
            force_mono = parse_u32(argv[++i], 0);
        } else if (arg.rfind("--force-mono=", 0) == 0) {
            force_mono = parse_u32(arg.c_str() + std::strlen("--force-mono="), 0);
        } else if (arg == "--reconfig-at" && i + 1 < argc) {
            reconfig_at = parse_f64(argv[++i], -1.0);
        } else if (arg.rfind("--reconfig-at=", 0) == 0) {
            reconfig_at = parse_f64(arg.c_str() + std::strlen("--reconfig-at="), -1.0);
        } else if (arg == "--reconfig-fixed-rate" && i + 1 < argc) {
            reconfig_fixed_rate = parse_u32(argv[++i], 0);
        } else if (arg.rfind("--reconfig-fixed-rate=", 0) == 0) {
            reconfig_fixed_rate = parse_u32(arg.c_str() + std::strlen("--reconfig-fixed-rate="), 0);
        } else if (arg == "--reconfig-fade-in" && i + 1 < argc) {
            reconfig_fade_in_ms = parse_u32(argv[++i], 0);
        } else if (arg.rfind("--reconfig-fade-in=", 0) == 0) {
            reconfig_fade_in_ms = parse_u32(arg.c_str() + std::strlen("--reconfig-fade-in="), 0);
        } else if (arg == "--fail-reconfig-open") {
            reconfig_fail_open = true;
        } else if (!arg.empty() && arg[0] != '-') {
            path = arg;
        }
    }

    if (use_tone && duration_sec > 0) {
        tone_cfg.duration_sec = duration_sec;
    }
    if (use_tone) {
        charm::system::Clock clock{nullptr, {.now_us = &now_us}};
        if (use_pull_sim) {
            return run_pull_sim(tone_cfg, clock);
        }
        return run_tone_demo(tone_cfg, clock);
    }

    if (path.empty()) {
        print_usage();
        return 1;
    }

    PlayerConfig config{};
    config.profile = profile->profile;
    if (fixed_rate > 0) {
        config.output_mode = OutputMode::fixed_rate;
        config.fixed_rate = fixed_rate;
    }
    if (force_mono == 1 || force_mono == 2) {
        config.force_channels = static_cast<std::uint16_t>(force_mono);
    }
    if (reconfig_fail_open) {
        config.fail_reconfig_step = 1;
    }

    charm::system::Clock clock{nullptr, {.now_us = &now_us}};
    AudioPlayer player(config, clock);
    player.set_stress_ms(stress_ms);

    auto play_res = player.play(path.c_str());
    if (!play_res) {
        std::printf("[audio] play enqueue failed\n");
        return 1;
    }

    player.tick();
    if (player.state() == PlayerState::error) {
        std::printf("[audio] open failed\n");
        return 1;
    }

    PlayerSnapshot snapshot = player.snapshot(false);
    const double period_ms = snapshot.period_frames
        ? (1000.0 * static_cast<double>(snapshot.period_frames) / static_cast<double>(snapshot.output_fmt.rate))
        : 0.0;
    const double chunk_ms = snapshot.chunk_frames
        ? (1000.0 * static_cast<double>(snapshot.chunk_frames) / static_cast<double>(snapshot.output_fmt.rate))
        : 0.0;

    std::printf("[audio] input_rate=%u output_rate=%u in_ch=%u out_ch=%u profile=%s\n",
        snapshot.input_fmt.rate,
        snapshot.output_fmt.rate,
        snapshot.input_fmt.channels,
        snapshot.output_fmt.channels,
        profile->name);
    std::printf("[config] period_ms=%.2f cb_frames=%u low=%ums high=%ums chunk=%.1fms fifo=%ums stress=%ums\n",
        period_ms,
        snapshot.period_frames,
        profile->profile.low_ms,
        profile->profile.high_ms,
        chunk_ms,
        profile->profile.fifo_ms,
        stress_ms);

    const auto start_time = std::chrono::steady_clock::now();
    auto last_log = start_time;
    bool stop_requested = false;
    bool reconfig_pending = false;
    bool reconfig_done = false;
    PlayerState prev_state = player.state();
    std::chrono::steady_clock::time_point reconfig_start{};
    std::size_t reconfig_peak_water = 0;

    while (player.is_running()) {
        player.tick();

        const auto now = std::chrono::steady_clock::now();
        if (!stop_requested && duration_sec > 0) {
            if (std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() >= duration_sec) {
                (void)player.stop();
                stop_requested = true;
            }
        }

        const double elapsed = std::chrono::duration<double>(now - start_time).count();
        if (!reconfig_done && reconfig_at >= 0.0 && elapsed >= reconfig_at && !reconfig_pending) {
            reconfig_pending = true;
            reconfig_start = now;
            reconfig_peak_water = 0;
            std::printf("[reconfig] begin at %.2fs fixed_rate=%u fade_in=%u\n",
                elapsed, reconfig_fixed_rate, reconfig_fade_in_ms);
            auto rc = player.reconfigure_output(reconfig_fixed_rate, reconfig_fade_in_ms);
            if (!rc) {
                std::printf("[reconfig] failed\n");
                reconfig_done = true;
                reconfig_pending = false;
            }
        }

        const PlayerState cur_state = player.state();
        if (reconfig_pending) {
            PlayerSnapshot snap = player.snapshot(false);
            reconfig_peak_water = std::max(reconfig_peak_water, snap.stats.max_water);
            const std::uint32_t expected_rate = reconfig_fixed_rate > 0 ? reconfig_fixed_rate : snap.input_fmt.rate;
            if (cur_state == PlayerState::playing && snap.output_fmt.rate == expected_rate && snap.callback.last_request_frames > 0) {
                const auto done = std::chrono::steady_clock::now();
                const auto ms = std::chrono::duration<double, std::milli>(done - reconfig_start).count();
                const double peak_ms = bytes_to_ms(reconfig_peak_water, snap.output_fmt);
                std::printf("[reconfig] done in %.2fms out_rate=%u cb_frames=%u peak=%.0fms\n",
                    ms,
                    snap.output_fmt.rate,
                    snap.callback.last_request_frames,
                    peak_ms);
                reconfig_done = true;
                reconfig_pending = false;
            }
        }
        prev_state = cur_state;

        if (now - last_log >= std::chrono::seconds(1)) {
            PlayerSnapshot snap = player.snapshot(false);
            const double water_min_ms = bytes_to_ms(snap.stats.min_water, snap.output_fmt);
            const double water_max_ms = bytes_to_ms(snap.stats.max_water, snap.output_fmt);
            const double refill_avg_ms = snap.stats.refill_count
                ? (snap.stats.refill_sum_ms / static_cast<double>(snap.stats.refill_count))
                : 0.0;
            const double cb_min_ms = snap.callback.dt_min_ns ? (static_cast<double>(snap.callback.dt_min_ns) / 1e6) : 0.0;
            const double cb_max_ms = snap.callback.dt_max_ns ? (static_cast<double>(snap.callback.dt_max_ns) / 1e6) : 0.0;
            std::printf("t=%.1fs rate=%u cb_frames=%u cb_dt(ms)=%.2f/%.2f/%.2f water(ms)=%.0f..%.0f underrun=%llu refill=%llu refill_ms=%.2f/%.2f/%.2f\n",
                elapsed,
                snap.output_fmt.rate,
                snap.callback.last_request_frames,
                cb_min_ms, snap.callback.dt_avg_ms, cb_max_ms,
                water_min_ms, water_max_ms,
                static_cast<unsigned long long>(snap.stats.underrun_count),
                static_cast<unsigned long long>(snap.stats.refill_count),
                snap.stats.refill_min_ms, refill_avg_ms, snap.stats.refill_max_ms);

            if (!reconfig_pending) {
                (void)player.snapshot(true);
            }
            last_log = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    PlayerSnapshot final_stats = player.snapshot(false);
    std::printf("[stats] underrun=%llu refill=%llu overrun=%llu water_bytes=%zu\n",
        static_cast<unsigned long long>(final_stats.stats.underrun_count),
        static_cast<unsigned long long>(final_stats.stats.refill_count),
        static_cast<unsigned long long>(final_stats.stats.overrun_count),
        final_stats.water_bytes);
    return 0;
}
