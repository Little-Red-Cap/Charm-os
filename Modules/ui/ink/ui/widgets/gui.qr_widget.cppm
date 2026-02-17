// gui.qr_widget.cppm
// QR encode + draw helpers (no allocation, user-provided buffer).

module;
#include <array>
#include <cstdint>
#include <span>

export module gui.qr_widget;

import gui.core;
import gui.qr_encode;

export namespace gui::qr {
    using alg::qr::Encoder;
    using alg::qr::EncoderState;
    constexpr std::int16_t kMaxSize = 33;
    constexpr std::int16_t kMaxStride = (kMaxSize + 7) / 8;

    [[nodiscard]] inline bool encode_to_bitmap(const char* text,
                                               EncoderState& st,
                                               std::span<std::uint8_t> buffer,
                                               std::int16_t& size_out) noexcept
    {
        if (!text || buffer.empty()) return false;
        if (!Encoder::encode(st, text)) return false;
        int qr_size = Encoder::size(st);
        if (qr_size <= 0) return false;
        if (qr_size > kMaxSize) qr_size = kMaxSize;
        size_out = (std::int16_t)qr_size;
        const int stride = (qr_size + 7) / 8;
        const std::size_t need = (std::size_t)stride * (std::size_t)qr_size;
        if (buffer.size() < need) return false;
        for (std::size_t i = 0; i < need; ++i) buffer[i] = 0;
        for (int y = 0; y < qr_size; ++y) {
            const int row = y * stride;
            for (int x = 0; x < qr_size; ++x) {
                if (Encoder::module_on(st, x, y)) {
                    const int byte_index = x / 8;
                    const int bit_index = 7 - (x % 8);
                    buffer[(std::size_t)row + (std::size_t)byte_index] |= (std::uint8_t)(1u << bit_index);
                }
            }
        }
        return true;
    }

    template <class R>
    void draw_qr(R& r,
                 const ::gui::Rect& rc,
                 std::span<const std::uint8_t> buffer,
                 std::int16_t size,
                 bool on = true) noexcept
    {
        if (size <= 0 || rc.w <= 0 || rc.h <= 0) return;
        const int stride = (size + 7) / 8;
        const std::size_t need = (std::size_t)stride * (std::size_t)size;
        if (buffer.size() < need) return;

        int scale = rc.w / size;
        if (rc.h / size < scale) scale = rc.h / size;
        if (scale <= 0) return;
        const int draw_w = size * scale;
        const int draw_h = size * scale;
        const int x0 = rc.x + (rc.w - draw_w) / 2;
        const int y0 = rc.y + (rc.h - draw_h) / 2;

        for (int y = 0; y < size; ++y) {
            const int row = y * stride;
            for (int x = 0; x < size; ++x) {
                const int byte_index = x / 8;
                const int bit_index = 7 - (x % 8);
                const std::uint8_t data = buffer[(std::size_t)row + (std::size_t)byte_index];
                if ((data >> bit_index) & 0x1) {
                    const int px = x0 + x * scale;
                    const int py = y0 + y * scale;
                    r.fillRect(::gui::Rect{(std::int16_t)px, (std::int16_t)py,
                                    (std::int16_t)scale, (std::int16_t)scale}, on);
                }
            }
        }
    }

    struct QrCode {
        std::array<std::uint8_t, (std::size_t)kMaxSize * (std::size_t)kMaxStride> buffer{};
        EncoderState state{};
        std::int16_t size{0};
        bool valid{false};

        [[nodiscard]] bool encode(const char* text) noexcept
        {
            valid = encode_to_bitmap(text, state, std::span<std::uint8_t>{buffer.data(), buffer.size()}, size);
            return valid;
        }

        template <class R>
        void draw(R& r, const ::gui::Rect& rc, bool on = true) const noexcept
        {
            if (!valid || size <= 0) return;
            draw_qr(r, rc, std::span<const std::uint8_t>{buffer.data(), buffer.size()}, size, on);
        }
    };
} // namespace gui::qr
