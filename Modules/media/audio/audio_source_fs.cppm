module;

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string_view>

export module audio.source.fs;

import audio.result;
import fs_core;
import fs_stream;
import fs_vfs;
import util.core;
import media.stream.source;

export namespace audio {
    class FsDataSource {
    public:
        FsDataSource() = default;
        explicit FsDataSource(const char* path) { (void)open(path); }

        bool open(const char* path) {
            close();
            if (!path) return false;
            auto st = fs::vfs_open(std::string_view{path}, file_);
            if (!st) return false;
            opened_ = true;
            return true;
        }

        void close() {
            if (opened_) {
                (void)fs::flush(file_);
                (void)fs::vfs_close(file_);
                opened_ = false;
            }
        }

        ~FsDataSource() { close(); }

        Result<std::size_t> read(std::span<std::byte> out) noexcept {
            if (!opened_) return unexpected(Err{Errc::bad_state, 0});
            std::size_t to_read = out.size();
            if (file_.node.size > 0 && file_.node.offset >= 0) {
                const auto remaining = static_cast<std::int64_t>(file_.node.size - file_.node.offset);
                if (remaining <= 0) {
                    return static_cast<std::size_t>(0);
                }
                if (static_cast<std::uint64_t>(remaining) < to_read) {
                    to_read = static_cast<std::size_t>(remaining);
                }
            }
            const auto before = file_.node.offset;
            const auto st = fs::read(file_, std::span<util::u8>(
                reinterpret_cast<util::u8*>(out.data()), to_read));
            if (!st) return unexpected(Err{Errc::io_error, 0});
            const auto after = file_.node.offset;
            if (after >= before) {
                const auto delta = static_cast<std::size_t>(after - before);
                return (delta <= to_read) ? delta : to_read;
            }
            return to_read;
        }

        Result<std::int64_t> seek(std::int64_t offset, int whence) {
            if (!opened_) return unexpected(Err{Errc::bad_state, 0});
            std::int64_t target = offset;
            const auto cur = static_cast<std::int64_t>(file_.node.offset);
            const auto size = static_cast<std::int64_t>(file_.node.size);
            if (whence == SEEK_CUR) {
                target = cur + offset;
            } else if (whence == SEEK_END) {
                target = size + offset;
            }
            if (target < 0) return unexpected(Err{Errc::io_error, 0});
            const auto st = fs::seek(file_, static_cast<util::i64>(target));
            if (!st) return unexpected(Err{Errc::io_error, 0});
            return tell();
        }

        Result<std::int64_t> seek(std::int64_t offset, media::SeekWhence whence) noexcept {
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

        Result<std::int64_t> tell() noexcept {
            if (!opened_) return unexpected(Err{Errc::bad_state, 0});
            return static_cast<std::int64_t>(file_.node.offset);
        }

        Result<std::int64_t> size() noexcept {
            if (!opened_) return unexpected(Err{Errc::bad_state, 0});
            return static_cast<std::int64_t>(file_.node.size);
        }

    private:
        fs::File file_{};
        bool opened_{false};
    };
}
