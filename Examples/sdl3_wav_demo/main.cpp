import audio.result;
import audio.format;
import audio.fifo;
import audio.source.file;
import audio.decoder.wav;
import audio.decoder.flac;
import audio.sink.sdl3;

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <thread>
#include <vector>

using audio::AudioFormat;
using audio::FlacDecoder;
using audio::FileDataSource;
using audio::PcmFifo;
using audio::Sdl3AudioSink;
using audio::SinkConfig;

struct AudioStats {
    std::uint64_t underrun_count{0};
    std::size_t min_water{0};
    std::size_t max_water{0};
    std::uint64_t refill_count{0};
    std::uint64_t overrun_count{0};
    double refill_min_ms{0.0};
    double refill_max_ms{0.0};
    double refill_sum_ms{0.0};
};

static std::size_t ms_to_bytes(std::uint32_t ms, const AudioFormat& fmt) {
    const std::uint64_t frames = (static_cast<std::uint64_t>(fmt.rate) * ms) / 1000;
    return static_cast<std::size_t>(frames * fmt.frame_size());
}

static double bytes_to_ms(std::size_t bytes, const AudioFormat& fmt) {
    const double frames = static_cast<double>(bytes) / static_cast<double>(fmt.frame_size());
    return frames * 1000.0 / static_cast<double>(fmt.rate);
}

static bool ends_with_icase(const std::string& text, const char* suffix) {
    const std::string suf{suffix};
    if (text.size() < suf.size()) return false;
    const std::size_t start = text.size() - suf.size();
    for (std::size_t i = 0; i < suf.size(); ++i) {
        const char a = static_cast<char>(std::tolower(text[start + i]));
        const char b = static_cast<char>(std::tolower(suf[i]));
        if (a != b) return false;
    }
    return true;
}

static std::size_t fill_from_fifo(std::span<std::byte> dst, void* user) noexcept {
    auto* fifo = static_cast<PcmFifo*>(user);
    const std::size_t frame = 4;
    std::size_t need = dst.size() - (dst.size() % frame);
    std::size_t filled = 0;

    while (filled < need) {
        auto v = fifo->readable_view();
        if (v.a.empty() && v.b.empty()) break;

        auto copy_one = [&](std::span<std::byte> src) {
            std::size_t n = std::min(src.size(), need - filled);
            n -= n % frame;
            if (n == 0) return;
            std::memcpy(dst.data() + filled, src.data(), n);
            fifo->commit_read(n);
            filled += n;
        };

        if (!v.a.empty()) copy_one(v.a);
        else if (!v.b.empty()) copy_one(v.b);
    }

    return filled;
}

