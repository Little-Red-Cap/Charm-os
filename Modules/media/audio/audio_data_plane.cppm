module;

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

export module audio.data_plane;

import audio.decode_pipe;
import audio.dsp_graph;
import audio.eq;
import audio.fifo;
import audio.frame_writer;
import audio.frame_queue;
import audio.format;
import audio.pcm_buffer;
import audio.pump;
import audio.result;
import media.stream.source;

export namespace audio {
#ifndef CHARM_AUDIO_MAX_CHANNELS
#define CHARM_AUDIO_MAX_CHANNELS 2
#endif
#ifndef CHARM_AUDIO_MAX_PERIOD_FRAMES
#define CHARM_AUDIO_MAX_PERIOD_FRAMES 480
#endif
#ifndef CHARM_AUDIO_MAX_CHUNK_MULT
#define CHARM_AUDIO_MAX_CHUNK_MULT 10
#endif

    class AudioDataPlane {
    public:
        Result<void> open_source(media::StreamSourceRef src, SourceKind kind) noexcept {
            frame_queue_.clear();
            return decode_.open(src, kind);
        }

        void close_source() noexcept {
            decode_.close();
            frame_queue_.clear();
            last_written_bytes_ = 0;
        }

        Result<void> seek_frames(std::uint64_t frames) noexcept {
            frame_queue_.clear();
            last_written_bytes_ = 0;
            return decode_.seek_frames(frames);
        }

        bool configure_output(std::uint16_t force_channels,
                              bool fixed_rate,
                              std::uint32_t fixed_rate_value) noexcept {
            return decode_.configure_output(force_channels, fixed_rate, fixed_rate_value);
        }

        void set_eq(const EqConfig& eq) noexcept { decode_.set_eq(eq); }
        void maybe_update_eq() noexcept { decode_.maybe_update_eq(); }
        void set_volume_gain(float gain) noexcept { graph_.set_gain(gain); }
        void reset_fade(std::uint64_t frames) noexcept { graph_.reset_fade(frames); }
        void enable_dc_block(bool on) noexcept { graph_.enable_dc_block(on); }
        void enable_soft_clip(bool on) noexcept { graph_.enable_soft_clip(on); }
        void set_soft_clip_threshold(float threshold) noexcept {
            graph_.set_soft_clip_threshold(threshold);
        }
        void set_graph_block_frames(std::uint32_t frames) noexcept {
            graph_block_frames_ = frames;
        }

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
            if (!prepare_frame_queue(fmt.channels)) return false;
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
            if (!prepare_frame_queue(fmt.channels)) return false;
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
            graph_block_frames_ = 0;
            fifo_.clear();
            pump_.reset_stats();
            frame_queue_.clear();
            graph_.reset(0);
        }

        void clear_fifo() noexcept {
            fifo_.clear();
            frame_queue_.clear();
            last_written_bytes_ = 0;
        }

        std::size_t refill() noexcept {
            last_written_bytes_ = 0;
            if (fifo_capacity_ == 0 || chunk_bytes_ == 0) return 0;
            const auto& fmt = decode_.output_format();
            if (fmt.frame_size() == 0 || fmt.channels == 0) return 0;
            std::size_t writable = std::min(fifo_.free_bytes(), chunk_bytes_);
            writable = (writable / fmt.frame_size()) * fmt.frame_size();
            if (writable == 0) return 0;
            std::size_t frames_needed = writable / fmt.frame_size();
            frames_needed = std::min<std::size_t>(frames_needed, chunk_frames_);
            if (frames_needed == 0) return 0;

            if (frame_queue_.size_frames() < frames_needed && decode_.has_more_data()) {
                const std::size_t free_frames = frame_queue_.free_frames();
                const std::size_t decode_cap = std::min<std::size_t>(free_frames, chunk_frames_);
                if (decode_cap > 0) {
                    const std::size_t decoded_frames = decode_.decode_frames(decode_cap);
                    if (decoded_frames > 0) {
                        write_queue_frames(decode_.output_frames(), decoded_frames);
                    }
                }
            }

            const std::size_t available_frames = frame_queue_.size_frames();
            const std::size_t frames_to_output = std::min(frames_needed, available_frames);
            if (frames_to_output == 0) return 0;

            FrameWriter writer{s16_out_.data(), s16_out_.size() / fmt.channels, fmt.channels};
            const std::size_t frames_written =
                consume_queue_frames(frames_to_output, fmt.channels, writer);
            if (frames_written == 0) return 0;
            const auto out = writer.written_bytes();
            const std::size_t written = write_pcm_fifo(fifo_, out, fmt.frame_size());
            last_written_bytes_ = written;
            return written;
        }

