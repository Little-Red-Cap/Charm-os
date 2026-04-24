module;

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <span>

export module audio.pcm_buffer;

import audio.fifo;

export namespace audio {
    struct PcmWaterStats {
        std::size_t min{0};
        std::size_t max{0};
        bool has_sample{false};
    };

    inline void update_water_stats(PcmWaterStats& stats, std::size_t water) noexcept {
        if (!stats.has_sample) {
            stats.min = water;
            stats.max = water;
            stats.has_sample = true;
            return;
        }
        stats.min = std::min(stats.min, water);
        stats.max = std::max(stats.max, water);
    }

    inline std::size_t read_pcm_fifo(
        PcmFifo& fifo,
        std::span<std::byte> dst,
        std::size_t frame_size) noexcept {
        if (frame_size == 0) return 0;

        std::size_t need = dst.size() - (dst.size() % frame_size);
        std::size_t filled = 0;

        while (filled < need) {
            auto view = fifo.consumer_readable_view();
            if (view.a.empty() && view.b.empty()) break;

            auto copy_one = [&](std::span<std::byte> src) {
                std::size_t n = std::min(src.size(), need - filled);
                n -= n % frame_size;
                if (n == 0) return;
                std::memcpy(dst.data() + filled, src.data(), n);
                fifo.consumer_commit_read(n);
                filled += n;
            };

            if (!view.a.empty()) {
                copy_one(view.a);
            } else if (!view.b.empty()) {
                copy_one(view.b);
            }
        }

        return filled;
    }

    inline std::size_t write_pcm_fifo(
        PcmFifo& fifo,
        std::span<const std::byte> src,
        std::size_t frame_size) noexcept {
        if (frame_size == 0) return 0;

        std::size_t need = src.size() - (src.size() % frame_size);
        std::size_t written = 0;

        while (written < need) {
            auto view = fifo.producer_writable_view();
            if (view.a.empty() && view.b.empty()) break;

            auto copy_one = [&](std::span<std::byte> dst) {
                std::size_t n = std::min(dst.size(), need - written);
                n -= n % frame_size;
                if (n == 0) return;
                std::memcpy(dst.data(), src.data() + written, n);
                fifo.producer_commit_write(n);
                written += n;
            };

            if (!view.a.empty()) {
                copy_one(view.a);
            } else if (!view.b.empty()) {
                copy_one(view.b);
            }
        }

        return written;
    }
}
