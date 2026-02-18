module;

#include <array>
#include <cstdint>
#include <span>

export module service_qr_bitmap;

import alg.qr_encode;

export namespace service::qr {
    constexpr std::int16_t kMaxSize = 33;
    constexpr std::int16_t kMaxStride = (kMaxSize + 7) / 8;

    [[nodiscard]] constexpr int stride_for_size(int size) noexcept
    {
        return (size + 7) / 8;
    }

    [[nodiscard]] constexpr std::size_t required_bytes(int size) noexcept
    {
        return static_cast<std::size_t>(stride_for_size(size)) * static_cast<std::size_t>(size);
    }

    struct Bitmap {
        std::array<std::uint8_t, static_cast<std::size_t>(kMaxSize) * static_cast<std::size_t>(kMaxStride)> buffer{};
        alg::qr::EncoderState state{};
        std::int16_t size{0};
        bool valid{false};
    };

    [[nodiscard]] inline bool encode_to_bitmap(const char* text,
                                               alg::qr::EncoderState& st,
                                               std::span<std::uint8_t> buffer,
                                               std::int16_t& size_out) noexcept
    {
        if (!text || buffer.empty()) return false;
        if (!alg::qr::Encoder::encode(st, text)) return false;

        int qr_size = alg::qr::Encoder::size(st);
        if (qr_size <= 0) return false;
        if (qr_size > kMaxSize) qr_size = kMaxSize;

        size_out = static_cast<std::int16_t>(qr_size);
        const int stride = stride_for_size(qr_size);
        const std::size_t need = required_bytes(qr_size);
        if (buffer.size() < need) return false;

        for (std::size_t i = 0; i < need; ++i) buffer[i] = 0;

        for (int y = 0; y < qr_size; ++y) {
            const int row = y * stride;
            for (int x = 0; x < qr_size; ++x) {
                if (!alg::qr::Encoder::module_on(st, x, y)) continue;
                const int byte_index = x / 8;
                const int bit_index = 7 - (x % 8);
                buffer[static_cast<std::size_t>(row) + static_cast<std::size_t>(byte_index)] |= static_cast<std::uint8_t>(1u << bit_index);
            }
        }
        return true;
    }


    [[nodiscard]] inline bool module_on(std::span<const std::uint8_t> buffer,
                                        std::int16_t size,
                                        int x,
                                        int y) noexcept
    {
        if (size <= 0 || x < 0 || y < 0 || x >= size || y >= size) return false;
        const int stride = stride_for_size(size);
        const std::size_t need = required_bytes(size);
        if (buffer.size() < need) return false;

        const int row = y * stride;
        const int byte_index = x / 8;
        const int bit_index = 7 - (x % 8);
        const std::uint8_t data = buffer[static_cast<std::size_t>(row) + static_cast<std::size_t>(byte_index)];
        return ((data >> bit_index) & 0x1u) != 0;
    }

    [[nodiscard]] inline bool module_on(const Bitmap& bm, int x, int y) noexcept
    {
        return module_on(std::span<const std::uint8_t>{bm.buffer.data(), bm.buffer.size()}, bm.size, x, y);
    }

    [[nodiscard]] inline bool encode(Bitmap& bm, const char* text) noexcept
    {
        bm.valid = encode_to_bitmap(text,
                                    bm.state,
                                    std::span<std::uint8_t>{bm.buffer.data(), bm.buffer.size()},
                                    bm.size);
        return bm.valid;
    }
}
