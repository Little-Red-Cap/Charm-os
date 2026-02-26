module;

#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

export module audio.decoder.flac;

import audio.result;
import util.span;
import media.stream.source;
import media.stream.filter;
import media.stream.types;

export namespace audio {
    struct FlacInfo {
        std::uint32_t sample_rate{0};
        std::uint16_t channels{0};
    };

    namespace detail {
        template <typename Source>
        struct SourceOps {
            static std::size_t on_read(void* user, void* buffer_out, std::size_t bytes_to_read) {
                auto* src = static_cast<Source*>(user);
                auto res = src->read(util::span<std::byte>(reinterpret_cast<std::byte*>(buffer_out), bytes_to_read));
                if (!res) return 0;
                return *res;
            }

            static drflac_bool32 on_seek(void* user, int offset, drflac_seek_origin origin) {
                auto* src = static_cast<Source*>(user);
                int whence = SEEK_SET;
                if (origin == DRFLAC_SEEK_CUR) whence = SEEK_CUR;
                if (origin == DRFLAC_SEEK_END) whence = SEEK_END;
                auto res = src->seek(static_cast<std::int64_t>(offset), whence);
                return res.has_value() ? DRFLAC_TRUE : DRFLAC_FALSE;
            }

            static drflac_bool32 on_tell(void* user, drflac_int64* cursor) {
                auto* src = static_cast<Source*>(user);
                auto res = src->tell();
                if (!res) return DRFLAC_FALSE;
                if (cursor) {
                    *cursor = static_cast<drflac_int64>(*res);
                }
                return DRFLAC_TRUE;
            }
        };
    }

    class FlacDecoder {
    public:
        FlacDecoder() = default;
        ~FlacDecoder() { close(); }

        FlacDecoder(const FlacDecoder&) = delete;
        FlacDecoder& operator=(const FlacDecoder&) = delete;

        template <typename Source>
        Result<FlacInfo> open(Source& src) {
            close();
            src_ = &src;
            flac_ = drflac_open(
                detail::SourceOps<Source>::on_read,
                detail::SourceOps<Source>::on_seek,
                detail::SourceOps<Source>::on_tell,
                src_, nullptr);
            if (!flac_) {
                return unexpected(Err{Errc::invalid_arg, 0});
            }
            flac_->_noSeekTableSeek = DRFLAC_TRUE;
            flac_->_noBinarySearchSeek = DRFLAC_TRUE;
            FlacInfo info{};
            info.sample_rate = flac_->sampleRate;
            info.channels = static_cast<std::uint16_t>(flac_->channels);
            return info;
        }

        Result<std::size_t> read_s32(std::int32_t* out, std::size_t frames) {
            if (!flac_) return unexpected(Err{Errc::bad_state, 0});
            const auto read = drflac_read_pcm_frames_s32(
                flac_, frames, reinterpret_cast<drflac_int32*>(out));
            return static_cast<std::size_t>(read);
        }

        Result<void> seek_pcm_frame(std::uint64_t frame) {
            if (!flac_) return unexpected(Err{Errc::bad_state, 0});
            const auto total = static_cast<std::uint64_t>(flac_->totalPCMFrameCount);
            if (total == 0) return unexpected(Err{Errc::not_supported, 0});
            if (frame >= total) frame = total - 1;
            flac_->_noSeekTableSeek = DRFLAC_TRUE;
            flac_->_noBinarySearchSeek = DRFLAC_TRUE;
            flac_->_noBruteForceSeek = DRFLAC_FALSE;
            const auto ok = drflac_seek_to_pcm_frame(flac_, static_cast<drflac_uint64>(frame));
            return ok ? Result<void>{} : unexpected(Err{Errc::invalid_arg, 0});
        }

        std::uint64_t total_frames() const noexcept {
            if (!flac_) return 0;
            return static_cast<std::uint64_t>(flac_->totalPCMFrameCount);
        }

        void close() {
            if (flac_) {
                drflac_close(flac_);
                flac_ = nullptr;
            }
            src_ = nullptr;
        }

    private:
        drflac* flac_{nullptr};
        void* src_{nullptr};
    };

    class FlacFilter : public media::IStreamFilter {
    public:
        Result<void> open(media::IStreamSource& src) noexcept {
            view_.src = &src;
            auto info = decoder_.open(view_);
            if (!info) return unexpected(info.error());
            info_ = *info;
            opened_ = true;
            return Result<void>{};
        }

        void close() noexcept {
            decoder_.close();
            opened_ = false;
            view_.src = nullptr;
        }

        Result<void> reset() noexcept override {
            if (!opened_) return unexpected(Err{Errc::bad_state, 0});
            return decoder_.seek_pcm_frame(0);
        }

        Result<media::FilterResult> process(util::span<const std::byte>,
                                            util::span<std::byte> out) noexcept override {
            if (!opened_) return unexpected(Err{Errc::bad_state, 0});
            const std::size_t frame_bytes = static_cast<std::size_t>(info_.channels) * sizeof(std::int32_t);
            if (frame_bytes == 0) return unexpected(Err{Errc::bad_state, 0});
            const std::size_t frames = out.size() / frame_bytes;
            if (frames == 0) return media::FilterResult{};
            auto* pcm = reinterpret_cast<std::int32_t*>(out.data());
            auto read = decoder_.read_s32(pcm, frames);
            if (!read) return unexpected(read.error());
            const std::size_t produced = (*read) * frame_bytes;
            return media::FilterResult{0, produced, *read == 0};
        }

        media::StreamFormat format() const noexcept override {
            media::StreamFormat fmt{};
            fmt.kind = media::StreamKind::audio;
            fmt.rate = info_.sample_rate;
            fmt.channels = info_.channels;
            fmt.bits_per_sample = 32;
            return fmt;
        }

        Result<void> seek_pcm_frame(std::uint64_t frame) noexcept {
            if (!opened_) return unexpected(Err{Errc::bad_state, 0});
            return decoder_.seek_pcm_frame(frame);
        }

        std::uint64_t total_frames() const noexcept {
            return decoder_.total_frames();
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

        FlacDecoder decoder_{};
        SourceView view_{};
        FlacInfo info_{};
        bool opened_{false};
    };
}
