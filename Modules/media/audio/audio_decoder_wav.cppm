module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

export module audio.decoder.wav;

import audio.result;
import util.span;

export namespace audio {
    struct WavInfo {
        std::uint16_t channels{0};
        std::uint32_t sample_rate{0};
        std::uint16_t bits_per_sample{0};
        std::uint32_t data_offset{0};
        std::uint32_t data_size{0};
    };

    namespace detail {
        struct ChunkHeader {
            char id[4];
            std::uint32_t size{0};
        };

        struct RiffHeader {
            char riff[4];
            std::uint32_t size{0};
            char wave[4];
        };

        struct FmtChunk {
            std::uint16_t audio_format{0};
            std::uint16_t channels{0};
            std::uint32_t sample_rate{0};
            std::uint32_t byte_rate{0};
            std::uint16_t block_align{0};
            std::uint16_t bits_per_sample{0};
        };

        bool match_id(const char* id, const char* value) {
            return std::memcmp(id, value, 4) == 0;
        }
    }

    template <typename Source>
    inline Result<WavInfo> parse_wav(Source& src) {
        auto start = src.seek(0, SEEK_SET);
        if (!start) return unexpected(start.error());

        detail::RiffHeader riff{};
        auto read_riff = src.read(util::span<std::byte>(reinterpret_cast<std::byte*>(&riff), sizeof(riff)));
        if (!read_riff || *read_riff != sizeof(riff)) {
            return unexpected(Err{Errc::invalid_arg, 0});
        }
        if (!detail::match_id(riff.riff, "RIFF") || !detail::match_id(riff.wave, "WAVE")) {
            return unexpected(Err{Errc::invalid_arg, 0});
        }

        WavInfo info{};
        bool got_fmt = false;
        bool got_data = false;

        while (true) {
        detail::ChunkHeader ch{};
            auto read_ch = src.read(util::span<std::byte>(reinterpret_cast<std::byte*>(&ch), sizeof(ch)));
            if (!read_ch || *read_ch != sizeof(ch)) break;

            if (detail::match_id(ch.id, "fmt ")) {
                if (ch.size < sizeof(detail::FmtChunk)) {
                    return unexpected(Err{Errc::invalid_arg, 0});
                }
                detail::FmtChunk fmt{};
                auto read_fmt = src.read(util::span<std::byte>(reinterpret_cast<std::byte*>(&fmt), sizeof(fmt)));
                if (!read_fmt || *read_fmt != sizeof(fmt)) {
                    return unexpected(Err{Errc::io_error, 0});
                }
                const auto skip = static_cast<std::int64_t>(ch.size - sizeof(fmt));
                if (skip > 0) {
                    auto sk = src.seek(skip, SEEK_CUR);
                    if (!sk) return unexpected(sk.error());
                }
                if (fmt.audio_format != 1) {
                    return unexpected(Err{Errc::not_supported, 0});
                }
                info.channels = fmt.channels;
                info.sample_rate = fmt.sample_rate;
                info.bits_per_sample = fmt.bits_per_sample;
                got_fmt = true;
            } else if (detail::match_id(ch.id, "data")) {
                auto pos = src.tell();
                if (!pos) return unexpected(pos.error());
                info.data_offset = static_cast<std::uint32_t>(*pos);
                info.data_size = ch.size;
                auto sk = src.seek(ch.size, SEEK_CUR);
                if (!sk) return unexpected(sk.error());
                got_data = true;
            } else {
                auto sk = src.seek(ch.size, SEEK_CUR);
                if (!sk) return unexpected(sk.error());
            }

            if (got_fmt && got_data) break;
        }

        if (!got_fmt || !got_data) {
            return unexpected(Err{Errc::invalid_arg, 0});
        }

        return info;
    }
}
