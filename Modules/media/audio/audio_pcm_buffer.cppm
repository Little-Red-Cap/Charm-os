module;

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <span>

export module audio.pcm_buffer;

import audio.fifo;

export namespace audio {
    inline std::size_t read_pcm_fifo(
        PcmFifo& fifo,
        std::span<std::byte> dst,
        std::size_t frame_size) noexcept {
        if (frame_size == 0) return 0;

        std::size_t need = dst.size() - (dst.size() % frame_size);
        std::size_t filled = 0;

        while (filled < need) {
            auto view = fifo.readable_view();
            if (view.a.empty() && view.b.empty()) break;

            auto copy_one = [&](std::span<std::byte> src) {
                std::size_t n = std::min(src.size(), need - filled);
                n -= n % frame_size;
                if (n == 0) return;
                std::memcpy(dst.data() + filled, src.data(), n);
                fifo.commit_read(n);
                filled += n;
            };

            if (!view.a.empty()) {
                copy_one(view.a);
            } else if (!view.b.empty()) {
                copy_one(view.b);
            }
        }

        return filled;
    }
}
