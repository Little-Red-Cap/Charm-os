module;

#include <cstddef>
#include <cstdint>

export module block.file;

import fs_block_file;
import block.device;
import util.core;

export namespace block {
    // File-backed block device for Win/host validation.
    class FileDevice {
    public:
        [[nodiscard]] Status open(const char* path, util::u64 block_size) noexcept {
            return file_.open(path, block_size);
        }

        void close() noexcept { file_.close(); }

        [[nodiscard]] Device& device() noexcept { return file_.device(); }
        [[nodiscard]] const Device& device() const noexcept { return file_.device(); }

        [[nodiscard]] util::u64 block_size() const noexcept { return file_.block_size(); }
        [[nodiscard]] util::u64 block_count() const noexcept { return file_.block_count(); }

    private:
        fs::BlockFile file_;
    };
}
