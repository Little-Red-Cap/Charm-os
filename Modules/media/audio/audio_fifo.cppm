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

    class PcmFifo {
    public:
        explicit PcmFifo(std::size_t capacity_bytes)
            : storage_(capacity_bytes) {}

        std::size_t capacity_bytes() const noexcept { return storage_.size(); }

        std::size_t size_bytes() const noexcept {
            return size_.load(std::memory_order_acquire);
        }

        std::size_t free_bytes() const noexcept {
            return capacity_bytes() - size_bytes();
        }

        PcmFifoView writable_view() noexcept {
            const std::size_t cap = storage_.size();
            const std::size_t head = head_.load(std::memory_order_relaxed);
            const std::size_t free = free_bytes();
            const std::size_t a_len = std::min(free, cap - head);
            const std::size_t b_len = free - a_len;
            return {
                std::span<std::byte>(storage_.data() + head, a_len),
                std::span<std::byte>(storage_.data(), b_len)
            };
        }

        void commit_write(std::size_t bytes) noexcept {
            if (bytes == 0) return;
            const std::size_t cap = storage_.size();
            const std::size_t head = head_.load(std::memory_order_relaxed);
            head_.store((head + bytes) % cap, std::memory_order_release);
            size_.fetch_add(bytes, std::memory_order_release);
        }

        PcmFifoView readable_view() noexcept {
            const std::size_t cap = storage_.size();
            const std::size_t tail = tail_.load(std::memory_order_relaxed);
            const std::size_t size = size_bytes();
            const std::size_t a_len = std::min(size, cap - tail);
            const std::size_t b_len = size - a_len;
            return {
                std::span<std::byte>(storage_.data() + tail, a_len),
                std::span<std::byte>(storage_.data(), b_len)
            };
        }

        void commit_read(std::size_t bytes) noexcept {
            if (bytes == 0) return;
            const std::size_t cap = storage_.size();
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
        std::vector<std::byte> storage_;
        std::atomic<std::size_t> head_{0};
        std::atomic<std::size_t> tail_{0};
        std::atomic<std::size_t> size_{0};
    };
}
