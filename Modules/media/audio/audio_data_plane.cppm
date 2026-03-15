module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

export module audio.data_plane;

import audio.decode_pipe;
import audio.eq;
import audio.fifo;
import audio.format;
import audio.pcm_buffer;
import audio.pump;
import audio.result;
import media.stream.source;

export namespace audio {
    class AudioDataPlane {
    public:
        Result<void> open_source(media::StreamSourceRef src, SourceKind kind) noexcept {
            return decode_.open(src, kind);
        }

        void close_source() noexcept { decode_.close(); }

        Result<void> seek_frames(std::uint64_t frames) noexcept {
            return decode_.seek_frames(frames);
        }

        bool configure_output(std::uint16_t force_channels,
                              bool fixed_rate,
                              std::uint32_t fixed_rate_value) noexcept {
            return decode_.configure_output(force_channels, fixed_rate, fixed_rate_value);
        }

        void set_eq(const EqConfig& eq) noexcept { decode_.set_eq(eq); }
        void maybe_update_eq() noexcept { decode_.maybe_update_eq(); }
        void set_volume_gain(float gain) noexcept { decode_.set_volume_gain(gain); }
        void reset_fade(std::uint64_t frames) noexcept { decode_.reset_fade(frames); }

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
            return decode_.configure_buffers(chunk_frames, chunk_bytes);
        }

        bool update_period(std::uint32_t period_frames,
                           std::uint32_t chunk_frames,
                           std::size_t chunk_bytes,
                           const AudioFormat& fmt) noexcept {
            period_frames_ = period_frames;
            chunk_frames_ = chunk_frames;
            chunk_bytes_ = chunk_bytes;
            pump_.bind(fifo_, fmt);
            return decode_.configure_buffers(chunk_frames, chunk_bytes);
        }

        void reset() noexcept {
            fifo_capacity_ = 0;
            low_water_ = 0;
            high_water_ = 0;
            period_frames_ = 0;
            chunk_frames_ = 0;
            chunk_bytes_ = 0;
            last_written_bytes_ = 0;
            fifo_.clear();
            pump_.reset_stats();
        }

        void clear_fifo() noexcept { fifo_.clear(); }

        std::size_t refill() noexcept {
            last_written_bytes_ = 0;
            if (fifo_capacity_ == 0 || chunk_bytes_ == 0) return 0;
            const auto& fmt = decode_.output_format();
            if (fmt.frame_size() == 0) return 0;
            std::size_t writable = std::min(fifo_.free_bytes(), chunk_bytes_);
            writable = (writable / fmt.frame_size()) * fmt.frame_size();
            if (writable == 0) return 0;
            const std::size_t frames_needed = writable / fmt.frame_size();
            const std::size_t decoded_bytes = decode_.decode_frames(frames_needed);
            if (decoded_bytes == 0) return 0;
            auto out = decode_.output_bytes();
            const std::size_t to_write = std::min(decoded_bytes, out.size());
            const std::size_t written = write_pcm_fifo(
                fifo_,
                out.first(to_write),
                fmt.frame_size());
            last_written_bytes_ = written;
            return written;
        }

        std::span<const std::byte> last_output() const noexcept {
            if (last_written_bytes_ == 0) return {};
            auto out = decode_.output_bytes();
            const std::size_t count = std::min(last_written_bytes_, out.size());
            return out.first(count);
        }

        bool has_more_data() const noexcept { return decode_.has_more_data(); }
        std::uint64_t total_frames() const noexcept { return decode_.total_frames(); }
        const AudioFormat& input_format() const noexcept { return decode_.input_format(); }
        const AudioFormat& output_format() const noexcept { return decode_.output_format(); }

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
        AudioDecodePipe decode_{};
        PcmFifo fifo_{};
        AudioPump pump_{};
        std::size_t fifo_capacity_{0};
        std::size_t low_water_{0};
        std::size_t high_water_{0};
        std::uint32_t period_frames_{0};
        std::uint32_t chunk_frames_{0};
        std::size_t chunk_bytes_{0};
        std::size_t last_written_bytes_{0};
    };
}
