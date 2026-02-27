module;

#include <span>
#include <cstddef>

export module alg_heatshrink;

import util.core;
import alg_compress;

export namespace alg {
    struct BitWriter {
        std::span<util::u8> out{};
        util::usize bitpos{0};
        util::usize written{0};

        bool put_bit(bool bit) noexcept {
            const util::usize byte = bitpos / 8;
            const util::usize off = bitpos % 8;
            if (byte >= out.size()) return false;
            if (off == 0) out[byte] = 0;
            if (bit) out[byte] |= static_cast<util::u8>(1u << off);
            ++bitpos;
            if (byte + 1 > written) written = byte + 1;
            return true;
        }

        bool put_bits(util::u32 value, util::u32 count) noexcept {
            for (util::u32 i = 0; i < count; ++i) {
                if (!put_bit(((value >> i) & 1u) != 0)) return false;
            }
            return true;
        }
    };

    struct BitReader {
        std::span<const util::u8> in{};
        util::usize bitpos{0};

        bool get_bit(bool& bit) noexcept {
            const util::usize byte = bitpos / 8;
            const util::usize off = bitpos % 8;
            if (byte >= in.size()) return false;
            bit = ((in[byte] >> off) & 1u) != 0;
            ++bitpos;
            return true;
        }

        bool get_bits(util::u32& value, util::u32 count) noexcept {
            value = 0;
            for (util::u32 i = 0; i < count; ++i) {
                bool bit = false;
                if (!get_bit(bit)) return false;
                if (bit) value |= (1u << i);
            }
            return true;
        }
    };

    template <util::u32 WindowBits, util::u32 LookaheadBits>
    inline CompResult heatshrink_compress(std::span<const util::u8> in, std::span<util::u8> out) noexcept {
        constexpr util::u32 window_size = 1u << WindowBits;
        constexpr util::u32 max_len = (1u << LookaheadBits) + 1u;
        BitWriter bw{out};

        util::usize i = 0;
        while (i < in.size()) {
            util::u32 best_len = 0;
            util::u32 best_off = 0;
            const util::usize win_start = (i > window_size) ? (i - window_size) : 0;
            for (util::usize j = win_start; j < i; ++j) {
                util::u32 len = 0;
                while (i + len < in.size()
                    && in[j + len] == in[i + len]
                    && len < max_len) {
                    ++len;
                }
                if (len > best_len && len >= 3) {
                    best_len = len;
                    best_off = static_cast<util::u32>(i - j);
                }
            }
            if (best_len >= 3) {
                if (!bw.put_bit(true)) return {false, bw.written, CompErr::out_of_space};
                if (!bw.put_bits(best_off - 1, WindowBits)) return {false, bw.written, CompErr::out_of_space};
                if (!bw.put_bits(best_len - 3, LookaheadBits)) return {false, bw.written, CompErr::out_of_space};
                i += best_len;
            } else {
                if (!bw.put_bit(false)) return {false, bw.written, CompErr::out_of_space};
                if (!bw.put_bits(in[i], 8)) return {false, bw.written, CompErr::out_of_space};
                ++i;
            }
        }
        return {true, bw.written, CompErr::ok};
    }

    template <util::u32 WindowBits, util::u32 LookaheadBits>
    inline CompResult heatshrink_decompress(std::span<const util::u8> in, std::span<util::u8> out) noexcept {
        constexpr util::u32 window_size = 1u << WindowBits;
        constexpr util::u32 max_len = (1u << LookaheadBits) + 1u;
        (void)window_size;
        (void)max_len;
        BitReader br{in};
        util::usize w = 0;
        while (true) {
            bool tag = false;
            if (!br.get_bit(tag)) break;
            if (!tag) {
                util::u32 value = 0;
                if (!br.get_bits(value, 8)) return {false, w, CompErr::malformed};
                if (w >= out.size()) return {false, w, CompErr::out_of_space};
                out[w++] = static_cast<util::u8>(value);
            } else {
                util::u32 off = 0;
                util::u32 len = 0;
                if (!br.get_bits(off, WindowBits)) return {false, w, CompErr::malformed};
                if (!br.get_bits(len, LookaheadBits)) return {false, w, CompErr::malformed};
                off += 1;
                len += 3;
                if (off == 0 || off > w) return {false, w, CompErr::malformed};
                if (w + len > out.size()) return {false, w, CompErr::out_of_space};
                const util::usize start = w - off;
                for (util::u32 i = 0; i < len; ++i) {
                    out[w++] = out[start + i];
                }
            }
        }
        return {true, w, CompErr::ok};
    }
}
