module;

#include <cstddef>
#include <cstdint>
#include <cstdio>

export module audio.source.file;

import audio.result;
import media.stream.source;
import util.span;

export namespace audio {
    class FileDataSource : public media::IStreamSource {
    public:
        FileDataSource() = default;
        explicit FileDataSource(const char* path) { (void)open(path); }

        bool open(const char* path) {
            close();
            file_ = std::fopen(path, "rb");
            if (!file_) return false;
            return true;
        }

        void close() {
            if (file_) {
                std::fclose(file_);
                file_ = nullptr;
            }
        }

        ~FileDataSource() { close(); }

        Result<std::size_t> read(util::span<std::byte> out) noexcept override {
            if (!file_) return unexpected(Err{Errc::bad_state, 0});
            const auto n = std::fread(out.data(), 1, out.size(), file_);
            if (n == 0 && std::ferror(file_)) {
                return unexpected(Err{Errc::io_error, 0});
            }
            return n;
        }

        Result<std::int64_t> seek(std::int64_t offset, int whence) {
            if (!file_) return unexpected(Err{Errc::bad_state, 0});
            if (std::fseek(file_, static_cast<long>(offset), whence) != 0) {
                return unexpected(Err{Errc::io_error, 0});
            }
            return tell();
        }

        Result<std::int64_t> seek(std::int64_t offset, media::SeekWhence whence) noexcept override {
            int mapped = SEEK_SET;
            switch (whence) {
            case media::SeekWhence::set:
                mapped = SEEK_SET;
                break;
            case media::SeekWhence::cur:
                mapped = SEEK_CUR;
                break;
            case media::SeekWhence::end:
                mapped = SEEK_END;
                break;
            }
            return seek(offset, mapped);
        }

        Result<std::int64_t> tell() noexcept override {
            if (!file_) return unexpected(Err{Errc::bad_state, 0});
            const long pos = std::ftell(file_);
            if (pos < 0) return unexpected(Err{Errc::io_error, 0});
            return static_cast<std::int64_t>(pos);
        }

        Result<std::int64_t> size() noexcept override {
            if (!file_) return unexpected(Err{Errc::bad_state, 0});
            const auto cur = tell();
            if (!cur) return unexpected(cur.error());
            if (std::fseek(file_, 0, SEEK_END) != 0) {
                return unexpected(Err{Errc::io_error, 0});
            }
            const auto end = tell();
            if (!end) return unexpected(end.error());
            (void)seek(*cur, SEEK_SET);
            return end;
        }

    private:
        std::FILE* file_{nullptr};
    };
}
