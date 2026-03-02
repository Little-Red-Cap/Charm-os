module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "i2s.h"
#include "dma.h"

export module player.stm32.audio_mp3_demo;

import audio.decoder.mp3;
import fs_core;
import fs_errno;
import fs_stream;
import fs_vfs;
import media.stream.filter;
import media.stream.source;
import media.stream.types;
import out.api;
import util.core;
import util.expected;

namespace {
    constexpr std::uint32_t kTimeoutMs = 1000;
    constexpr std::size_t kI2sBufFrames = 2048;
    constexpr int kGainShift = 8;
    constexpr std::size_t kI2sWordsPerFrame = 2; // 16-bit stereo: 1 word per channel
    constexpr std::uint32_t kI2sStandard = I2S_STANDARD_PHILIPS;
    constexpr std::uint32_t kI2sDataFormat = I2S_DATAFORMAT_16B_EXTENDED;
    constexpr bool kVerbose = false;
    constexpr bool kRuntimeLog = false;
    constexpr bool kStartupLog = true;
    constexpr bool kMp3FileDiag = false;
    constexpr bool kMp3DecodeDiag = false;
    constexpr bool kUseBlockingI2s = true;
    constexpr std::uint32_t kFilterOpenLogStep = 128;
    constexpr std::uint32_t kFilterOpenSeekLogStep = 64;
    constexpr bool kUseDrmp3Id3Skip = true;
    constexpr std::size_t kMaxStreamRead = 4096;
    constexpr std::size_t kDiagReadChunk = 4096;
    constexpr std::size_t kDiagDecodeFrames = 1024;
    constexpr std::size_t kDiagDecodeBlocks = 3;

    std::uint32_t crc32_update(std::uint32_t crc, std::span<const util::u8> data) noexcept {
        for (const auto b : data) {
            crc ^= static_cast<std::uint32_t>(b);
            for (int i = 0; i < 8; ++i) {
                const std::uint32_t mask = (crc & 1u) ? 0xEDB88320u : 0u;
                crc = (crc >> 1) ^ mask;
            }
        }
        return crc;
    }

    static bool g_in_filter_open = false;
    static std::uint32_t g_filter_open_reads = 0;
    static std::uint32_t g_filter_open_seeks = 0;
    static std::uint32_t g_filter_open_zero_reads = 0;
    static std::uint32_t g_filter_open_tells = 0;
    static std::uint32_t g_filter_open_sizes = 0;
    static std::uint32_t g_filter_open_read_calls = 0;

    std::uint32_t map_i2s_freq(std::uint32_t rate) noexcept {
        switch (rate) {
            case 8000: return I2S_AUDIOFREQ_8K;
            case 11025: return I2S_AUDIOFREQ_11K;
            case 16000: return I2S_AUDIOFREQ_16K;
            case 22050: return I2S_AUDIOFREQ_22K;
            case 32000: return I2S_AUDIOFREQ_32K;
            case 44100: return I2S_AUDIOFREQ_44K;
            case 48000: return I2S_AUDIOFREQ_48K;
            case 96000: return I2S_AUDIOFREQ_96K;
            default: return 0;
        }
    }

    bool reinit_i2s(std::uint32_t rate) noexcept {
        const auto freq = map_i2s_freq(rate);
        if (freq == 0) return false;
        hi2s2.Init.AudioFreq = freq;
        hi2s2.Init.Standard = kI2sStandard;
        hi2s2.Init.DataFormat = kI2sDataFormat;
        HAL_I2S_DeInit(&hi2s2);
        return HAL_I2S_Init(&hi2s2) == HAL_OK;
    }

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

    bool is_wav_name(std::string_view name) noexcept {
        const auto len = name_len(name);
        if (len < 4) return false;
        const char c0 = ascii_lower(name.data()[len - 4]);
        const char c1 = ascii_lower(name.data()[len - 3]);
        const char c2 = ascii_lower(name.data()[len - 2]);
        const char c3 = ascii_lower(name.data()[len - 1]);
        return c0 == '.' && c1 == 'w' && c2 == 'a' && c3 == 'v';
    }

    struct FindAudioCtx {
        const char* prefix{nullptr};
        std::size_t prefix_len{0};
        char path[128]{};
        bool found{false};
    };

