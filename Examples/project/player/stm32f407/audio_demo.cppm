module;
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "i2s.h"
#include "usart.h"

export module player.stm32.audio_demo;

import audio.decoder.mp3;
import audio.decoder.flac;
import util.core;
import util.expected;
import fs_core;
import fs_errno;
import fs_stream;
import fs_vfs;
import media.stream.source;
import media.stream.types;

namespace {
    constexpr util::u32 kTimeoutMs = 1000;
    constexpr std::size_t kReadBufSize = 2048;
    constexpr std::size_t kI2sBufSamples = 1024;

    void uart_write(const char* text) noexcept {
        if (!text) return;
        HAL_UART_Transmit(&huart1,
            reinterpret_cast<uint8_t*>(const_cast<char*>(text)),
            static_cast<uint16_t>(std::strlen(text)), kTimeoutMs);
    }

    void uart_write_uint(util::u32 value) noexcept {
        char buf[32]{};
        std::size_t pos = 0;
        if (value == 0) {
            buf[pos++] = '0';
        } else {
            char tmp[32]{};
            std::size_t len = 0;
            while (value > 0 && len < sizeof(tmp)) {
                tmp[len++] = static_cast<char>('0' + (value % 10));
                value /= 10;
            }
            while (len > 0) {
                buf[pos++] = tmp[--len];
            }
        }
        buf[pos++] = '\r';
        buf[pos++] = '\n';
        buf[pos] = '\0';
        uart_write(buf);
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

    bool is_wav_name(std::string_view name) noexcept {
        const auto len = name_len(name);
        if (len < 4) return false;
        const char c0 = ascii_lower(name.data()[len - 4]);
        const char c1 = ascii_lower(name.data()[len - 3]);
        const char c2 = ascii_lower(name.data()[len - 2]);
        const char c3 = ascii_lower(name.data()[len - 1]);
        return c0 == '.' && c1 == 'w' && c2 == 'a' && c3 == 'v';
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

    bool is_flac_name(std::string_view name) noexcept {
        const auto len = name_len(name);
        if (len < 5) return false;
        const char c0 = ascii_lower(name.data()[len - 5]);
        const char c1 = ascii_lower(name.data()[len - 4]);
        const char c2 = ascii_lower(name.data()[len - 3]);
        const char c3 = ascii_lower(name.data()[len - 2]);
        const char c4 = ascii_lower(name.data()[len - 1]);
        return c0 == '.' && c1 == 'f' && c2 == 'l' && c3 == 'a' && c4 == 'c';
    }

    struct WavInfo {
        util::u16 channels{0};
        util::u32 sample_rate{0};
        util::u16 bits_per_sample{0};
        util::u32 data_offset{0};
        util::u32 data_size{0};
    };

    util::u16 read_u16_le(const util::u8* data) noexcept {
        return static_cast<util::u16>(data[0] | (static_cast<util::u16>(data[1]) << 8));
    }

    util::u32 read_u32_le(const util::u8* data) noexcept {
        return static_cast<util::u32>(data[0])
            | (static_cast<util::u32>(data[1]) << 8)
            | (static_cast<util::u32>(data[2]) << 16)
            | (static_cast<util::u32>(data[3]) << 24);
    }

    bool read_wav_header(fs::File& f, WavInfo& info) noexcept {
        std::array<util::u8, 12> riff{};
        auto st = fs::vfs_read(f, std::span<util::u8>{riff});
        if (!st) return false;
        if (std::memcmp(riff.data(), "RIFF", 4) != 0 || std::memcmp(riff.data() + 8, "WAVE", 4) != 0) {
            return false;
        }
        util::u32 cursor = 12;
        bool fmt_ok = false;
        bool data_ok = false;
        while (!fmt_ok || !data_ok) {
            std::array<util::u8, 8> chunk{};
            st = fs::vfs_read(f, std::span<util::u8>{chunk});
            if (!st) return false;
            cursor += 8;
            const util::u32 size = read_u32_le(chunk.data() + 4);
            if (std::memcmp(chunk.data(), "fmt ", 4) == 0) {
                if (size < 16) return false;
                std::array<util::u8, 16> fmt{};
                st = fs::vfs_read(f, std::span<util::u8>{fmt});
                if (!st) return false;
                cursor += 16;
                const util::u16 format = read_u16_le(fmt.data());
                info.channels = read_u16_le(fmt.data() + 2);
                info.sample_rate = read_u32_le(fmt.data() + 4);
                info.bits_per_sample = read_u16_le(fmt.data() + 14);
                if (format != 1) return false;
                if (size > 16) {
                    cursor += static_cast<util::u32>(size - 16);
                    st = fs::vfs_seek(f, static_cast<util::i64>(cursor));
                    if (!st) return false;
                }
                fmt_ok = true;
            } else if (std::memcmp(chunk.data(), "data", 4) == 0) {
                info.data_offset = cursor;
                info.data_size = size;
                data_ok = true;
                cursor += size;
                st = fs::vfs_seek(f, static_cast<util::i64>(cursor));
                if (!st) return false;
            } else {
                cursor += size;
                st = fs::vfs_seek(f, static_cast<util::i64>(cursor));
                if (!st) return false;
            }
        }
        return true;
    }

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
        HAL_I2S_DeInit(&hi2s2);
        return HAL_I2S_Init(&hi2s2) == HAL_OK;
    }

    bool stream_pcm(fs::File& f, const WavInfo& info) noexcept {
        if (info.bits_per_sample != 16 || (info.channels != 1 && info.channels != 2)) {
            uart_write("wav: only 16-bit mono/stereo supported\r\n");
            return false;
        }
        if (!reinit_i2s(info.sample_rate)) {
            uart_write("wav: unsupported sample rate\r\n");
            return false;
        }
        auto st = fs::vfs_seek(f, static_cast<util::i64>(info.data_offset));
        if (!st) return false;

        std::array<util::u8, kReadBufSize> read_buf{};
        std::array<util::u16, kI2sBufSamples> i2s_buf{};
        util::u32 remaining = info.data_size;
        const std::size_t frame_bytes = static_cast<std::size_t>(info.channels) * sizeof(util::u16);

        while (remaining > 0) {
            const std::size_t want = std::min<std::size_t>(read_buf.size(), remaining);
            const auto before = f.node.offset;
            st = fs::vfs_read(f, std::span<util::u8>{read_buf.data(), want});
            if (!st) return false;
            const auto after = f.node.offset;
            if (after < before) return false;
            const std::size_t got = static_cast<std::size_t>(after - before);
            if (got == 0) break;
            remaining -= static_cast<util::u32>(got);
            const std::size_t usable = got - (got % frame_bytes);
            if (usable == 0) continue;

            const util::u16* samples = reinterpret_cast<const util::u16*>(read_buf.data());
            const std::size_t sample_count = usable / sizeof(util::u16);

            if (info.channels == 2) {
                std::size_t offset = 0;
                while (offset < sample_count) {
                    const std::size_t chunk = std::min<std::size_t>(kI2sBufSamples, sample_count - offset);
                    std::memcpy(i2s_buf.data(), samples + offset, chunk * sizeof(util::u16));
                    if (HAL_I2S_Transmit(&hi2s2, i2s_buf.data(), static_cast<uint16_t>(chunk), kTimeoutMs) != HAL_OK) {
                        return false;
                    }
                    offset += chunk;
                }
            } else {
                std::size_t offset = 0;
                while (offset < sample_count) {
                    const std::size_t frames = std::min<std::size_t>(kI2sBufSamples / 2, sample_count - offset);
                    for (std::size_t i = 0; i < frames; ++i) {
                        const util::u16 s = samples[offset + i];
                        i2s_buf[i * 2] = s;
                        i2s_buf[i * 2 + 1] = s;
                    }
                    if (HAL_I2S_Transmit(&hi2s2, i2s_buf.data(), static_cast<uint16_t>(frames * 2), kTimeoutMs) != HAL_OK) {
                        return false;
                    }
                    offset += frames;
                }
            }
        }
        return true;
    }

    enum class AudioKind : util::u8 { wav, mp3, flac };

    struct FindAudioCtx {
        char path[128]{};
        bool found{false};
        AudioKind kind{AudioKind::wav};
    };

    int kind_priority(AudioKind kind) noexcept {
        switch (kind) {
            case AudioKind::mp3: return 3;
            case AudioKind::flac: return 2;
            case AudioKind::wav: return 1;
        }
        return 0;
    }

    fs::Status find_first_audio(void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
        auto* out = static_cast<FindAudioCtx*>(ctx);
        if (!out) return fs::Status{fs::Err::inval};
        if (entry.type != fs::NodeType::file) return fs::Status{fs::Err::ok};
        AudioKind kind{};
        if (is_mp3_name(entry.name)) {
            kind = AudioKind::mp3;
        } else if (is_flac_name(entry.name)) {
            kind = AudioKind::flac;
        } else if (is_wav_name(entry.name)) {
            kind = AudioKind::wav;
        } else {
            return fs::Status{fs::Err::ok};
        }
        if (out->found && kind_priority(kind) <= kind_priority(out->kind)) {
            return fs::Status{fs::Err::ok};
        }
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
        out->kind = kind;
        return fs::Status{fs::Err::ok};
    }

    struct FileSource {
        fs::File* file{nullptr};

        media::Result<util::usize> read(std::span<std::byte> out) noexcept {
            if (!file) return util::unexpected(media::Error{media::Errc::bad_state, 0});
            const auto remaining = file->node.size - file->node.offset;
            if (remaining <= 0) return static_cast<util::usize>(0);
            const auto to_read = static_cast<std::size_t>(
                std::min<util::i64>(remaining, static_cast<util::i64>(out.size())));
            const auto before = file->node.offset;
            auto st = fs::vfs_read(*file, std::span<util::u8>(
                reinterpret_cast<util::u8*>(out.data()), to_read));
            if (!st) return util::unexpected(media::Error{media::Errc::io_error, 0});
            const auto after = file->node.offset;
            if (after < before) return util::unexpected(media::Error{media::Errc::io_error, 0});
            return static_cast<util::usize>(after - before);
        }

        media::Result<util::i64> seek(util::i64 offset, media::SeekWhence whence) noexcept {
            if (!file) return util::unexpected(media::Error{media::Errc::bad_state, 0});
            util::i64 target = offset;
            if (whence == media::SeekWhence::cur) {
                target = file->node.offset + offset;
            } else if (whence == media::SeekWhence::end) {
                target = file->node.size + offset;
            }
            auto st = fs::vfs_seek(*file, target);
            if (!st) return util::unexpected(media::Error{media::Errc::io_error, 0});
            return target;
        }

        media::Result<util::i64> tell() noexcept {
            if (!file) return util::unexpected(media::Error{media::Errc::bad_state, 0});
            return file->node.offset;
        }

        media::Result<util::i64> size() noexcept {
            if (!file) return util::unexpected(media::Error{media::Errc::bad_state, 0});
            return file->node.size;
        }
    };

    bool stream_mp3(fs::File& f) noexcept {
        FileSource src{&f};
        auto ref = media::make_stream_source_ref(src);

        audio::Mp3Filter filter{};
        auto st = filter.open(ref);
        if (!st) return false;
        const auto fmt = filter.format();
        if (fmt.channels != 1 && fmt.channels != 2) return false;
        if (!reinit_i2s(fmt.rate)) return false;

        std::array<std::int16_t, kI2sBufSamples * 2> pcm{};
        std::array<std::int16_t, kI2sBufSamples * 2> stereo{};
        const std::size_t frame_bytes = fmt.channels * sizeof(std::int16_t);

        while (true) {
            auto res = filter.process({}, std::span<std::byte>(
                reinterpret_cast<std::byte*>(pcm.data()), pcm.size() * sizeof(std::int16_t)));
            if (!res) return false;
            if (res->produced == 0 && res->end_of_stream) break;
            const std::size_t frames = res->produced / frame_bytes;
            if (frames == 0) continue;
            if (fmt.channels == 2) {
                const std::size_t samples = frames * 2;
                if (HAL_I2S_Transmit(&hi2s2, reinterpret_cast<uint16_t*>(pcm.data()),
                        static_cast<uint16_t>(samples), kTimeoutMs) != HAL_OK) {
                    return false;
                }
            } else {
                for (std::size_t i = 0; i < frames; ++i) {
                    const auto s = pcm[i];
                    stereo[i * 2] = s;
                    stereo[i * 2 + 1] = s;
                }
                const std::size_t samples = frames * 2;
                if (HAL_I2S_Transmit(&hi2s2, reinterpret_cast<uint16_t*>(stereo.data()),
                        static_cast<uint16_t>(samples), kTimeoutMs) != HAL_OK) {
                    return false;
                }
            }
        }
        filter.close();
        return true;
    }

    bool stream_flac(fs::File& f) noexcept {
        FileSource src{&f};
        auto ref = media::make_stream_source_ref(src);

        audio::FlacFilter filter{};
        auto st = filter.open(ref);
        if (!st) return false;
        const auto fmt = filter.format();
        if (fmt.channels != 1 && fmt.channels != 2) return false;
        if (!reinit_i2s(fmt.rate)) return false;

        std::array<std::int32_t, kI2sBufSamples * 2> pcm32{};
        std::array<std::int16_t, kI2sBufSamples * 2> pcm16{};
        std::array<std::int16_t, kI2sBufSamples * 2> stereo{};
        const std::size_t frame_bytes = fmt.channels * sizeof(std::int32_t);

        while (true) {
            auto res = filter.process({}, std::span<std::byte>(
                reinterpret_cast<std::byte*>(pcm32.data()), pcm32.size() * sizeof(std::int32_t)));
            if (!res) return false;
            if (res->produced == 0 && res->end_of_stream) break;
            const std::size_t frames = res->produced / frame_bytes;
            if (frames == 0) continue;
            const std::size_t samples = frames * fmt.channels;
            for (std::size_t i = 0; i < samples; ++i) {
                std::int32_t v = pcm32[i] >> 16;
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;
                pcm16[i] = static_cast<std::int16_t>(v);
            }
            if (fmt.channels == 2) {
                if (HAL_I2S_Transmit(&hi2s2, reinterpret_cast<uint16_t*>(pcm16.data()),
                        static_cast<uint16_t>(samples), kTimeoutMs) != HAL_OK) {
                    return false;
                }
            } else {
                for (std::size_t i = 0; i < frames; ++i) {
                    const auto s = pcm16[i];
                    stereo[i * 2] = s;
                    stereo[i * 2 + 1] = s;
                }
                const std::size_t out_samples = frames * 2;
                if (HAL_I2S_Transmit(&hi2s2, reinterpret_cast<uint16_t*>(stereo.data()),
                        static_cast<uint16_t>(out_samples), kTimeoutMs) != HAL_OK) {
                    return false;
                }
            }
        }
        filter.close();
        return true;
    }
} // namespace

export void audio_demo_run() noexcept {
    uart_write("audio demo: scan /MUSIC\r\n");
    FindAudioCtx ctx{};
    auto st = fs::vfs_list("/MUSIC", &ctx, &find_first_audio);
    if (!st || !ctx.found) {
        uart_write("audio demo: no audio found\r\n");
        return;
    }
    uart_write("audio demo: open ");
    uart_write(ctx.path);
    uart_write("\r\n");

    fs::File f{};
    st = fs::vfs_open(ctx.path, f);
    if (!st) {
        uart_write("audio demo: open failed\r\n");
        uart_write_uint(static_cast<util::u32>(st.err));
        return;
    }

    bool ok = false;
    if (ctx.kind == AudioKind::wav) {
        WavInfo info{};
        if (!read_wav_header(f, info)) {
            uart_write("audio demo: invalid wav\r\n");
            (void)fs::vfs_close(f);
            return;
        }
        uart_write("audio demo: ch=");
        uart_write_uint(info.channels);
        uart_write("audio demo: rate=");
        uart_write_uint(info.sample_rate);
        ok = stream_pcm(f, info);
    } else if (ctx.kind == AudioKind::mp3) {
        uart_write("audio demo: mp3\r\n");
        ok = stream_mp3(f);
    } else {
        uart_write("audio demo: flac\r\n");
        ok = stream_flac(f);
    }

    if (!ok) {
        uart_write("audio demo: stream failed\r\n");
    } else {
        uart_write("audio demo: done\r\n");
    }
    (void)fs::vfs_close(f);
}
