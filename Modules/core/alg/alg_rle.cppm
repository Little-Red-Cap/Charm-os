module;

#include <cstddef>

export module alg_rle;

import util.core;
import util.alias;
import alg_compress;

export namespace alg {
    inline CompResult rle_compress(util::span<const util::u8> in, util::span<util::u8> out) noexcept {
        util::usize w = 0;
        util::usize i = 0;
        while (i < in.size()) {
            const auto value = in[i];
            util::usize run = 1;
            while (i + run < in.size() && in[i + run] == value && run < 255) {
                ++run;
            }
            if (w + 2 > out.size()) {
                return {false, w, CompErr::out_of_space};
            }
            out[w++] = static_cast<util::u8>(run);
            out[w++] = value;
            i += run;
        }
        return {true, w, CompErr::ok};
    }

    inline CompResult rle_decompress(util::span<const util::u8> in, util::span<util::u8> out) noexcept {
        util::usize w = 0;
        util::usize i = 0;
        while (i + 1 < in.size()) {
            const auto run = in[i++];
            const auto value = in[i++];
            if (w + run > out.size()) {
                return {false, w, CompErr::out_of_space};
            }
            for (util::u8 r = 0; r < run; ++r) {
                out[w++] = value;
            }
        }
        if (i != in.size()) {
            return {false, w, CompErr::malformed};
        }
        return {true, w, CompErr::ok};
    }
}
