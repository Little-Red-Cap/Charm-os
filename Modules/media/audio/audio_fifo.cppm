module;

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

export module audio.fifo;

export namespace audio {
    struct PcmFifoView {
        std::span<std::byte> a;
        std::span<std::byte> b;
    };

    // Single-producer/single-consumer ring buffer. Keep producer/consumer roles explicit.
    class PcmFifo {
    public:
        PcmFifo() noexcept { }

        explicit PcmFifo(std::size_t capacity_bytes)
            : owned_(capacity_bytes) {
            storage_ = owned_.data();
            capacity_ = owned_.size();
        }

        explicit PcmFifo(std::span<std::byte> storage) {
            reset(storage);
        }

        void reset(std::span<std::byte> storage) noexcept {
            owned_.clear();
            storage_ = storage.data();
            capacity_ = storage.size();
            clear();
        }

        std::size_t capacity_bytes() const noexcept { return capacity_; }

        std::size_t size_bytes() const noexcept {
            return size_.load(std::memory_order_acquire);
        }

        std::size_t free_bytes() const noexcept {
            return capacity_bytes() - size_bytes();
        }

        std::size_t producer_free_bytes() const noexcept { return free_bytes(); }
        PcmFifoView producer_writable_view() noexcept { return writable_view(); }
        void producer_commit_write(std::size_t bytes) noexcept { commit_write(bytes); }

        std::size_t consumer_size_bytes() const noexcept { return size_bytes(); }
        PcmFifoView consumer_readable_view() noexcept { return readable_view(); }
        void consumer_commit_read(std::size_t bytes) noexcept { commit_read(bytes); }

        PcmFifoView writable_view() noexcept {
            const std::size_t cap = capacity_;
            if (cap == 0) return {};
            const std::size_t head = head_.load(std::memory_order_relaxed);
            const std::size_t free = free_bytes();
            const std::size_t a_len = std::min(free, cap - head);
            const std::size_t b_len = free - a_len;
            return {
                std::span<std::byte>(storage_ + head, a_len),
                std::span<std::byte>(storage_, b_len)
            };
        }

        void commit_write(std::size_t bytes) noexcept {
            if (bytes == 0) return;
            const std::size_t cap = capacity_;
            if (cap == 0) return;
            const std::size_t head = head_.load(std::memory_order_relaxed);
            head_.store((head + bytes) % cap, std::memory_order_release);
            size_.fetch_add(bytes, std::memory_order_release);
        }

        PcmFifoView readable_view() noexcept {
            const std::size_t cap = capacity_;
            if (cap == 0) return {};
            const std::size_t tail = tail_.load(std::memory_order_relaxed);
            const std::size_t size = size_bytes();
            const std::size_t a_len = std::min(size, cap - tail);
            const std::size_t b_len = size - a_len;
            return {
                std::span<std::byte>(storage_ + tail, a_len),
                std::span<std::byte>(storage_, b_len)
            };
        }

        void commit_read(std::size_t bytes) noexcept {
            if (bytes == 0) return;
            const std::size_t cap = capacity_;
            if (cap == 0) return;
            const std::size_t tail = tail_.load(std::memory_order_relaxed);
            tail_.store((tail + bytes) % cap, std::memory_order_release);
            size_.fetch_sub(bytes, std::memory_order_release);
        }

        void clear() noexcept {
            head_.store(0, std::memory_order_release);
            tail_.store(0, std::memory_order_release);
            size_.store(0, std::memory_order_release);
        }

    private:
        std::vector<std::byte> owned_{};
        std::byte* storage_{nullptr};
        std::size_t capacity_{0};
        std::atomic<std::size_t> head_{0};
        std::atomic<std::size_t> tail_{0};
        std::atomic<std::size_t> size_{0};
    };
}
