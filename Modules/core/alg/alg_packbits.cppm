module;

#include <cstddef>

export module alg_packbits;

import util.core;
import util.alias;
import alg_compress;

export namespace alg {
    inline CompResult packbits_compress(util::span<const util::u8> in, util::span<util::u8> out) noexcept {
        util::usize w = 0;
        util::usize i = 0;
        while (i < in.size()) {
            util::usize run = 1;
            while (i + run < in.size() && in[i + run] == in[i] && run < 128) {
                ++run;
            }
            if (run >= 3) {
                if (w + 2 > out.size()) return {false, w, CompErr::out_of_space};
                out[w++] = static_cast<util::u8>(257 - run);
                out[w++] = in[i];
                i += run;
                continue;
            }
            util::usize lit_start = i;
            util::usize lit_count = 0;
            while (i < in.size() && lit_count < 128) {
                util::usize r = 1;
                while (i + r < in.size() && in[i + r] == in[i] && r < 128) {
                    ++r;
                }
                if (r >= 3) break;
                ++i;
                ++lit_count;
            }
            if (w + 1 + lit_count > out.size()) return {false, w, CompErr::out_of_space};
            out[w++] = static_cast<util::u8>(lit_count - 1);
            for (util::usize j = 0; j < lit_count; ++j) {
                out[w++] = in[lit_start + j];
            }
        }
        return {true, w, CompErr::ok};
    }

    inline CompResult packbits_decompress(util::span<const util::u8> in, util::span<util::u8> out) noexcept {
        util::usize w = 0;
        util::usize i = 0;
        while (i < in.size()) {
            const auto code = static_cast<int>(static_cast<signed char>(in[i++]));
            if (code >= 0) {
                const util::usize count = static_cast<util::usize>(code) + 1;
                if (i + count > in.size()) return {false, w, CompErr::malformed};
                if (w + count > out.size()) return {false, w, CompErr::out_of_space};
                for (util::usize j = 0; j < count; ++j) {
                    out[w++] = in[i++];
                }
            } else if (code >= -127) {
                const util::usize count = static_cast<util::usize>(1 - code);
                if (i >= in.size()) return {false, w, CompErr::malformed};
                const auto value = in[i++];
                if (w + count > out.size()) return {false, w, CompErr::out_of_space};
                for (util::usize j = 0; j < count; ++j) {
                    out[w++] = value;
                }
            } else {
                // -128: noop
            }
        }
        return {true, w, CompErr::ok};
    }
}
