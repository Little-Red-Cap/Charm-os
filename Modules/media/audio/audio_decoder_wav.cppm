module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

export module audio.decoder.wav;

import audio.result;
import util.span;
import media.stream.source;
import media.stream.filter;

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

    class WavFilter : public media::IStreamFilter {
    public:
        Result<void> open(media::IStreamSource& src) noexcept {
            src_ = &src;
            SourceView view{src_};
            auto parsed = parse_wav(view);
            if (!parsed) return unexpected(parsed.error());
            info_ = *parsed;
            data_end_ = static_cast<std::uint64_t>(info_.data_offset) +
                        static_cast<std::uint64_t>(info_.data_size);
            opened_ = true;
            return reset();
        }

        Result<void> reset() noexcept override {
            if (!src_ || !opened_) return unexpected(Err{Errc::bad_state, 0});
            auto res = src_->seek(static_cast<std::int64_t>(info_.data_offset), media::SeekWhence::set);
            return res ? Result<void>{} : unexpected(res.error());
        }

        Result<media::FilterResult> process(util::span<const std::byte>,
                                            util::span<std::byte> out) noexcept override {
            if (!src_ || !opened_) return unexpected(Err{Errc::bad_state, 0});
            if (out.empty()) return media::FilterResult{};
            auto pos = src_->tell();
            if (!pos) return unexpected(pos.error());
            const auto cur = static_cast<std::uint64_t>(*pos);
            if (cur >= data_end_) {
                return media::FilterResult{0, 0, true};
            }
            auto remaining = data_end_ - cur;
            if (remaining < out.size()) {
                out = util::span<std::byte>(out.data(), static_cast<std::size_t>(remaining));
            }
            auto rd = src_->read(out);
            if (!rd) return unexpected(rd.error());
            const bool eos = (*rd == 0) || (static_cast<std::uint64_t>(*rd) >= remaining);
            return media::FilterResult{0, *rd, eos};
        }

        media::StreamFormat format() const noexcept override {
            media::StreamFormat fmt{};
            fmt.kind = media::StreamKind::audio;
            fmt.rate = info_.sample_rate;
            fmt.channels = info_.channels;
            fmt.bits_per_sample = info_.bits_per_sample;
            return fmt;
        }

    private:
        struct SourceView {
            media::IStreamSource* src{nullptr};

            Result<std::size_t> read(util::span<std::byte> out) noexcept {
                return src->read(out);
            }

            Result<std::int64_t> seek(std::int64_t offset, int whence) noexcept {
                media::SeekWhence mapped = media::SeekWhence::set;
                if (whence == SEEK_CUR) mapped = media::SeekWhence::cur;
                if (whence == SEEK_END) mapped = media::SeekWhence::end;
                return src->seek(offset, mapped);
            }

            Result<std::int64_t> tell() noexcept { return src->tell(); }
        };

        media::IStreamSource* src_{nullptr};
        WavInfo info_{};
        std::uint64_t data_end_{0};
        bool opened_{false};
    };
}
