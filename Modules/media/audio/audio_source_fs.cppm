module;

#include <cstddef>
#include <cstdint>
#include <span>

export module audio.source.fs;

import audio.result;
import fs_core;
import fs_stream;
import fs_vfs;
import util.alias;
import util.core;

export namespace audio {
    class FsDataSource {
    public:
        FsDataSource() = default;
        explicit FsDataSource(const char* path) { (void)open(path); }

        bool open(const char* path) {
            close();
            if (!path) return false;
            auto st = fs::vfs_open(util::string_view{path}, file_);
            if (!st) return false;
            opened_ = true;
            return true;
        }

        void close() {
            if (opened_) {
                (void)fs::flush(file_);
                file_ = {};
                opened_ = false;
            }
        }

        ~FsDataSource() { close(); }

        Result<std::size_t> read(std::span<std::byte> out) {
            if (!opened_) return std::unexpected(Err{Errc::bad_state, 0});
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
            const auto st = fs::read(file_, std::span<util::u8>(
                reinterpret_cast<util::u8*>(out.data()), to_read));
            if (!st) return std::unexpected(Err{Errc::io_error, 0});
            return to_read;
        }

        Result<std::int64_t> seek(std::int64_t offset, int) {
            if (!opened_) return std::unexpected(Err{Errc::bad_state, 0});
            const auto st = fs::seek(file_, static_cast<util::i64>(offset));
            if (!st) return std::unexpected(Err{Errc::io_error, 0});
            return tell();
        }

        Result<std::int64_t> tell() {
            if (!opened_) return std::unexpected(Err{Errc::bad_state, 0});
            return static_cast<std::int64_t>(file_.node.offset);
        }

        Result<std::int64_t> size() {
            if (!opened_) return std::unexpected(Err{Errc::bad_state, 0});
            return static_cast<std::int64_t>(file_.node.size);
        }

    private:
        fs::File file_{};
        bool opened_{false};
    };
}
