module;

#include <cstddef>
#include <cstdint>

export module fs_mal_file;

import util.core;
import fs_stream;
import fs_errno;
import fs_block_file;
import fs_mal;

export namespace fs {
    class MalFile {
    public:
        MalFile() = default;
        MalFile(const MalFile&) = delete;
        MalFile& operator=(const MalFile&) = delete;

        ~MalFile() { close(); }

        Status open(const char* path, util::u64 block_size) noexcept {
            const auto st = file_.open(path, block_size);
            if (!st) return st;
            mal_ = make_mal_from_block(file_.device(), MalKind::file);
            return Status{Errc::ok};
        }

        void close() noexcept {
            file_.close();
            mal_ = {};
        }

        [[nodiscard]] MalDevice& device() noexcept { return mal_; }
        [[nodiscard]] const MalDevice& device() const noexcept { return mal_; }
        [[nodiscard]] BlockFile& block_file() noexcept { return file_; }

    private:
        BlockFile file_;
        MalDevice mal_{};
    };
}
