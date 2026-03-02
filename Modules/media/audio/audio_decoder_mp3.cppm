module;

#include <span>
#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

export module audio.decoder.mp3;

import audio.result;
import media.stream.source;
import media.stream.filter;
import media.stream.types;

export namespace audio {
    struct Mp3Info {
        std::uint32_t sample_rate{0};
        std::uint16_t channels{0};
    };

    namespace mp3_detail {
        template <typename Source>
        struct SourceOps {
            static std::size_t on_read(void* user, void* buffer_out, std::size_t bytes_to_read) {
                auto* src = static_cast<Source*>(user);
                auto res = src->read(std::span<std::byte>(reinterpret_cast<std::byte*>(buffer_out), bytes_to_read));
                if (!res) return 0;
                return *res;
            }

            static drmp3_bool32 on_seek(void* user, int offset, drmp3_seek_origin origin) {
                auto* src = static_cast<Source*>(user);
                int whence = SEEK_SET;
                if (origin == DRMP3_SEEK_CUR) whence = SEEK_CUR;
                if (origin == DRMP3_SEEK_END) whence = SEEK_END;
                auto res = src->seek(static_cast<std::int64_t>(offset), whence);
                return res.has_value() ? DRMP3_TRUE : DRMP3_FALSE;
            }

            static drmp3_bool32 on_tell(void* user, drmp3_int64* cursor) {
                auto* src = static_cast<Source*>(user);
                auto res = src->tell();
                if (!res) return DRMP3_FALSE;
                if (cursor) {
                    *cursor = static_cast<drmp3_int64>(*res);
                }
                return DRMP3_TRUE;
            }
        };
    }

    class Mp3Decoder {
    public:
        Mp3Decoder() = default;
        ~Mp3Decoder() { close(); }

        Mp3Decoder(const Mp3Decoder&) = delete;
        Mp3Decoder& operator=(const Mp3Decoder&) = delete;

        template <typename Source>
        Result<Mp3Info> open(Source& src) {
            close();
            src_ = &src;
            if (!drmp3_init(&mp3_,
                mp3_detail::SourceOps<Source>::on_read,
                mp3_detail::SourceOps<Source>::on_seek,
                mp3_detail::SourceOps<Source>::on_tell,
                nullptr, src_, nullptr)) {
                return unexpected(Errc::invalid_arg);
            }
            Mp3Info info{};
            info.sample_rate = mp3_.sampleRate;
            info.channels = static_cast<std::uint16_t>(mp3_.channels);
            return info;
        }

        Result<std::size_t> read_s16(std::int16_t* out, std::size_t frames) {
            if (!src_) return unexpected(Errc::bad_state);
            const auto read = drmp3_read_pcm_frames_s16(&mp3_, frames, out);
            return static_cast<std::size_t>(read);
        }

        Result<void> seek_pcm_frame(std::uint64_t frame) {
            if (!src_) return unexpected(Errc::bad_state);
            const auto ok = drmp3_seek_to_pcm_frame(&mp3_, static_cast<drmp3_uint64>(frame));
            return ok ? Result<void>{} : unexpected(Errc::invalid_arg);
        }

        std::uint64_t total_frames() const noexcept {
            if (!src_) return 0;
            const auto frames = drmp3_get_pcm_frame_count(const_cast<drmp3*>(&mp3_));
            return static_cast<std::uint64_t>(frames);
        }

        void close() {
            if (src_) {
                drmp3_uninit(&mp3_);
                src_ = nullptr;
            }
        }

    private:
        drmp3 mp3_{};
        void* src_{nullptr};
    };

    class Mp3Filter {
    public:
        Result<void> open(media::StreamSourceRef src) noexcept {
            view_.src = src;
            auto info = decoder_.open(view_);
            if (!info) return unexpected(info.error());
            info_ = *info;
            opened_ = true;
            return Result<void>{};
        }

        void close() noexcept {
            decoder_.close();
            opened_ = false;
            view_.src = {};
        }

        Result<void> reset() noexcept {
            if (!opened_) return unexpected(Errc::bad_state);
            return decoder_.seek_pcm_frame(0);
        }

        Result<media::FilterResult> process(std::span<const std::byte>,
                                            std::span<std::byte> out) noexcept {
            if (!opened_) return unexpected(Errc::bad_state);
            const std::size_t frame_bytes = static_cast<std::size_t>(info_.channels) * sizeof(std::int16_t);
            if (frame_bytes == 0) return unexpected(Errc::bad_state);
            const std::size_t frames = out.size() / frame_bytes;
            if (frames == 0) return media::FilterResult{};
            auto* pcm = reinterpret_cast<std::int16_t*>(out.data());
            auto read = decoder_.read_s16(pcm, frames);
            if (!read) return unexpected(read.error());
            const std::size_t produced = (*read) * frame_bytes;
            return media::FilterResult{0, produced, *read == 0};
        }

        media::StreamFormat format() const noexcept {
            media::StreamFormat fmt{};
            fmt.kind = media::StreamKind::audio;
            fmt.rate = info_.sample_rate;
            fmt.channels = info_.channels;
            fmt.bits_per_sample = 16;
            return fmt;
        }

        Result<void> seek_pcm_frame(std::uint64_t frame) noexcept {
            if (!opened_) return unexpected(Errc::bad_state);
            return decoder_.seek_pcm_frame(frame);
        }

        std::uint64_t total_frames() const noexcept {
            return decoder_.total_frames();
        }

    private:
        struct SourceView {
            media::StreamSourceRef src{};

            Result<std::size_t> read(std::span<std::byte> out) noexcept {
                return src.read(out);
            }

            Result<std::int64_t> seek(std::int64_t offset, int whence) noexcept {
                media::SeekWhence mapped = media::SeekWhence::set;
                if (whence == SEEK_CUR) mapped = media::SeekWhence::cur;
                if (whence == SEEK_END) mapped = media::SeekWhence::end;
                return src.seek(offset, mapped);
            }

            Result<std::int64_t> tell() noexcept { return src.tell(); }
        };

        Mp3Decoder decoder_{};
        SourceView view_{};
        Mp3Info info_{};
        bool opened_{false};
    };
}
