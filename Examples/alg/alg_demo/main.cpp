#include <array>
#include <complex>
#include <cstdio>
#include <span>

import util.core;
import alg_fft;
import alg_filters;
import alg_stats;
import alg_color;
import alg_compress;
import alg_rle;
import alg_packbits;
import alg_heatshrink;
import alg_lz4;

int main() {
    std::array<std::complex<double>, 4> sig{
        std::complex<double>{1.0, 0.0},
        std::complex<double>{0.0, 0.0},
        std::complex<double>{-1.0, 0.0},
        std::complex<double>{0.0, 0.0}
    };
    alg::fft_inplace(sig);
    std::printf("[fft] %0.2f %0.2f %0.2f %0.2f\n",
                sig[0].real(), sig[1].real(), sig[2].real(), sig[3].real());

    alg::Ewma ewma{0.2};
    (void)ewma.update(10.0);
    (void)ewma.update(12.0);
    std::printf("[ewma] %0.2f\n", ewma.value());

    alg::Kalman1D kf{1e-3, 1e-2};
    (void)kf.update(10.0);
    (void)kf.update(9.5);
    std::printf("[kalman] %0.2f\n", kf.value());

    alg::Biquad biquad{};
    biquad.set_coeffs(alg::Biquad::lowpass(1000.0, 50.0));
    const auto y = biquad.process(1.0);
    std::printf("[biquad] %0.2f\n", y);

    std::array<int, 5> data{5, 1, 9, 3, 7};
    std::printf("[mean] %0.2f\n", alg::mean<int>(data));
    std::printf("[median] %d\n", alg::median<int, 5>(data));

    const alg::Rgb rgb{64, 128, 255};
    const auto yuv = alg::rgb_to_yuv601(rgb);
    const auto rgb2 = alg::yuv_to_rgb601(yuv);
    std::printf("[yuv] y=%.1f u=%.1f v=%.1f -> rgb=%u,%u,%u\n",
                yuv.y, yuv.u, yuv.v, rgb2.r, rgb2.g, rgb2.b);

    const auto hsv = alg::rgb_to_hsv(rgb);
    const auto rgb3 = alg::hsv_to_rgb(hsv);
    std::printf("[hsv] h=%.1f s=%.2f v=%.2f -> rgb=%u,%u,%u\n",
                hsv.h, hsv.s, hsv.v, rgb3.r, rgb3.g, rgb3.b);

    const auto yuv709 = alg::rgb_to_yuv709(rgb);
    const auto rgb709 = alg::yuv_to_rgb709(yuv709);
    std::printf("[yuv709] y=%.1f u=%.1f v=%.1f -> rgb=%u,%u,%u\n",
                yuv709.y, yuv709.u, yuv709.v, rgb709.r, rgb709.g, rgb709.b);

    const auto linear = alg::gamma_decode(rgb);
    const auto gamma = alg::gamma_encode(linear);
    std::printf("[gamma] lin=%u,%u,%u -> enc=%u,%u,%u\n",
                linear.r, linear.g, linear.b, gamma.r, gamma.g, gamma.b);

    const alg::Rgb a{255, 0, 0};
    const alg::Rgb b{0, 0, 255};
    const auto yuy2 = alg::pack_yuy2(a, b);
    alg::Rgb ua{}, ub{};
    alg::unpack_yuy2(yuy2, ua, ub);
    std::printf("[yuy2] %u,%u,%u | %u,%u,%u\n", ua.r, ua.g, ua.b, ub.r, ub.g, ub.b);

    const auto nv12 = alg::pack_nv12(a, b);
    alg::Rgb na{}, nb{};
    alg::unpack_nv12(nv12, na, nb);
    std::printf("[nv12] %u,%u,%u | %u,%u,%u\n", na.r, na.g, na.b, nb.r, nb.g, nb.b);

    const auto ycc = alg::rgb_to_ycbcr2020(rgb, true);
    const auto rgb2020 = alg::ycbcr2020_to_rgb(ycc, true);
    std::printf("[ycbcr2020] y=%.1f cb=%.1f cr=%.1f -> rgb=%u,%u,%u\n",
                ycc.y, ycc.cb, ycc.cr, rgb2020.r, rgb2020.g, rgb2020.b);

    const auto lab = alg::rgb_to_lab(rgb);
    const auto rgb_lab = alg::lab_to_rgb(lab);
    std::printf("[lab] L=%.1f a=%.1f b=%.1f -> rgb=%u,%u,%u\n",
                lab.l, lab.a, lab.b, rgb_lab.r, rgb_lab.g, rgb_lab.b);

    alg::Lut3D<4> lut{};
    lut.set(0, 0, 0, alg::Rgb{0, 0, 0});
    lut.set(3, 3, 3, alg::Rgb{255, 255, 255});
    const auto lut_rgb = lut.sample(rgb.r, rgb.g, rgb.b);
    std::printf("[lut] %u,%u,%u\n", lut_rgb.r, lut_rgb.g, lut_rgb.b);

    std::array<util::u8, 32> msg{};
    const char* text = "aaaaabbbbccccdddd";
    std::size_t tlen = 0;
    while (text[tlen] != '\0' && tlen < msg.size()) {
        msg[tlen] = static_cast<util::u8>(text[tlen]);
        ++tlen;
    }

    std::array<util::u8, 64> buf{};
    std::array<util::u8, 64> dec{};

    auto rle = alg::rle_compress(std::span<const util::u8>(msg.data(), tlen), buf);
    auto rle_dec = alg::rle_decompress(std::span<const util::u8>(buf.data(), rle.used), dec);
    std::printf("[rle] %llu -> %llu\n",
                static_cast<unsigned long long>(tlen),
                static_cast<unsigned long long>(rle_dec.used));

    auto pb = alg::packbits_compress(std::span<const util::u8>(msg.data(), tlen), buf);
    auto pb_dec = alg::packbits_decompress(std::span<const util::u8>(buf.data(), pb.used), dec);
    std::printf("[packbits] %llu -> %llu\n",
                static_cast<unsigned long long>(tlen),
                static_cast<unsigned long long>(pb_dec.used));

    auto hs = alg::heatshrink_compress<5, 4>(std::span<const util::u8>(msg.data(), tlen), buf);
    auto hs_dec = alg::heatshrink_decompress<5, 4>(std::span<const util::u8>(buf.data(), hs.used), dec);
    std::printf("[heatshrink] %llu -> %llu\n",
                static_cast<unsigned long long>(tlen),
                static_cast<unsigned long long>(hs_dec.used));

    auto lz = alg::lz4_compress(std::span<const util::u8>(msg.data(), tlen), buf);
    auto lz_dec = alg::lz4_decompress(std::span<const util::u8>(buf.data(), lz.used), dec);
    std::printf("[lz4] %llu -> %llu\n",
                static_cast<unsigned long long>(tlen),
                static_cast<unsigned long long>(lz_dec.used));
    return 0;
}
