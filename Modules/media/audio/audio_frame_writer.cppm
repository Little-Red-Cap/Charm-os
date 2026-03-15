module;

#include <cstddef>
#include <cstdint>
#include <span>

export module audio.frame_writer;

import audio.fifo;

export namespace audio {
    struct FrameWriter {
        FrameWriter(std::int16_t* data,
                    std::size_t capacity_frames,
                    std::uint16_t channels) noexcept
            : data_(data),
              capacity_frames_(capacity_frames),
              channels_(channels) {}

        std::span<std::int16_t> writable(std::size_t frames) noexcept {
            if (!data_ || channels_ == 0) return {};
            if (frames == 0) return {};
            const std::size_t remaining = (capacity_frames_ > written_frames_)
                ? (capacity_frames_ - written_frames_)
                : 0;
            if (frames > remaining) return {};
            const std::size_t samples = frames * channels_;
            return std::span<std::int16_t>(data_ + written_frames_ * channels_, samples);
        }

        void commit(std::size_t frames) noexcept {
            const std::size_t remaining = (capacity_frames_ > written_frames_)
                ? (capacity_frames_ - written_frames_)
                : 0;
            const std::size_t clamped = (frames > remaining) ? remaining : frames;
            written_frames_ += clamped;
        }

        std::size_t written_frames() const noexcept { return written_frames_; }

        std::span<const std::byte> written_bytes() const noexcept {
            const std::size_t samples = written_frames_ * channels_;
            return std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(data_),
                samples * sizeof(std::int16_t));
        }

    private:
        std::int16_t* data_{nullptr};
        std::size_t capacity_frames_{0};
        std::size_t written_frames_{0};
        std::uint16_t channels_{0};
    };

    using BufferWriter = FrameWriter;

    struct FifoWriter {
        explicit FifoWriter(PcmFifo& fifo, std::uint16_t channels) noexcept
            : fifo_(fifo),
              channels_(channels),
              bytes_per_frame_(static_cast<std::size_t>(channels) * sizeof(std::int16_t)) {}

        std::span<std::int16_t> writable(std::size_t frames) noexcept {
            if (channels_ == 0 || bytes_per_frame_ == 0) return {};
            auto view = fifo_.writable_view();
            if (view.a.empty() && view.b.empty()) return {};
            const std::size_t cap_bytes = view.a.size();
            const std::size_t max_frames = cap_bytes / bytes_per_frame_;
            if (max_frames == 0) return {};
            const std::size_t use_frames = (frames > max_frames) ? max_frames : frames;
            auto* ptr = reinterpret_cast<std::int16_t*>(view.a.data());
            return std::span<std::int16_t>(ptr, use_frames * channels_);
        }

        void commit(std::size_t frames) noexcept {
            if (frames == 0 || bytes_per_frame_ == 0) return;
            const std::size_t bytes = frames * bytes_per_frame_;
            fifo_.commit_write(bytes);
            written_frames_ += frames;
        }

        std::size_t written_frames() const noexcept { return written_frames_; }
        std::size_t written_bytes() const noexcept { return written_frames_ * bytes_per_frame_; }

    private:
        PcmFifo& fifo_;
        std::size_t written_frames_{0};
        std::uint16_t channels_{0};
        std::size_t bytes_per_frame_{0};
    };
}
