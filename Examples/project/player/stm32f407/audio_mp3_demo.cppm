module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "i2s.h"

export module player.stm32.audio_mp3_demo;

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

namespace {
    constexpr std::uint32_t kTimeoutMs = 1000;
    constexpr std::size_t kI2sBufSamples = 512;

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
} // namespace

export void audio_mp3_demo_run() noexcept {
    HAL_I2S_DMAStop(&hi2s2);
    FindAudioCtx ctx{};
    auto st = fs::vfs_list("/MUSIC", &ctx, &find_first_mp3);
    if (!st || !ctx.found) {
        out::println<"mp3 demo: no mp3 found">();
        return;
    }

    out::println<"mp3 demo: open {}">(ctx.path);
    fs::File f{};
    st = fs::vfs_open(ctx.path, f);
    if (!st) {
        out::println<"mp3 demo: open failed {}">(static_cast<int>(st.err));
        return;
    }

    FileSource src{&f};
    auto ref = media::make_stream_source_ref(src);
    audio::Mp3Filter filter{};
    auto rst = filter.open(ref);
    if (!rst) {
        out::println<"mp3 demo: decoder open failed">();
        (void)fs::vfs_close(f);
        return;
    }
    const auto fmt = filter.format();
    if (fmt.channels != 1 && fmt.channels != 2) {
        out::println<"mp3 demo: channels {} not supported">(fmt.channels);
        filter.close();
        (void)fs::vfs_close(f);
        return;
    }
    out::println<"mp3 demo: rate={} ch={}">(fmt.rate, fmt.channels);
    if (!reinit_i2s(fmt.rate)) {
        out::println<"mp3 demo: unsupported sample rate {}">(fmt.rate);
        filter.close();
        (void)fs::vfs_close(f);
        return;
    }

    std::array<std::int16_t, kI2sBufSamples * 2> pcm{};
    std::array<std::int16_t, kI2sBufSamples * 2> stereo{};
    const std::size_t frame_bytes = fmt.channels * sizeof(std::int16_t);
    std::size_t block_count = 0;

    while (true) {
        auto res = filter.process({}, std::span<std::byte>(
            reinterpret_cast<std::byte*>(pcm.data()), pcm.size() * sizeof(std::int16_t)));
        if (!res) {
            out::println<"mp3 demo: decode error">();
            break;
        }
        if (res->produced == 0 && res->end_of_stream) {
            out::println<"mp3 demo: end">();
            break;
        }
        const std::size_t frames = res->produced / frame_bytes;
        if (frames == 0) continue;

        if (fmt.channels == 2) {
            const std::size_t samples = frames * 2;
            if (HAL_I2S_Transmit(&hi2s2,
                    reinterpret_cast<uint16_t*>(pcm.data()),
                    static_cast<uint16_t>(samples),
                    kTimeoutMs) != HAL_OK) {
                out::println<"mp3 demo: i2s tx failed">();
                break;
            }
        } else {
            for (std::size_t i = 0; i < frames; ++i) {
                const auto s = pcm[i];
                stereo[i * 2] = s;
                stereo[i * 2 + 1] = s;
            }
            const std::size_t samples = frames * 2;
            if (HAL_I2S_Transmit(&hi2s2,
                    reinterpret_cast<uint16_t*>(stereo.data()),
                    static_cast<uint16_t>(samples),
                    kTimeoutMs) != HAL_OK) {
                out::println<"mp3 demo: i2s tx failed">();
                break;
            }
        }

        if ((++block_count % 100) == 0) {
            out::println<"mp3 demo: blocks={}">(block_count);
        }
    }

    filter.close();
    (void)fs::vfs_close(f);
}
