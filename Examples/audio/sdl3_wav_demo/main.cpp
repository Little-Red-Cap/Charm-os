import charm.domain;
import charm.system.clock;
import platform.win.time_source;

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

using audio::AudioPlayer;
using audio::OutputMode;
using audio::PlayerConfig;
using audio::PlayerProfile;
using audio::PlayerSnapshot;
using audio::PlayerState;

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
    std::printf("usage: sdl3-wav-demo [--profile lowlat|stable] [--seconds N] [--stress[=ms]] [--fixed-rate N] [--force-mono 1|2] [--reconfig-at SEC] [--reconfig-fixed-rate N] [--reconfig-fade-in MS] [--fail-reconfig-open] <file.wav|file.flac|file.mp3>\n");
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

    std::string path{};
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--profile" && i + 1 < argc) {
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