static void update_refill_stats(AudioStats& stats, double ms) {
    if (stats.refill_count == 0) {
        stats.refill_min_ms = ms;
        stats.refill_max_ms = ms;
    } else {
        stats.refill_min_ms = std::min(stats.refill_min_ms, ms);
        stats.refill_max_ms = std::max(stats.refill_max_ms, ms);
    }
    stats.refill_sum_ms += ms;
    stats.refill_count++;
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    if (argc < 2) {
        std::printf("usage: sdl3-wav-demo <file.wav|file.flac>\n");
        return 1;
    }

    const std::string path = argv[1];
    FileDataSource src;
    if (!src.open(path.c_str())) {
        std::printf("[audio] open failed\n");
        return 1;
    }

    const bool is_flac = ends_with_icase(path, ".flac");
    const bool is_wav = ends_with_icase(path, ".wav");

    AudioFormat fmt{};
    std::size_t remaining_bytes = 0;

    FlacDecoder flac;

    if (is_flac) {
        const auto info_res = flac.open(src);
        if (!info_res) {
            std::printf("[flac] open failed\n");
            return 1;
        }
        fmt.rate = info_res->sample_rate;
        fmt.channels = info_res->channels;
        fmt.sample_type = audio::SampleType::s16;
    } else if (is_wav) {
        const auto info_res = audio::parse_wav(src);
        if (!info_res) {
            std::printf("[wav] parse failed\n");
            return 1;
        }
        const auto info = *info_res;
        if (info.bits_per_sample != 16) {
            std::printf("[wav] only PCM16 supported\n");
            return 1;
        }
        fmt.rate = info.sample_rate;
        fmt.channels = info.channels;
        fmt.sample_type = audio::SampleType::s16;

        const auto data_start = src.seek(info.data_offset, SEEK_SET);
        if (!data_start) {
            std::printf("[wav] seek failed\n");
            return 1;
        }
        remaining_bytes = info.data_size;
    } else {
        std::printf("[audio] unknown file type\n");
        return 1;
    }

    std::printf("[audio] rate=%u channels=%u\n", fmt.rate, fmt.channels);

    const std::size_t fifo_capacity = ms_to_bytes(400, fmt);
    PcmFifo fifo(fifo_capacity);

    Sdl3AudioSink sink;
    SinkConfig cfg{};
    cfg.fmt = fmt;
    cfg.preferred_period_frames = fmt.rate / 100;

    if (!sink.open(cfg)) {
        std::printf("[sink] open failed\n");
        return 1;
    }
    sink.set_fill_callback(fill_from_fifo, &fifo);
    if (!sink.start()) {
        std::printf("[sink] start failed\n");
        return 1;
    }

    const std::size_t low_water = ms_to_bytes(40, fmt);
    const std::size_t high_water = ms_to_bytes(120, fmt);
    const std::size_t chunk_frames = (fmt.rate / 100) * 4;
    const std::size_t chunk_bytes = chunk_frames * fmt.frame_size();

    std::vector<std::byte> raw(chunk_bytes);
    std::vector<std::int16_t> s16_in(chunk_bytes / sizeof(std::int16_t));
    std::vector<std::int16_t> s16_out(chunk_bytes / sizeof(std::int16_t));
    std::vector<std::int32_t> s32_buf(chunk_bytes / sizeof(std::int16_t));

    AudioStats stats{};
    stats.min_water = fifo_capacity;

    auto last_log = std::chrono::steady_clock::now();
    bool running = true;

    while (running) {
        const std::size_t water = fifo.size_bytes();
        stats.min_water = std::min(stats.min_water, water);
        stats.max_water = std::max(stats.max_water, water);

        if (sink.consume_underrun_flag()) {
            stats.underrun_count = sink.underrun_count();
        }

        if (water < low_water) {
            std::size_t writable = std::min(fifo.free_bytes(), chunk_bytes);
            writable = (writable / fmt.frame_size()) * fmt.frame_size();
            if (writable == 0) {
                stats.overrun_count++;
            } else {
                const auto t0 = std::chrono::steady_clock::now();
                std::size_t bytes_written = 0;

                if (is_flac) {
                    const std::size_t frames = writable / fmt.frame_size();
                    auto res = flac.read_s32(s32_buf.data(), frames);
                    if (!res) {
                        std::printf("[flac] read error\n");
                        break;
                    }
                    if (*res == 0) {
                        running = fifo.size_bytes() > 0;
                    } else {
                        const std::size_t samples = *res * fmt.channels;
                        for (std::size_t i = 0; i < samples; ++i) {
                            const std::int32_t clamped = std::clamp(s32_buf[i], static_cast<std::int32_t>(-32768 << 16), static_cast<std::int32_t>(32767 << 16));
                            s16_out[i] = static_cast<std::int16_t>(clamped >> 16);
                        }
                        bytes_written = samples * sizeof(std::int16_t);
                    }
                } else {
                    const std::size_t to_read = std::min(writable, remaining_bytes);
                    auto res = src.read(std::span<std::byte>(raw.data(), to_read));
                    if (!res) {
                        std::printf("[wav] read error\n");
                        break;
                    }
                    if (*res == 0) {
                        remaining_bytes = 0;
                    } else {
                        remaining_bytes -= *res;
                    }

                    const std::size_t samples = *res / sizeof(std::int16_t);
                    std::memcpy(s16_in.data(), raw.data(), samples * sizeof(std::int16_t));
                    for (std::size_t i = 0; i < samples; ++i) {
                        const std::int32_t s32 = static_cast<std::int32_t>(s16_in[i]) << 16;
                        const std::int32_t clamped = std::clamp(s32, static_cast<std::int32_t>(-32768 << 16), static_cast<std::int32_t>(32767 << 16));
                        s16_out[i] = static_cast<std::int16_t>(clamped >> 16);
                    }
                    bytes_written = samples * sizeof(std::int16_t);
                }

                if (bytes_written > 0) {
                    auto view = fifo.writable_view();
                    bytes_written = std::min(bytes_written, view.a.size() + view.b.size());

                    std::size_t written = 0;
                    const std::size_t a = std::min(view.a.size(), bytes_written);
                    std::memcpy(view.a.data(), reinterpret_cast<std::byte*>(s16_out.data()), a);
                    written += a;

                    if (written < bytes_written && !view.b.empty()) {
                        const std::size_t b = std::min(view.b.size(), bytes_written - written);
                        std::memcpy(view.b.data(), reinterpret_cast<std::byte*>(s16_out.data()) + written, b);
                        written += b;
                    }
                    fifo.commit_write(written);
                }

                const auto t1 = std::chrono::steady_clock::now();
                const double dt_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                update_refill_stats(stats, dt_ms);
            }
        }

        if (!is_flac) {
            if (remaining_bytes == 0 && fifo.size_bytes() == 0) {
                running = false;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - last_log >= std::chrono::seconds(1)) {
            const auto cb = sink.callback_stats();
            const double water_min_ms = bytes_to_ms(stats.min_water, fmt);
            const double water_max_ms = bytes_to_ms(stats.max_water, fmt);
            const double refill_avg_ms = stats.refill_count ? (stats.refill_sum_ms / static_cast<double>(stats.refill_count)) : 0.0;
            const double cb_min_ms = cb.dt_min_ns ? (static_cast<double>(cb.dt_min_ns) / 1e6) : 0.0;
            const double cb_max_ms = cb.dt_max_ns ? (static_cast<double>(cb.dt_max_ns) / 1e6) : 0.0;

            std::printf("cb_frames=%u cb_dt(ms)=%.2f/%.2f/%.2f water(ms)=%.0f..%.0f underrun=%llu refill=%llu refill_ms=%.2f/%.2f/%.2f\n",
                cb.last_request_frames,
                cb_min_ms, cb.dt_avg_ms, cb_max_ms,
                water_min_ms, water_max_ms,
                static_cast<unsigned long long>(stats.underrun_count),
                static_cast<unsigned long long>(stats.refill_count),
                stats.refill_min_ms, refill_avg_ms, stats.refill_max_ms);

            stats.min_water = fifo_capacity;
            stats.max_water = 0;
            last_log = now;
        }

        if (water >= high_water) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    (void)sink.stop();
    sink.close();

    std::printf("[stats] underrun=%llu min=%zu max=%zu refill=%llu overrun=%llu\n",
        static_cast<unsigned long long>(sink.underrun_count()),
        stats.min_water,
        stats.max_water,
        static_cast<unsigned long long>(stats.refill_count),
        static_cast<unsigned long long>(stats.overrun_count));
    return 0;
}
