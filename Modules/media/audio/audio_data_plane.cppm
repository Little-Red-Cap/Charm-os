module;

#include <cstddef>
#include <span>

export module audio.data_plane;

import audio.fifo;
import audio.format;
import audio.pump;

export namespace audio {
    class AudioDataPlane {
    public:
        bool configure(std::span<std::byte> storage,
                       std::size_t fifo_capacity,
                       std::size_t low_water,
                       std::size_t high_water,
                       std::uint32_t period_frames,
                       std::uint32_t chunk_frames,
                       std::size_t chunk_bytes,
                       const AudioFormat& fmt) noexcept {
            if (fifo_capacity != 0 && storage.size() < fifo_capacity) return false;
            fifo_capacity_ = fifo_capacity;
            low_water_ = low_water;
            high_water_ = high_water;
            period_frames_ = period_frames;
            chunk_frames_ = chunk_frames;
            chunk_bytes_ = chunk_bytes;
            fifo_.reset(storage.first(fifo_capacity));
            pump_.bind(fifo_, fmt);
            return true;
        }

        void update_period(std::uint32_t period_frames,
                           std::uint32_t chunk_frames,
                           std::size_t chunk_bytes,
                           const AudioFormat& fmt) noexcept {
            period_frames_ = period_frames;
            chunk_frames_ = chunk_frames;
            chunk_bytes_ = chunk_bytes;
            pump_.bind(fifo_, fmt);
        }

        void reset() noexcept {
            fifo_capacity_ = 0;
            low_water_ = 0;
            high_water_ = 0;
            period_frames_ = 0;
            chunk_frames_ = 0;
            chunk_bytes_ = 0;
            fifo_.clear();
            pump_.reset_stats();
        }

        void clear_fifo() noexcept { fifo_.clear(); }

        PcmFifo& fifo() noexcept { return fifo_; }
        const PcmFifo& fifo() const noexcept { return fifo_; }
        AudioPump& pump() noexcept { return pump_; }
        std::size_t fill(std::span<std::byte> dst) noexcept { return pump_.fill(dst); }

        std::size_t fifo_capacity() const noexcept { return fifo_capacity_; }
        std::size_t low_water() const noexcept { return low_water_; }
        std::size_t high_water() const noexcept { return high_water_; }
        std::uint32_t period_frames() const noexcept { return period_frames_; }
        std::uint32_t chunk_frames() const noexcept { return chunk_frames_; }
        std::size_t chunk_bytes() const noexcept { return chunk_bytes_; }

    private:
        PcmFifo fifo_{};
        AudioPump pump_{};
        std::size_t fifo_capacity_{0};
        std::size_t low_water_{0};
        std::size_t high_water_{0};
        std::uint32_t period_frames_{0};
        std::uint32_t chunk_frames_{0};
        std::size_t chunk_bytes_{0};
    };
}
