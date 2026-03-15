module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

export module audio.frame_queue;

export namespace audio {
    struct FrameQueueView {
        std::span<std::int32_t> a;
        std::span<std::int32_t> b;
    };

    class FrameQueue {
    public:
        void reset(std::span<std::int32_t> storage, std::uint16_t channels) noexcept {
            storage_ = storage.data();
            capacity_samples_ = storage.size();
            channels_ = channels;
            clear();
        }

        std::uint16_t channels() const noexcept { return channels_; }

        std::size_t capacity_frames() const noexcept {
            if (channels_ == 0) return 0;
            return capacity_samples_ / channels_;
        }

        std::size_t size_frames() const noexcept {
            if (channels_ == 0) return 0;
            return size_samples_ / channels_;
        }

        std::size_t free_frames() const noexcept {
            if (channels_ == 0) return 0;
            return free_samples() / channels_;
        }

        FrameQueueView writable_view() noexcept {
            if (channels_ == 0 || capacity_samples_ == 0) return {};
            std::size_t free = free_samples();
            free -= (free % channels_);
            if (free == 0) return {};
            const std::size_t head = head_samples_;
            std::size_t a_len = std::min(free, capacity_samples_ - head);
            a_len -= (a_len % channels_);
            const std::size_t b_len = free - a_len;
            return {
                std::span<std::int32_t>(storage_ + head, a_len),
                std::span<std::int32_t>(storage_, b_len)
            };
        }

        void commit_write_frames(std::size_t frames) noexcept {
            if (frames == 0 || channels_ == 0 || capacity_samples_ == 0) return;
            const std::size_t samples = frames * channels_;
            if (samples == 0) return;
            head_samples_ = (head_samples_ + samples) % capacity_samples_;
            size_samples_ += samples;
        }

        FrameQueueView readable_view() noexcept {
            if (channels_ == 0 || capacity_samples_ == 0) return {};
            std::size_t used = size_samples_;
            used -= (used % channels_);
            if (used == 0) return {};
            const std::size_t tail = tail_samples_;
            std::size_t a_len = std::min(used, capacity_samples_ - tail);
            a_len -= (a_len % channels_);
            const std::size_t b_len = used - a_len;
            return {
                std::span<std::int32_t>(storage_ + tail, a_len),
                std::span<std::int32_t>(storage_, b_len)
            };
        }

        void commit_read_frames(std::size_t frames) noexcept {
            if (frames == 0 || channels_ == 0 || capacity_samples_ == 0) return;
            const std::size_t samples = frames * channels_;
            if (samples == 0) return;
            tail_samples_ = (tail_samples_ + samples) % capacity_samples_;
            size_samples_ = (samples >= size_samples_) ? 0 : (size_samples_ - samples);
        }

        void clear() noexcept {
            head_samples_ = 0;
            tail_samples_ = 0;
            size_samples_ = 0;
        }

    private:
        std::size_t free_samples() const noexcept {
            return (capacity_samples_ >= size_samples_)
                ? (capacity_samples_ - size_samples_)
                : 0;
        }

        std::int32_t* storage_{nullptr};
        std::size_t capacity_samples_{0};
        std::size_t head_samples_{0};
        std::size_t tail_samples_{0};
        std::size_t size_samples_{0};
        std::uint16_t channels_{0};
    };
}
