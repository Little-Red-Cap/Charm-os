module;

#define DR_FLAC_IMPLEMENTATION
#include "../Examples/ThirdParty/dr_flac/dr_flac.h"

#include <cstdint>
#include <cstdio>
#include <expected>
#include <span>

export module audio.decoder.flac;

import audio.result;
import audio.source.file;

export namespace audio {
    struct FlacInfo {
        std::uint32_t sample_rate{0};
        std::uint16_t channels{0};
    };

    namespace detail {
        std::size_t on_read(void* user, void* buffer_out, std::size_t bytes_to_read) {
            auto* src = static_cast<FileDataSource*>(user);
            auto res = src->read(std::span<std::byte>(reinterpret_cast<std::byte*>(buffer_out), bytes_to_read));
            if (!res) return 0;
            return *res;
        }

        drflac_bool32 on_seek(void* user, int offset, drflac_seek_origin origin) {
            auto* src = static_cast<FileDataSource*>(user);
            int whence = SEEK_SET;
            if (origin == DRFLAC_SEEK_CUR) whence = SEEK_CUR;
            if (origin == DRFLAC_SEEK_END) whence = SEEK_END;
            auto res = src->seek(static_cast<std::int64_t>(offset), whence);
            return res.has_value() ? DRFLAC_TRUE : DRFLAC_FALSE;
        }

        drflac_bool32 on_tell(void* user, drflac_int64* cursor) {
            auto* src = static_cast<FileDataSource*>(user);
            auto res = src->tell();
            if (!res) return DRFLAC_FALSE;
            if (cursor) {
                *cursor = static_cast<drflac_int64>(*res);
            }
            return DRFLAC_TRUE;
        }
    }

    class FlacDecoder {
    public:
        FlacDecoder() = default;
        ~FlacDecoder() { close(); }

        FlacDecoder(const FlacDecoder&) = delete;
        FlacDecoder& operator=(const FlacDecoder&) = delete;

        Result<FlacInfo> open(FileDataSource& src) {
            close();
            src_ = &src;
            flac_ = drflac_open(detail::on_read, detail::on_seek, detail::on_tell, src_, nullptr);
            if (!flac_) {
                return std::unexpected(Err{Errc::invalid_arg, 0});
            }
            FlacInfo info{};
            info.sample_rate = flac_->sampleRate;
            info.channels = static_cast<std::uint16_t>(flac_->channels);
            return info;
        }

        Result<std::size_t> read_s32(std::int32_t* out, std::size_t frames) {
            if (!flac_) return std::unexpected(Err{Errc::bad_state, 0});
            const auto read = drflac_read_pcm_frames_s32(flac_, frames, out);
            return static_cast<std::size_t>(read);
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
        FileDataSource* src_{nullptr};
    };
}
