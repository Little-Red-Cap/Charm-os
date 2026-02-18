module;

#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>

#include <cstdint>
#include <cstdio>
#include <span>

export module audio.decoder.mp3;

import audio.result;

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
                return unexpected(Err{Errc::invalid_arg, 0});
            }
            Mp3Info info{};
            info.sample_rate = mp3_.sampleRate;
            info.channels = static_cast<std::uint16_t>(mp3_.channels);
            return info;
        }

        Result<std::size_t> read_s16(std::int16_t* out, std::size_t frames) {
            if (!src_) return unexpected(Err{Errc::bad_state, 0});
            const auto read = drmp3_read_pcm_frames_s16(&mp3_, frames, out);
            return static_cast<std::size_t>(read);
        }

        Result<void> seek_pcm_frame(std::uint64_t frame) {
            if (!src_) return unexpected(Err{Errc::bad_state, 0});
            const auto ok = drmp3_seek_to_pcm_frame(&mp3_, static_cast<drmp3_uint64>(frame));
            return ok ? Result<void>{} : unexpected(Err{Errc::invalid_arg, 0});
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
}