    fs::Status find_first_mp3(void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
        auto* out = static_cast<FindAudioCtx*>(ctx);
        if (!out) return fs::Status{fs::Err::inval};
        if (out->found || entry.type != fs::NodeType::file) return fs::Status{fs::Err::ok};
        if (!is_mp3_name(entry.name)) return fs::Status{fs::Err::ok};
        const auto len = name_len(entry.name);
        std::size_t pos = 0;
        if (out->prefix && out->prefix_len > 0) {
            for (std::size_t i = 0; i < out->prefix_len && pos + 1 < sizeof(out->path); ++i) {
                out->path[pos++] = out->prefix[i];
            }
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
            if (to_read > kMaxStreamRead) {
                to_read = kMaxStreamRead;
            }
            const auto before = file->node.offset;
            if (g_in_filter_open) {
                const auto count = ++g_filter_open_read_calls;
                if (count <= 8 || (count % kFilterOpenLogStep) == 0) {
                    out::println<"mp3 demo: filter read call#{} off {} req {}">(
                        count, before, to_read);
                }
            }
            auto st = fs::vfs_read(*file, std::span<util::u8>(
                reinterpret_cast<util::u8*>(out.data()), to_read));
            if (!st) {
                out::println<"mp3 demo: read failed err={}">(
                    static_cast<int>(st.err));
                return util::unexpected(media::Error{media::Errc::io_error, 0});
            }
            const auto after = file->node.offset;
            const auto total_read = static_cast<std::size_t>(
                after > before ? (after - before) : 0);
            if (g_in_filter_open) {
                const auto count = ++g_filter_open_reads;
                if (count <= 8 || (count % kFilterOpenLogStep) == 0) {
                    out::println<"mp3 demo: filter read#{} off {} -> {} req {}">(
                        count, before, after, to_read);
                }
            }
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
            if (total_read == 0 || after <= before) {
                if (g_in_filter_open) {
                    const auto count = ++g_filter_open_zero_reads;
                    if (count <= 8 || (count % kFilterOpenLogStep) == 0) {
                        out::println<"mp3 demo: filter read zero#{} off {} -> {} req {}">(
                            count, before, after, to_read);
                    }
                }
                return static_cast<util::usize>(0);
            }
            return static_cast<util::usize>(total_read);
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
            if (g_in_filter_open) {
                const auto count = ++g_filter_open_seeks;
                if (count <= 8 || (count % kFilterOpenSeekLogStep) == 0) {
                    out::println<"mp3 demo: filter seek#{} whence={} off={} -> {}">(count,
                        static_cast<int>(whence), offset, target);
                }
            }
            auto st = fs::vfs_seek(*file, target);
            if (!st) return util::unexpected(media::Error{media::Errc::io_error, 0});
            return target;
        }

        media::Result<util::i64> tell() noexcept {
            if (!file) return util::unexpected(media::Error{media::Errc::bad_state, 0});
            if (g_in_filter_open) {
                const auto count = ++g_filter_open_tells;
                if (count <= 8 || (count % kFilterOpenSeekLogStep) == 0) {
                    out::println<"mp3 demo: filter tell#{} -> {}">(
                        count, static_cast<long long>(file->node.offset - base_offset));
                }
            }
            return file->node.offset - base_offset;
        }

        media::Result<util::i64> size() noexcept {
            if (!file) return util::unexpected(media::Error{media::Errc::bad_state, 0});
            if (g_in_filter_open) {
                const auto count = ++g_filter_open_sizes;
                if (count <= 8 || (count % kFilterOpenSeekLogStep) == 0) {
                    out::println<"mp3 demo: filter size#{} -> {}">(
                        count, static_cast<long long>(file->node.size - base_offset));
                }
            }
            return file->node.size - base_offset;
        }
    };

    struct Mp3Session {
        audio::Mp3Filter filter{};
        FileSource source{};
        bool ended{false};
    };

    struct WavInfo {
        std::uint16_t channels{0};
        std::uint32_t sample_rate{0};
        std::uint16_t bits_per_sample{0};
        std::uint32_t data_offset{0};
        std::uint32_t data_size{0};
    };

