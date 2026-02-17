module;

#include <cstddef>
#include <cstdint>

export module service_heap;

import util.core;

export namespace service {
    class LinearHeap {
    public:
        LinearHeap(std::byte* buffer, util::usize size) noexcept : buf_(buffer), size_(size) {}

        [[nodiscard]] void* alloc(util::usize bytes) noexcept {
            if (bytes == 0 || offset_ + bytes > size_) {
                return nullptr;
            }
            void* out = buf_ + offset_;
            offset_ += bytes;
            return out;
        }

        void reset() noexcept { offset_ = 0; }
        [[nodiscard]] util::usize used() const noexcept { return offset_; }
        [[nodiscard]] util::usize capacity() const noexcept { return size_; }

    private:
        std::byte* buf_{nullptr};
        util::usize size_{0};
        util::usize offset_{0};
    };
}