        std::span<const std::byte> last_output() const noexcept {
            if (last_written_bytes_ == 0) return {};
            const std::size_t max_bytes = s16_out_.size() * sizeof(std::int16_t);
            const std::size_t count = std::min(last_written_bytes_, max_bytes);
            return std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(s16_out_.data()),
                count);
        }

        bool has_more_data() const noexcept {
            return decode_.has_more_data() || frame_queue_.size_frames() > 0;
        }
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
        bool prepare_frame_queue(std::uint16_t channels) noexcept {
            if (channels == 0) return false;
            if (chunk_frames_ == 0) return false;
            if (chunk_frames_ > kMaxChunkFrames) return false;
            const std::size_t out_samples = static_cast<std::size_t>(chunk_frames_) * channels;
            if (out_samples > s16_out_.size()) return false;
            frame_queue_.reset(
                std::span<std::int32_t>(frame_queue_storage_.data(), frame_queue_storage_.size()),
                channels);
            graph_.reset(channels);
            return true;
        }

        std::size_t write_queue_frames(std::span<const std::int32_t> src, std::size_t frames) noexcept {
            const std::uint16_t channels = frame_queue_.channels();
            if (channels == 0 || frames == 0) return 0;
            std::size_t samples = frames * channels;
            if (samples > src.size()) {
                samples = src.size() - (src.size() % channels);
            }
            if (samples == 0) return 0;

            std::size_t remaining = samples;
            std::size_t offset = 0;
            auto view = frame_queue_.writable_view();
            auto copy_span = [&](std::span<std::int32_t> dst) {
                const std::size_t n = std::min(dst.size(), remaining);
                if (n == 0) return;
                std::memcpy(dst.data(), src.data() + offset, n * sizeof(std::int32_t));
                offset += n;
                remaining -= n;
            };
            if (!view.a.empty()) {
                copy_span(view.a);
            }
            if (remaining > 0 && !view.b.empty()) {
                copy_span(view.b);
            }

            const std::size_t written_samples = samples - remaining;
            const std::size_t written_frames = written_samples / channels;
            frame_queue_.commit_write_frames(written_frames);
            return written_frames;
        }

        std::size_t consume_queue_frames(std::size_t frames,
                                         std::uint16_t channels,
                                         FrameWriter& writer) noexcept {
            if (frames == 0 || channels == 0) return 0;
            std::size_t remaining = frames;

            bool writer_full = false;
            while (remaining > 0 && !writer_full) {
                auto view = frame_queue_.readable_view();
                if (view.a.empty() && view.b.empty()) break;

                auto consume_span = [&](std::span<std::int32_t> src) {
                    std::size_t frames_avail = std::min(remaining, src.size() / channels);
                    if (graph_block_frames_ != 0 && graph_block_frames_ < frames_avail) {
                        frames_avail = graph_block_frames_;
                    }
                    if (frames_avail == 0) return;
                    const std::size_t samples = frames_avail * channels;
                    auto slice = src.first(samples);
                    graph_.process(slice, frames_avail);
                    auto dst = writer.writable(frames_avail);
                    if (dst.empty()) {
                        writer_full = true;
                        return;
                    }
                    quantize_s32(slice, dst);
                    writer.commit(frames_avail);
                    remaining -= frames_avail;
                    frame_queue_.commit_read_frames(frames_avail);
                };

                if (!view.a.empty()) {
                    consume_span(view.a);
                } else if (!view.b.empty()) {
                    consume_span(view.b);
                }
            }

            return writer.written_frames();
        }

        static void quantize_s32(std::span<const std::int32_t> src,
                                 std::span<std::int16_t> dst) noexcept {
            const std::size_t samples = std::min(src.size(), dst.size());
            for (std::size_t i = 0; i < samples; ++i) {
                const std::int32_t v = std::clamp(
                    src[i],
                    static_cast<std::int32_t>(-32768 << 16),
                    static_cast<std::int32_t>(32767 << 16));
                dst[i] = static_cast<std::int16_t>(v >> 16);
            }
        }

        static constexpr std::size_t kMaxChannels = CHARM_AUDIO_MAX_CHANNELS;
        static constexpr std::size_t kMaxPeriodFrames = CHARM_AUDIO_MAX_PERIOD_FRAMES;
        static constexpr std::size_t kMaxChunkMult = CHARM_AUDIO_MAX_CHUNK_MULT;
        static constexpr std::size_t kMaxChunkFrames = kMaxPeriodFrames * kMaxChunkMult;
        static constexpr std::size_t kFrameQueueSamples = kMaxChunkFrames * kMaxChannels * 2;

        AudioDecodePipe decode_{};
        PcmFifo fifo_{};
        AudioPump pump_{};
        FrameQueue frame_queue_{};
        DspGraph graph_{};
        std::size_t fifo_capacity_{0};
        std::size_t low_water_{0};
        std::size_t high_water_{0};
        std::uint32_t period_frames_{0};
        std::uint32_t chunk_frames_{0};
        std::size_t chunk_bytes_{0};
        std::size_t last_written_bytes_{0};
        std::uint32_t graph_block_frames_{0};

        std::array<std::int32_t, kFrameQueueSamples> frame_queue_storage_{};
        std::array<std::int16_t, kMaxChunkFrames * kMaxChannels> s16_out_{};
    };
}
