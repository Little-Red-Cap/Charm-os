module;

#include <array>
#include <cstddef>
#include <cstdint>

export module daplink.ring_buffer;

export namespace daplink::ring_buffer {
    template <std::size_t N>
    struct Buffer {
        static_assert(N > 0);
        static_assert((N & (N - 1)) == 0);

        std::array<std::uint8_t, N> data{};
        std::uint16_t head = 0;
        std::uint16_t tail = 0;

        constexpr std::size_t mask() const noexcept {
            return N - 1;
        }

        bool empty() const noexcept {
            return head == tail;
        }

        bool full() const noexcept {
            return static_cast<std::uint16_t>((head + 1) & mask()) == tail;
        }

        bool push(const std::uint8_t value) noexcept {
            if (full()) {
                return false;
            }
            data[head] = value;
            head = static_cast<std::uint16_t>((head + 1) & mask());
            return true;
        }

        bool pop(std::uint8_t& value) noexcept {
            if (empty()) {
                return false;
            }
            value = data[tail];
            tail = static_cast<std::uint16_t>((tail + 1) & mask());
            return true;
        }

        std::uint16_t count() const noexcept {
            return static_cast<std::uint16_t>((head - tail) & mask());
        }

        std::uint16_t peek(std::uint8_t* dst, const std::uint16_t max_len) const noexcept {
            const std::uint16_t available = count();
            const std::uint16_t len = (available < max_len) ? available : max_len;
            std::uint16_t index = tail;
            for (std::uint16_t i = 0; i < len; ++i) {
                dst[i] = data[index];
                index = static_cast<std::uint16_t>((index + 1) & mask());
            }
            return len;
        }

        void drop(const std::uint16_t len) noexcept {
            tail = static_cast<std::uint16_t>((tail + len) & mask());
        }
    };
}
