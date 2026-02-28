module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_dma.h"
#include "stm32h7xx_hal_i2s.h"

export module player.stm32h7.audio_mp3_demo;

import audio.decoder.mp3;
import fs_core;
import fs_errno;
import fs_stream;
import fs_vfs;
import media.stream.source;
import media.stream.types;
import out.api;
import util.core;
import util.expected;

extern "C" I2S_HandleTypeDef hi2s1;

namespace {
    constexpr std::uint32_t kTimeoutMs = 1000;
    constexpr std::size_t kI2sBufFrames = 1024;
    constexpr int kGainShift = 0;
    constexpr std::size_t kI2sWordsPerFrame = 4; // 32-bit stereo: 2 words per channel
    constexpr bool kVerbose = false;
    constexpr bool kRuntimeLog = false;
    constexpr bool kStartupLog = true;

    char ascii_lower(char c) noexcept {
        if (c >= 'A' && c <= 'Z') return static_cast<char>(c + ('a' - 'A'));
        return c;
    }

    std::size_t name_len(std::string_view name) noexcept {
        if (!name.data()) return 0;
        if (name.size() > 0) return name.size();
        return std::strlen(name.data());
    }

    bool is_mp3_name(std::string_view name) noexcept {
        const auto len = name_len(name);
        if (len < 4) return false;
        const char c0 = ascii_lower(name.data()[len - 4]);
        const char c1 = ascii_lower(name.data()[len - 3]);
        const char c2 = ascii_lower(name.data()[len - 2]);
        const char c3 = ascii_lower(name.data()[len - 1]);
        return c0 == '.' && c1 == 'm' && c2 == 'p' && c3 == '3';
    }

    struct FindAudioCtx {
        char path[128]{};
        bool found{false};
    };

    fs::Status find_first_mp3(void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
        auto* out = static_cast<FindAudioCtx*>(ctx);
        if (!out) return fs::Status{fs::Err::inval};
        if (out->found || entry.type != fs::NodeType::file) return fs::Status{fs::Err::ok};
        if (!is_mp3_name(entry.name)) return fs::Status{fs::Err::ok};
        const auto len = name_len(entry.name);
        const char prefix[] = "/MUSIC/";
        std::size_t pos = 0;
        for (std::size_t i = 0; i < sizeof(prefix) - 1 && pos + 1 < sizeof(out->path); ++i) {
            out->path[pos++] = prefix[i];
        }
        for (std::size_t i = 0; i < len && pos + 1 < sizeof(out->path); ++i) {
            out->path[pos++] = entry.name.data()[i];
        }
        out->path[pos] = '\0';
        out->found = true;
        return fs::Status{fs::Err::ok};
    }

    struct FileSource {
        fs::File* file{nullptr};
        util::i64 base_offset{0};

        media::Result<util::usize> read(std::span<std::byte> out) noexcept {
            if (!file) return util::unexpected(media::Error{media::Errc::bad_state, 0});
            static int debug_read = 0;
            if (file->node.offset < base_offset) {
                (void)fs::vfs_seek(*file, base_offset);
            }
            std::size_t to_read = out.size();
            if (file->node.size > 0) {
                const auto remaining = file->node.size - file->node.offset;
                if (remaining <= 0) return static_cast<util::usize>(0);
                to_read = static_cast<std::size_t>(
                    std::min<util::i64>(remaining, static_cast<util::i64>(out.size())));
            }
            const auto before = file->node.offset;
            auto st = fs::vfs_read(*file, std::span<util::u8>(
                reinterpret_cast<util::u8*>(out.data()), to_read));
            if (!st) return util::unexpected(media::Error{media::Errc::io_error, 0});
            const auto after = file->node.offset;
            if constexpr (kVerbose) {
                if (debug_read < 4) {
                    out::println<"mp3 demo: read before={} after={} req={}">(before, after, to_read);
                    if (before == base_offset && to_read >= 8) {
                        auto* b = reinterpret_cast<const util::u8*>(out.data());
                        out::println<"mp3 demo: data {} {} {} {} {} {} {} {}">(b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
                    }
                    ++debug_read;
                }
            }
            if (after <= before) return static_cast<util::usize>(0);
            return static_cast<util::usize>(after - before);
        }

        media::Result<util::i64> seek(util::i64 offset, media::SeekWhence whence) noexcept {
            if (!file) return util::unexpected(media::Error{media::Errc::bad_state, 0});
            util::i64 target = offset;
            if (whence == media::SeekWhence::cur) {
                target = (file->node.offset - base_offset) + offset + base_offset;
            } else if (whence == media::SeekWhence::end) {
                target = (file->node.size - base_offset) + offset + base_offset;
            } else {
                target = base_offset + offset;
            }
            if constexpr (kVerbose) {
                static int debug_seek = 0;
                if (debug_seek < 4) {
                    out::println<"mp3 demo: seek whence={} off={} -> {}">(static_cast<int>(whence), offset, target);
                    ++debug_seek;
                }
            }
            auto st = fs::vfs_seek(*file, target);
            if (!st) return util::unexpected(media::Error{media::Errc::io_error, 0});
            return target;
        }

        media::Result<util::i64> tell() noexcept {
            if (!file) return util::unexpected(media::Error{media::Errc::bad_state, 0});
            return file->node.offset - base_offset;
        }

        media::Result<util::i64> size() noexcept {
            if (!file) return util::unexpected(media::Error{media::Errc::bad_state, 0});
            return file->node.size - base_offset;
        }
    };

