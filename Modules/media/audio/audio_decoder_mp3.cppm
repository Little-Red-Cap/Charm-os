module;

#define DR_MP3_IMPLEMENTATION
#include "../../../Examples/ThirdParty/dr_mp3/dr_mp3.h"

#include <cstdint>
#include <cstdio>
#include <expected>
#include <span>

export module audio.decoder.mp3;

import audio.result;
import audio.source.file;

export namespace audio {
    struct Mp3Info {
        std::uint32_t sample_rate{0};
        std::uint16_t channels{0};
    };

    namespace mp3_detail {
        std::size_t on_read(void* user, void* buffer_out, std::size_t bytes_to_read) {
            auto* src = static_cast<FileDataSource*>(user);
            auto res = src->read(std::span<std::byte>(reinterpret_cast<std::byte*>(buffer_out), bytes_to_read));
            if (!res) return 0;
            return *res;
        }

        drmp3_bool32 on_seek(void* user, int offset, drmp3_seek_origin origin) {
            auto* src = static_cast<FileDataSource*>(user);
            int whence = SEEK_SET;
            if (origin == DRMP3_SEEK_CUR) whence = SEEK_CUR;
            if (origin == DRMP3_SEEK_END) whence = SEEK_END;
            auto res = src->seek(static_cast<std::int64_t>(offset), whence);
            return res.has_value() ? DRMP3_TRUE : DRMP3_FALSE;
        }

        drmp3_bool32 on_tell(void* user, drmp3_int64* cursor) {
            auto* src = static_cast<FileDataSource*>(user);
            auto res = src->tell();
            if (!res) return DRMP3_FALSE;
            if (cursor) {
                *cursor = static_cast<drmp3_int64>(*res);
            }
            return DRMP3_TRUE;
        }
    }

    class Mp3Decoder {
    public:
        Mp3Decoder() = default;
        ~Mp3Decoder() { close(); }

        Mp3Decoder(const Mp3Decoder&) = delete;
        Mp3Decoder& operator=(const Mp3Decoder&) = delete;

        Result<Mp3Info> open(FileDataSource& src) {
            close();
            src_ = &src;
            if (!drmp3_init(&mp3_, mp3_detail::on_read, mp3_detail::on_seek, mp3_detail::on_tell, nullptr, src_, nullptr)) {
                return std::unexpected(Err{Errc::invalid_arg, 0});
            }
            Mp3Info info{};
            info.sample_rate = mp3_.sampleRate;
            info.channels = static_cast<std::uint16_t>(mp3_.channels);
            return info;
        }

        Result<std::size_t> read_s16(std::int16_t* out, std::size_t frames) {
            if (!src_) return std::unexpected(Err{Errc::bad_state, 0});
            const auto read = drmp3_read_pcm_frames_s16(&mp3_, frames, out);
            return static_cast<std::size_t>(read);
        }

        void close() {
            if (src_) {
                drmp3_uninit(&mp3_);
                src_ = nullptr;
            }
        }

    private:
        drmp3 mp3_{};
        FileDataSource* src_{nullptr};
    };
}
