// gui.qr_widget.cppm
// QR encode + draw helpers (no allocation, user-provided buffer).

module;
#include <cstdint>
#include <span>

export module gui.qr_widget;

import gui.core;
import alg.qr_encode;
import service_qr_bitmap;

export namespace gui::qr {
    constexpr std::int16_t kMaxSize = service::qr::kMaxSize;
    constexpr std::int16_t kMaxStride = service::qr::kMaxStride;

    [[nodiscard]] inline bool encode_to_bitmap(const char* text,
                                               alg::qr::EncoderState& st,
                                               std::span<std::uint8_t> buffer,
                                               std::int16_t& size_out) noexcept
    {
        return service::qr::encode_to_bitmap(text, st, buffer, size_out);
    }

    template <class R>
    void draw_qr(R& r,
                 const Rect& rc,
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
                    r.fillRect(Rect{(std::int16_t)px, (std::int16_t)py,
                                    (std::int16_t)scale, (std::int16_t)scale}, on);
                }
            }
        }
    }

    struct QrCode {
        service::qr::Bitmap bitmap{};

        [[nodiscard]] bool encode(const char* text) noexcept
        {
            return service::qr::encode(bitmap, text);
        }

        template <class R>
        void draw(R& r, const Rect& rc, bool on = true) const noexcept
        {
            if (!bitmap.valid || bitmap.size <= 0) return;
            draw_qr(r,
                    rc,
                    std::span<const std::uint8_t>{bitmap.buffer.data(), bitmap.buffer.size()},
                    bitmap.size,
                    on);
        }
    };
} // namespace gui::qr
