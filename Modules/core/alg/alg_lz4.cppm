module;

#include <cstddef>

export module alg_lz4;

import util.core;
import util.alias;
import alg_compress;

export namespace alg {
    inline CompResult lz4_compress(util::span<const util::u8> in, util::span<util::u8> out) noexcept {
        util::usize w = 0;
        util::usize i = 0;
        const util::usize n = in.size();
        const util::usize window = 65535;
        while (i < n) {
            util::usize best_len = 0;
            util::usize best_off = 0;
            const util::usize start = (i > window) ? (i - window) : 0;
            for (util::usize j = start; j + 4 <= i; ++j) {
                util::usize len = 0;
                while (i + len < n && in[j + len] == in[i + len] && len < 255 + 15 + 4) {
                    ++len;
                }
                if (len >= 4 && len > best_len) {
                    best_len = len;
                    best_off = i - j;
                }
            }
            if (best_len >= 4) {
                const util::usize lit_len = 0;
                const util::usize match_len = best_len - 4;
                const util::u8 token = static_cast<util::u8>((lit_len << 4) | (match_len < 15 ? match_len : 15));
                if (w + 1 + 2 > out.size()) return {false, w, CompErr::out_of_space};
                out[w++] = token;
                out[w++] = static_cast<util::u8>(best_off & 0xFF);
                out[w++] = static_cast<util::u8>((best_off >> 8) & 0xFF);
                if (match_len >= 15) {
                    util::usize len = match_len - 15;
                    while (len >= 255) {
                        if (w >= out.size()) return {false, w, CompErr::out_of_space};
                        out[w++] = 255;
                        len -= 255;
                    }
                    if (w >= out.size()) return {false, w, CompErr::out_of_space};
                    out[w++] = static_cast<util::u8>(len);
                }
                i += best_len;
            } else {
                const util::usize lit_len = 1;
                const util::u8 token = static_cast<util::u8>(lit_len << 4);
                if (w + 1 + lit_len > out.size()) return {false, w, CompErr::out_of_space};
                out[w++] = token;
                out[w++] = in[i];
                ++i;
            }
        }
        return {true, w, CompErr::ok};
    }

    inline CompResult lz4_decompress(util::span<const util::u8> in, util::span<util::u8> out) noexcept {
        util::usize r = 0;
        util::usize w = 0;
        while (r < in.size()) {
            const util::u8 token = in[r++];
            util::usize lit_len = token >> 4;
            if (lit_len == 15) {
                while (r < in.size()) {
                    const util::u8 v = in[r++];
                    lit_len += v;
                    if (v != 255) break;
                }
            }
            if (r + lit_len > in.size()) return {false, w, CompErr::malformed};
            if (w + lit_len > out.size()) return {false, w, CompErr::out_of_space};
            for (util::usize i = 0; i < lit_len; ++i) {
                out[w++] = in[r++];
            }
            if (r >= in.size()) break;
            if (r + 2 > in.size()) return {false, w, CompErr::malformed};
            const util::usize off = static_cast<util::usize>(in[r]) | (static_cast<util::usize>(in[r + 1]) << 8);
            r += 2;
            if (off == 0 || off > w) return {false, w, CompErr::malformed};
            util::usize match_len = token & 0xF;
            if (match_len == 15) {
                while (r < in.size()) {
                    const util::u8 v = in[r++];
                    match_len += v;
                    if (v != 255) break;
                }
            }
            match_len += 4;
            if (w + match_len > out.size()) return {false, w, CompErr::out_of_space};
            const util::usize start = w - off;
            for (util::usize i = 0; i < match_len; ++i) {
                out[w++] = out[start + i];
            }
        }
        return {true, w, CompErr::ok};
    }
}