    std::uint16_t read_u16_le(const std::uint8_t* data) noexcept {
        return static_cast<std::uint16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8));
    }

    std::uint32_t read_u32_le(const std::uint8_t* data) noexcept {
        return static_cast<std::uint32_t>(data[0])
            | (static_cast<std::uint32_t>(data[1]) << 8)
            | (static_cast<std::uint32_t>(data[2]) << 16)
            | (static_cast<std::uint32_t>(data[3]) << 24);
    }

    bool read_wav_header(fs::File& f, WavInfo& info) noexcept {
        std::array<std::uint8_t, 12> riff{};
        auto st = fs::vfs_read(f, std::span<std::uint8_t>{riff});
        if (!st) return false;
        if (std::memcmp(riff.data(), "RIFF", 4) != 0 || std::memcmp(riff.data() + 8, "WAVE", 4) != 0) {
            return false;
        }
        std::uint32_t cursor = 12;
        bool fmt_ok = false;
        bool data_ok = false;
        while (!fmt_ok || !data_ok) {
            if (cursor + 8 > f.node.size) return false;
            std::array<std::uint8_t, 8> chunk{};
            st = fs::vfs_read(f, std::span<std::uint8_t>{chunk});
            if (!st) return false;
            cursor += 8;
            const std::uint32_t size = read_u32_le(chunk.data() + 4);
            std::uint32_t next = cursor + size;
            if (size & 1u) {
                ++next;
            }
            if (std::memcmp(chunk.data(), "fmt ", 4) == 0) {
                if (size < 16) return false;
                std::array<std::uint8_t, 16> fmt{};
                st = fs::vfs_read(f, std::span<std::uint8_t>{fmt});
                if (!st) return false;
                cursor += 16;
                const std::uint16_t format = read_u16_le(fmt.data());
                info.channels = read_u16_le(fmt.data() + 2);
                info.sample_rate = read_u32_le(fmt.data() + 4);
                info.bits_per_sample = read_u16_le(fmt.data() + 14);
                if (format != 1 && format != 0xFFFEu) return false;
                fmt_ok = true;
            } else if (std::memcmp(chunk.data(), "data", 4) == 0) {
                info.data_offset = cursor;
                info.data_size = size;
                data_ok = true;
            } else {
                // skip unknown chunk
            }
            if (!fmt_ok || !data_ok) {
                cursor = next;
                st = fs::vfs_seek(f, static_cast<std::int64_t>(cursor));
                if (!st) return false;
            }
        }
        return true;
    }

    bool play_wav_blocking(fs::File& f, const WavInfo& info) noexcept {
        if ((info.bits_per_sample != 16 && info.bits_per_sample != 24) ||
            (info.channels != 1 && info.channels != 2)) {
            out::println<"wav: only 16/24-bit mono/stereo supported">();
            return false;
        }
        if (!reinit_i2s(info.sample_rate)) {
            out::println<"wav: unsupported sample rate {}">(info.sample_rate);
            return false;
        }
        if (!fs::vfs_seek(f, static_cast<std::int64_t>(info.data_offset))) return false;

        std::array<std::uint8_t, 4096> read_buf{};
        std::array<std::uint16_t, 2048> i2s_buf{};
        std::uint32_t remaining = info.data_size;
        const std::size_t sample_bytes = static_cast<std::size_t>(info.bits_per_sample / 8);
        const std::size_t frame_bytes = static_cast<std::size_t>(info.channels) * sample_bytes;

        while (remaining > 0) {
            const std::size_t want = std::min<std::size_t>(read_buf.size(), remaining);
            const auto before = f.node.offset;
            auto st = fs::vfs_read(f, std::span<std::uint8_t>{read_buf.data(), want});
            if (!st) return false;
            const auto after = f.node.offset;
            if (after < before) return false;
            const std::size_t got = static_cast<std::size_t>(after - before);
            if (got == 0) break;
            remaining -= static_cast<std::uint32_t>(got);
            const std::size_t usable = got - (got % frame_bytes);
            if (usable == 0) continue;

            if (info.bits_per_sample == 16) {
                const std::uint16_t* samples = reinterpret_cast<const std::uint16_t*>(read_buf.data());
                const std::size_t sample_count = usable / sizeof(std::uint16_t);
                if (info.channels == 2) {
                    std::size_t offset = 0;
                    while (offset < sample_count) {
                        const std::size_t chunk = std::min<std::size_t>(i2s_buf.size(), sample_count - offset);
                        std::memcpy(i2s_buf.data(), samples + offset, chunk * sizeof(std::uint16_t));
                        if (HAL_I2S_Transmit(&hi2s2, i2s_buf.data(),
                                static_cast<uint16_t>(chunk), kTimeoutMs) != HAL_OK) {
                            return false;
                        }
                        offset += chunk;
                    }
                } else {
                    std::size_t offset = 0;
                    while (offset < sample_count) {
                        const std::size_t frames = std::min<std::size_t>(i2s_buf.size() / 2, sample_count - offset);
                        for (std::size_t i = 0; i < frames; ++i) {
                            const std::uint16_t s = samples[offset + i];
                            i2s_buf[i * 2] = s;
                            i2s_buf[i * 2 + 1] = s;
                        }
                        if (HAL_I2S_Transmit(&hi2s2, i2s_buf.data(),
                                static_cast<uint16_t>(frames * 2), kTimeoutMs) != HAL_OK) {
                            return false;
                        }
                        offset += frames;
                    }
                }
            } else {
                auto decode24 = [](const std::uint8_t* p) -> std::int16_t {
                    std::int32_t v = static_cast<std::int32_t>(
                        static_cast<std::uint32_t>(p[0]) |
                        (static_cast<std::uint32_t>(p[1]) << 8) |
                        (static_cast<std::uint32_t>(p[2]) << 16));
                    if (v & 0x800000) v |= static_cast<std::int32_t>(0xFF000000u);
                    return static_cast<std::int16_t>(v >> 8);
                };
                const std::uint8_t* bytes = read_buf.data();
                const std::size_t frames = usable / frame_bytes;
                std::size_t offset = 0;
                while (offset < frames) {
                    const std::size_t chunk = std::min<std::size_t>(i2s_buf.size() / 2, frames - offset);
                    for (std::size_t i = 0; i < chunk; ++i) {
                        const std::uint8_t* frame = bytes + (offset + i) * frame_bytes;
                        const std::int16_t l = decode24(frame);
                        const std::int16_t r = (info.channels == 2) ? decode24(frame + 3) : l;
                        i2s_buf[i * 2] = static_cast<std::uint16_t>(l);
                        i2s_buf[i * 2 + 1] = static_cast<std::uint16_t>(r);
                    }
                    if (HAL_I2S_Transmit(&hi2s2, i2s_buf.data(),
                            static_cast<uint16_t>(chunk * 2), kTimeoutMs) != HAL_OK) {
                        return false;
                    }
                    offset += chunk;
                }
            }
        }
        return true;
    }

    void mp3_diag_file(fs::File& f, util::i64 base) noexcept {
        if constexpr (!kMp3FileDiag) return;
        out::println<"mp3 diag: file read begin size={}">(f.node.size);
        std::array<util::u8, kDiagReadChunk> buf{};
        util::u64 total = 0;
        std::uint32_t crc = 0xFFFFFFFFu;
        util::i64 remaining = static_cast<util::i64>(f.node.size);
        if (!fs::vfs_seek(f, 0)) {
            out::println<"mp3 diag: seek 0 failed">();
            return;
        }
        while (true) {
            const auto before = f.node.offset;
            if (remaining <= 0) break;
            const auto want = static_cast<std::size_t>(std::min<util::i64>(
                remaining, static_cast<util::i64>(buf.size())));
            auto st = fs::vfs_read(f, std::span<util::u8>(buf.data(), want));
            if (!st) {
                out::println<"mp3 diag: read failed {}">(static_cast<int>(st.err));
                break;
            }
            const auto after = f.node.offset;
            if (after <= before) break;
            const auto read_bytes = static_cast<std::size_t>(after - before);
            total += read_bytes;
            remaining -= static_cast<util::i64>(read_bytes);
            crc = crc32_update(crc, std::span<const util::u8>(buf.data(), read_bytes));
        }
        crc ^= 0xFFFFFFFFu;
        out::println<"mp3 diag: file read total={} size={} crc=0x{}">(
            total, f.node.size, crc);
        const util::i64 probes[] = {0, 2048, 1048576, static_cast<util::i64>(f.node.size) - 16};
        for (const auto off : probes) {
            if (off < 0) continue;
            if (!fs::vfs_seek(f, off)) {
                out::println<"mp3 diag: seek {} failed">(static_cast<long long>(off));
                continue;
            }
            std::array<util::u8, 16> head{};
            (void)fs::vfs_read(f, head);
            out::println<"mp3 diag: head@{} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}">(
                static_cast<long long>(off),
                head[0], head[1], head[2], head[3], head[4], head[5], head[6], head[7],
                head[8], head[9], head[10], head[11], head[12], head[13], head[14], head[15]);
        }
        (void)fs::vfs_seek(f, base);
    }

    void mp3_diag_decode(Mp3Session& s, std::uint8_t channels) noexcept {
        if constexpr (!kMp3DecodeDiag) return;
        out::println<"mp3 diag: total_frames={}">(
            static_cast<long long>(s.filter.total_frames()));
        std::array<std::int16_t, kDiagDecodeFrames * 2> pcm{};
        for (std::size_t b = 0; b < kDiagDecodeBlocks; ++b) {
            auto dst = std::span<std::byte>(
                reinterpret_cast<std::byte*>(pcm.data()),
                pcm.size() * sizeof(std::int16_t));
            auto res = s.filter.process({}, dst);
            if (!res) {
                out::println<"mp3 diag: decode err">();
                return;
            }
            if (res->produced == 0 && res->end_of_stream) {
                out::println<"mp3 diag: decode eos">();
                return;
            }
            const auto samples = static_cast<std::size_t>(res->produced / sizeof(std::int16_t));
            const auto frames = (channels == 0) ? 0 : (samples / channels);
            std::int16_t min_v = 32767;
            std::int16_t max_v = -32768;
            std::uint32_t nonzero = 0;
            for (std::size_t i = 0; i < samples; ++i) {
                const auto v = pcm[i];
                if (v < min_v) min_v = v;
                if (v > max_v) max_v = v;
                if (v != 0) ++nonzero;
            }
            out::println<"mp3 diag: dec#{} prod={} frames={} min={} max={} nz={} eos={}">(
                b, res->produced, frames, min_v, max_v, nonzero, res->end_of_stream ? 1 : 0);
        }
        auto rst = s.filter.reset();
        if (!rst) {
            out::println<"mp3 diag: reset failed">();
        }
    }

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
        std::size_t written = 0;
        media::FilterResult last{};
        while (written < dst.size()) {
            auto out = dst.subspan(written);
            auto res = s.filter.process({}, out);
            if (!res) {
                s.ended = true;
                std::memset(out_words.data(), 0, out_words.size_bytes());
                return false;
            }
            last = *res;
            if (last.produced == 0) {
                if (last.end_of_stream) {
                    s.ended = true;
                }
                break;
            }
            written += static_cast<std::size_t>(last.produced);
            if (last.end_of_stream) {
                s.ended = true;
                break;
            }
        }
        if constexpr (kVerbose) {
            if (debug_blocks < 3) {
                out::println<"mp3 demo: produced={} eos={}">(last.produced, last.end_of_stream ? 1 : 0);
                ++debug_blocks;
            }
        }
        if (written < dst.size()) {
            std::memset(dst.data() + written, 0, dst.size() - written);
        }
        if (written == 0 && s.ended) {
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
            out_words[o + 1] = static_cast<std::uint16_t>(r);
        }
        if constexpr (kVerbose) {
            if (energy_blocks < 3) {
            out::println<"mp3 demo: energy nonzero={} sum={}">(nonzero, abs_sum);
            ++energy_blocks;
            }
        }
        {
            static bool printed = false;
            if (!printed) {
                out::println<"mp3 demo: block0 pre=[{},{}] post=[{},{}] nonzero={}">(
                    pre_min, pre_max, post_min, post_max, nonzero);
                printed = true;
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
    constexpr std::size_t kTestFrames = 512;
    static std::array<std::uint16_t, kTestFrames * 2 * kI2sWordsPerFrame> g_i2s_test_buffer{};

extern "C" void charm_audio_i2s_half_notify() {
    g_half_ready = true;
}

extern "C" void charm_audio_i2s_full_notify() {
    g_full_ready = true;
}

export void audio_mp3_demo_run() noexcept {
    HAL_I2S_DMAStop(&hi2s2);
    FindAudioCtx ctx{};
    const char fixed_path[] = "/jtwayne-pianos-by-jtwayne-7-174717.wav";
    std::size_t pos = 0;
    for (std::size_t i = 0; i + 1 < sizeof(fixed_path) && pos + 1 < sizeof(ctx.path); ++i) {
        ctx.path[pos++] = fixed_path[i];
    }
    ctx.path[pos] = '\0';
    ctx.found = true;

    if constexpr (kStartupLog) {
        out::println<"mp3 demo: open {}">(ctx.path);
    }
    fs::File f{};
    auto st = fs::vfs_open(ctx.path, f);
    if (!st) {
        out::println<"mp3 demo: open failed {}">(static_cast<int>(st.err));
        return;
    }
    if (is_wav_name(ctx.path)) {
        WavInfo info{};
        if (!read_wav_header(f, info)) {
            std::array<std::uint8_t, 12> head{};
            (void)fs::vfs_seek(f, 0);
            (void)fs::vfs_read(f, head);
            out::println<"wav: head {} {} {} {} {} {} {} {} {} {} {} {}">(
                head[0], head[1], head[2], head[3], head[4], head[5],
                head[6], head[7], head[8], head[9], head[10], head[11]);
            out::println<"wav: invalid header">();
            (void)fs::vfs_close(f);
            return;
        }
        out::println<"wav: rate={} ch={} bits={} data={}">(
            info.sample_rate, info.channels, info.bits_per_sample, info.data_size);
        const auto ok = play_wav_blocking(f, info);
        out::println<"wav: end {}">(ok ? 1 : 0);
        (void)fs::vfs_close(f);
        return;
    }
    Mp3Session session{};
    session.source = FileSource{&f};
    const util::i64 base = 0;
    session.source.base_offset = base;
    (void)fs::vfs_seek(f, 0);
    if constexpr (kStartupLog) {
        out::println<"mp3 demo: filter open begin">();
    }
    if constexpr (kMp3FileDiag) {
        mp3_diag_file(f, base);
    }
    auto ref = media::make_stream_source_ref(session.source);
    g_in_filter_open = true;
    g_filter_open_reads = 0;
    g_filter_open_seeks = 0;
    g_filter_open_zero_reads = 0;
    g_filter_open_tells = 0;
    g_filter_open_sizes = 0;
    g_filter_open_read_calls = 0;
    auto rst = session.filter.open(ref);
    g_in_filter_open = false;
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
    if constexpr (kMp3DecodeDiag) {
        mp3_diag_decode(session, fmt.channels);
        session.filter.close();
        auto re = session.filter.open(ref);
        if (!re) {
            out::println<"mp3 demo: decoder reopen failed">();
            (void)fs::vfs_close(f);
            return;
        }
    }
    if (!reinit_i2s(fmt.rate)) {
        out::println<"mp3 demo: unsupported sample rate {}">(fmt.rate);
        session.filter.close();
        (void)fs::vfs_close(f);
        return;
    }

    auto pcm_first = std::span<std::int16_t>(g_pcm_buffer.data(), kI2sBufFrames * 2);
    auto pcm_second = std::span<std::int16_t>(g_pcm_buffer.data() + kI2sBufFrames * 2, kI2sBufFrames * 2);
    auto out_first = std::span<std::uint16_t>(g_i2s_buffer.data(), kI2sBufFrames * kI2sWordsPerFrame);
    auto out_second = std::span<std::uint16_t>(g_i2s_buffer.data() + (kI2sBufFrames * kI2sWordsPerFrame),
        kI2sBufFrames * kI2sWordsPerFrame);

    if constexpr (kUseBlockingI2s) {
        bool first_block = true;
        std::uint32_t block_count = 0;
        while (true) {
            if (!mp3_fill_block(session, pcm_first, out_first, fmt.channels)) {
                out::println<"mp3 demo: fill1 failed ended={}">(
                    session.ended ? 1 : 0);
                break;
            }
            const auto samples = static_cast<uint16_t>(out_first.size());
            if (first_block) {
                std::uint16_t min_w = 0xFFFF;
                std::uint16_t max_w = 0;
                std::uint32_t nz = 0;
                for (const auto w : out_first) {
                    if (w < min_w) min_w = w;
                    if (w > max_w) max_w = w;
                    if (w != 0) ++nz;
                }
                out::println<"mp3 demo: out0 min={} max={} nz={}">(
                    min_w, max_w, nz);
            }
            const auto t0 = HAL_GetTick();
            const auto tx = HAL_I2S_Transmit(&hi2s2, out_first.data(), samples, kTimeoutMs);
            const auto t1 = HAL_GetTick();
            if (first_block) {
                out::println<"mp3 demo: tx0 ms={} ret={} err={}">(
                    static_cast<int>(t1 - t0), static_cast<int>(tx),
                    static_cast<int>(HAL_I2S_GetError(&hi2s2)));
            }
            if (tx != HAL_OK) {
                out::println<"mp3 demo: i2s tx failed {}">(static_cast<int>(HAL_I2S_GetError(&hi2s2)));
                break;
            }
            if (!mp3_fill_block(session, pcm_second, out_second, fmt.channels)) {
                out::println<"mp3 demo: fill2 failed ended={}">(
                    session.ended ? 1 : 0);
                break;
            }
            const auto samples2 = static_cast<uint16_t>(out_second.size());
            if (HAL_I2S_Transmit(&hi2s2, out_second.data(), samples2, kTimeoutMs) != HAL_OK) {
                out::println<"mp3 demo: i2s tx failed {}">(static_cast<int>(HAL_I2S_GetError(&hi2s2)));
                break;
            }
            block_count += 2;
            if (session.ended) break;
            first_block = false;
        }
        out::println<"mp3 demo: end blocks={} ended={}">(
            block_count, session.ended ? 1 : 0);
        session.filter.close();
        (void)fs::vfs_close(f);
        return;
    }
    session.filter.close();
    (void)fs::vfs_close(f);
}

export void i2s_dma_selftest() noexcept {
    HAL_I2S_DMAStop(&hi2s2);
    g_half_ready = false;
    g_full_ready = false;
    g_underruns = 0;

    const auto ok = reinit_i2s(44100);
    if (!ok) {
        out::println<"i2s test: reinit failed">();
        return;
    }

    std::uint16_t sample = 0x2000;
    for (std::size_t i = 0; i < g_i2s_test_buffer.size(); i += 2) {
        sample = (sample == 0x2000) ? 0xE000 : 0x2000;
        g_i2s_test_buffer[i] = sample;
        g_i2s_test_buffer[i + 1] = sample;
    }

    const auto dma_status = HAL_I2S_Transmit_DMA(&hi2s2,
        g_i2s_test_buffer.data(),
        static_cast<uint16_t>(g_i2s_test_buffer.size()));
    if (dma_status != HAL_OK) {
        out::println<"i2s test: dma start failed {}">(static_cast<int>(dma_status));
        return;
    }

    std::uint32_t half_cnt = 0;
    std::uint32_t full_cnt = 0;
    std::uint32_t stress_cnt = 0;
    const auto start = HAL_GetTick();
    while ((HAL_GetTick() - start) < 60000u) {
        if (g_half_ready) {
            g_half_ready = false;
            ++half_cnt;
            for (std::size_t i = 0; i < g_i2s_test_buffer.size() / 2; i += 2) {
                sample = static_cast<std::uint16_t>(sample + 0x1111u);
                g_i2s_test_buffer[i] = sample;
                g_i2s_test_buffer[i + 1] = sample;
            }
            for (volatile int i = 0; i < 20000; ++i) {
                __asm volatile("");
            }
            ++stress_cnt;
        }
        if (g_full_ready) {
            g_full_ready = false;
            ++full_cnt;
            const std::size_t base = g_i2s_test_buffer.size() / 2;
            for (std::size_t i = 0; i < g_i2s_test_buffer.size() / 2; i += 2) {
                sample = static_cast<std::uint16_t>(sample + 0x1111u);
                g_i2s_test_buffer[base + i] = sample;
                g_i2s_test_buffer[base + i + 1] = sample;
            }
            for (volatile int i = 0; i < 20000; ++i) {
                __asm volatile("");
            }
            ++stress_cnt;
        }
    }
    HAL_I2S_DMAStop(&hi2s2);
    out::println<"i2s test: half={} full={} underrun={} err={}">(
        half_cnt, full_cnt, g_underruns, static_cast<int>(HAL_I2S_GetError(&hi2s2)));
    out::println<"i2s test: stress={}">(
        stress_cnt);
}