    struct Mp3Session {
        audio::Mp3Filter filter{};
        FileSource source{};
        bool ended{false};
    };

    bool mp3_fill_block(
        Mp3Session& s,
        std::span<std::int16_t> pcm,
        std::span<std::uint16_t> out_words,
        std::uint8_t channels) noexcept {
        static int debug_blocks = 0;
        static int energy_blocks = 0;
        static std::uint32_t block_count = 0;
        if (s.ended) {
            std::memset(out_words.data(), 0, out_words.size_bytes());
            return false;
        }
        auto dst = std::span<std::byte>(
            reinterpret_cast<std::byte*>(pcm.data()),
            pcm.size() * sizeof(std::int16_t));
        auto res = s.filter.process({}, dst);
        if constexpr (kVerbose) {
            if (debug_blocks < 3) {
            out::println<"mp3 demo: produced={} eos={}">(res ? res->produced : 0, res ? res->end_of_stream : 0);
            ++debug_blocks;
            }
        }
        if (!res) {
            s.ended = true;
            std::memset(out_words.data(), 0, out_words.size_bytes());
            return false;
        }
        if (res->produced < dst.size()) {
            std::memset(dst.data() + res->produced, 0, dst.size() - res->produced);
        }
        if (res->produced == 0 && res->end_of_stream) {
            s.ended = true;
            return false;
        }
        const auto samples = dst.size() / sizeof(std::int16_t);
        std::uint32_t nonzero = 0;
        std::uint64_t abs_sum = 0;
        std::int16_t pre_min = 32767;
        std::int16_t pre_max = -32768;
        std::int16_t post_min = 32767;
        std::int16_t post_max = -32768;
        for (std::size_t i = 0; i < samples; ++i) {
            auto v = static_cast<std::int32_t>(pcm[i]);
            if (v != 0) ++nonzero;
            abs_sum += static_cast<std::uint64_t>(v < 0 ? -v : v);
            if (v < pre_min) pre_min = static_cast<std::int16_t>(v);
            if (v > pre_max) pre_max = static_cast<std::int16_t>(v);
            v <<= kGainShift;
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            if (v < post_min) post_min = static_cast<std::int16_t>(v);
            if (v > post_max) post_max = static_cast<std::int16_t>(v);
            pcm[i] = static_cast<std::int16_t>(v);
        }
        const auto frames = static_cast<std::size_t>(samples / channels);
        for (std::size_t i = 0; i < frames; ++i) {
            const std::int16_t l = pcm[i * channels];
            const std::int16_t r = (channels > 1) ? pcm[i * channels + 1] : l;
            const std::size_t o = i * kI2sWordsPerFrame;
            out_words[o + 0] = static_cast<std::uint16_t>(l);
            out_words[o + 1] = 0;
            out_words[o + 2] = static_cast<std::uint16_t>(r);
            out_words[o + 3] = 0;
        }
        if constexpr (kVerbose) {
            if (energy_blocks < 3) {
            out::println<"mp3 demo: energy nonzero={} sum={}">(nonzero, abs_sum);
            ++energy_blocks;
            }
        }
        if constexpr (kVerbose) {
            if ((block_count % 100) == 0) {
            out::println<"mp3 demo: block#{} pre=[{},{}] post=[{},{}]">(
                block_count, pre_min, pre_max, post_min, post_max);
            }
        }
        if constexpr (kVerbose) {
            if (debug_blocks <= 2) {
            std::int16_t min_v = 32767;
            std::int16_t max_v = -32768;
            for (std::size_t i = 0; i < samples; ++i) {
                const auto v = pcm[i];
                if (v < min_v) min_v = v;
                if (v > max_v) max_v = v;
            }
            out::println<"mp3 demo: block min={} max={} s0={} s1={} s2={} s3={} s4={} s5={}">(min_v, max_v,
                pcm[0], pcm[1], pcm[2], pcm[3], pcm[4], pcm[5]);
            }
        }
        ++block_count;
        return true;
    }
} // namespace

static volatile bool g_half_ready = false;
static volatile bool g_full_ready = false;
static volatile std::uint32_t g_underruns = 0;
static std::array<std::int16_t, kI2sBufFrames * 2 * 2> g_pcm_buffer{};
static std::array<std::uint16_t, kI2sBufFrames * 2 * kI2sWordsPerFrame> g_i2s_buffer{};

extern "C" void charm_audio_i2s_half_notify() {
    g_half_ready = true;
}

extern "C" void charm_audio_i2s_full_notify() {
    g_full_ready = true;
}

export void audio_mp3_demo_run() noexcept {
    HAL_I2S_DMAStop(&hi2s1);
    FindAudioCtx ctx{};
    auto st = fs::vfs_list("/MUSIC", &ctx, &find_first_mp3);
    if (!st || !ctx.found) {
        out::println<"mp3 demo: no mp3 found">();
        return;
    }

    if constexpr (kStartupLog) {
        out::println<"mp3 demo: open {}">(ctx.path);
    }
    fs::File f{};
    st = fs::vfs_open(ctx.path, f);
    if (!st) {
        out::println<"mp3 demo: open failed {}">(static_cast<int>(st.err));
        return;
    }
    {
        std::array<util::u8, 16> head{};
        (void)fs::vfs_seek(f, 0);
        auto st_read = fs::vfs_read(f, head);
        (void)st_read;
        if constexpr (kStartupLog) {
            out::println<"mp3 demo: head {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}">(
                head[0], head[1], head[2], head[3], head[4], head[5], head[6], head[7],
                head[8], head[9], head[10], head[11], head[12], head[13], head[14], head[15]);
        }
        (void)fs::vfs_seek(f, 0);
    }
    (void)fs::vfs_seek(f, 0);

    Mp3Session session{};
    session.source = FileSource{&f};
    {
        std::array<util::u8, 10> id3{};
        (void)fs::vfs_seek(f, 0);
        (void)fs::vfs_read(f, id3);
        util::i64 base = 0;
        if (id3[0] == 'I' && id3[1] == 'D' && id3[2] == '3') {
            const std::uint32_t sz =
                (static_cast<std::uint32_t>(id3[6] & 0x7f) << 21) |
                (static_cast<std::uint32_t>(id3[7] & 0x7f) << 14) |
                (static_cast<std::uint32_t>(id3[8] & 0x7f) << 7) |
                (static_cast<std::uint32_t>(id3[9] & 0x7f));
            base = static_cast<util::i64>(10 + sz);
        }
        session.source.base_offset = base;
        if constexpr (kStartupLog) {
            out::println<"mp3 demo: base={}">(base);
        }
        if constexpr (kVerbose) {
            std::array<util::u8, 512> peek{};
            (void)fs::vfs_seek(f, base);
            const auto before = f.node.offset;
            (void)fs::vfs_read(f, peek);
            const auto after = f.node.offset;
            const auto read_bytes = static_cast<util::usize>(after > before ? (after - before) : 0);
            std::size_t nonzero = 0;
            std::size_t zeros = 0;
            std::size_t ff = 0;
            for (auto b : peek) {
                if (b == 0) ++zeros;
                if (b == 0xFF) ++ff;
                if (b != 0) ++nonzero;
            }
            out::println<"mp3 demo: data nonzero {} zeros {} ff {} read {}">(
                nonzero, zeros, ff, read_bytes);
            out::println<"mp3 demo: data16 {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}">(
                peek[0], peek[1], peek[2], peek[3], peek[4], peek[5], peek[6], peek[7],
                peek[8], peek[9], peek[10], peek[11], peek[12], peek[13], peek[14], peek[15]);
        }
        (void)fs::vfs_seek(f, base);
    }
    if constexpr (kStartupLog) {
        out::println<"mp3 demo: filter open begin">();
    }
    auto ref = media::make_stream_source_ref(session.source);
    auto rst = session.filter.open(ref);
    if constexpr (kStartupLog) {
        out::println<"mp3 demo: filter open end">();
    }
    if (!rst) {
        out::println<"mp3 demo: decoder open failed">();
        (void)fs::vfs_close(f);
        return;
    }
    const auto fmt = session.filter.format();
    if (fmt.channels != 1 && fmt.channels != 2) {
        out::println<"mp3 demo: channels {} not supported">(fmt.channels);
        session.filter.close();
        (void)fs::vfs_close(f);
        return;
    }
    if constexpr (kStartupLog) {
        out::println<"mp3 demo: rate={} ch={}">(fmt.rate, fmt.channels);
        out::println<"mp3 demo: size={}">(f.node.size);
    }

    const std::size_t period_words = kI2sBufFrames * kI2sWordsPerFrame;
    if (period_words == 0) {
        out::println<"mp3 demo: invalid period">();
        session.filter.close();
        (void)fs::vfs_close(f);
        return;
    }

    auto pcm_first = std::span<std::int16_t>(g_pcm_buffer.data(), kI2sBufFrames * 2);
    auto pcm_second = std::span<std::int16_t>(g_pcm_buffer.data() + kI2sBufFrames * 2, kI2sBufFrames * 2);
    auto out_first = std::span<std::uint16_t>(g_i2s_buffer.data(), period_words);
    auto out_second = std::span<std::uint16_t>(g_i2s_buffer.data() + period_words, period_words);
    mp3_fill_block(session, pcm_first, out_first, fmt.channels);
    mp3_fill_block(session, pcm_second, out_second, fmt.channels);
    {
        auto* pcm = g_pcm_buffer.data();
        const std::size_t samples = kI2sBufFrames * 2;
        std::int16_t min_v = 32767;
        std::int16_t max_v = -32768;
        for (std::size_t i = 0; i < samples; ++i) {
            const auto v = pcm[i];
            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
        }
        if constexpr (kStartupLog) {
            out::println<"mp3 demo: pcm min={} max={} s0={} s1={} s2={} s3={}">(min_v, max_v, pcm[0], pcm[1], pcm[2], pcm[3]);
        }
    }

    if (HAL_I2S_Transmit_DMA(&hi2s1,
            g_i2s_buffer.data(),
            static_cast<uint16_t>(g_i2s_buffer.size())) != HAL_OK) {
        out::println<"mp3 demo: dma start failed">();
        out::println<"mp3 demo: i2s state={} err={}">(static_cast<int>(hi2s1.State),
            static_cast<int>(HAL_I2S_GetError(&hi2s1)));
        session.filter.close();
        (void)fs::vfs_close(f);
        return;
    }
    if constexpr (kStartupLog) {
        out::println<"mp3 demo: i2s state={} err={}">(static_cast<int>(hi2s1.State),
            static_cast<int>(HAL_I2S_GetError(&hi2s1)));
    }

    std::size_t idle_ticks = 0;
    std::size_t tick = 0;
    bool half_filled = true;
    bool full_filled = true;
    while (true) {
        if (g_half_ready) {
            g_half_ready = false;
            if (!mp3_fill_block(session, pcm_first, out_first, fmt.channels)) {
                g_underruns = g_underruns + 1;
                half_filled = false;
            } else {
                half_filled = true;
            }
        }
        if (g_full_ready) {
            g_full_ready = false;
            if (!mp3_fill_block(session, pcm_second, out_second, fmt.channels)) {
                g_underruns = g_underruns + 1;
                full_filled = false;
            } else {
                full_filled = true;
            }
        }
        if (session.ended) {
            if (++idle_ticks > 50) break;
        }
        if constexpr (kRuntimeLog) {
            if ((++tick % 50000) == 0) {
                const auto ndtr = -1;
                const auto dstate = -1;
                out::println<"mp3 demo: playing... ndtr={} dma_state={} i2s_state={} underrun={} half_ok={} full_ok={}">(
                    ndtr, dstate, static_cast<int>(hi2s1.State), g_underruns,
                    static_cast<int>(half_filled), static_cast<int>(full_filled));
            }
        }
    }
    out::println<"mp3 demo: end">();
    HAL_I2S_DMAStop(&hi2s1);
    session.filter.close();
    (void)fs::vfs_close(f);
}
