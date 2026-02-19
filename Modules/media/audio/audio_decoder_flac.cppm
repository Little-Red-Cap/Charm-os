module;

#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>

#include <cstdint>
#include <cstdio>
#include <span>

export module audio.decoder.flac;

import audio.result;

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
                auto res = src->read(std::span<std::byte>(reinterpret_cast<std::byte*>(buffer_out), bytes_to_read));
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
            const auto read = drflac_read_pcm_frames_s32(flac_, frames, out);
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
}
