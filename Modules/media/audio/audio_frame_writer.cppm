module;

#include <cstddef>
#include <cstdint>
#include <span>

export module audio.frame_writer;

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
}
