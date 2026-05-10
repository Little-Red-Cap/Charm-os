module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#if !defined(_WIN32)
#include <sys/types.h>
#endif

export module fs_block_file;

import util.core;
import fs_errno;
import fs_stream;
import fs_block;

namespace {
#if defined(_MSC_VER)
    std::FILE* open_file_read_write(const char* path) noexcept {
        std::FILE* file = nullptr;
        if (fopen_s(&file, path, "r+b") == 0) {
            return file;
        }
        if (fopen_s(&file, path, "w+b") == 0) {
            return file;
        }
        return nullptr;
    }
#else
    std::FILE* open_file_read_write(const char* path) noexcept {
        if (std::FILE* file = std::fopen(path, "r+b")) {
            return file;
        }
        return std::fopen(path, "w+b");
    }
#endif
}

export namespace fs {
    class BlockFile {
    public:
        BlockFile() noexcept = default;
        BlockFile(const BlockFile&) = delete;
        BlockFile& operator=(const BlockFile&) = delete;

        ~BlockFile() { close(); }

        Status open(const char* path, util::u64 block_size) noexcept {
            if (!path || !*path || block_size == 0) return Status{Errc::inval};
            close();
            file_ = open_file_read_write(path);
            if (!file_) return Status{Errc::io};
            block_size_ = block_size;
            if (!refresh_size()) {
                close();
                return Status{Errc::inval};
            }
            device_.ctx = this;
            device_.read = &BlockFile::read_impl;
            device_.write = &BlockFile::write_impl;
            device_.erase = &BlockFile::erase_impl;
            device_.flush = &BlockFile::flush_impl;
            device_.block_size = block_size_;
            device_.block_count = block_count_;
            return Status{Errc::ok};
        }

        void close() noexcept {
            if (file_) {
                std::fclose(file_);
                file_ = nullptr;
            }
            block_size_ = 0;
            block_count_ = 0;
            device_ = {};
        }

        [[nodiscard]] BlockDevice& device() noexcept { return device_; }
        [[nodiscard]] const BlockDevice& device() const noexcept { return device_; }

        [[nodiscard]] util::u64 block_size() const noexcept { return block_size_; }
        [[nodiscard]] util::u64 block_count() const noexcept { return block_count_; }

    private:
        static bool seek64(std::FILE* file, util::u64 offset) noexcept {
            if (!file) return false;
#if defined(_WIN32)
            return _fseeki64(file, static_cast<__int64>(offset), SEEK_SET) == 0;
#elif defined(__NEWLIB__) || defined(__ARM_EABI__)
            return std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0;
#else
            return std::fseeko(file, static_cast<off_t>(offset), SEEK_SET) == 0;
#endif
        }

        static util::u64 tell64(std::FILE* file) noexcept {
            if (!file) return 0;
#if defined(_WIN32)
            const auto pos = _ftelli64(file);
#elif defined(__NEWLIB__) || defined(__ARM_EABI__)
            const auto pos = std::ftell(file);
#else
            const auto pos = std::ftello(file);
#endif
            return pos < 0 ? 0 : static_cast<util::u64>(pos);
        }

        bool refresh_size() noexcept {
            if (!file_ || block_size_ == 0) return false;
#if defined(_WIN32)
            if (_fseeki64(file_, 0, SEEK_END) != 0) return false;
#elif defined(__NEWLIB__) || defined(__ARM_EABI__)
            if (std::fseek(file_, 0, SEEK_END) != 0) return false;
#else
            if (std::fseeko(file_, 0, SEEK_END) != 0) return false;
#endif
            const util::u64 usize = tell64(file_);
#if defined(_WIN32)
            if (_fseeki64(file_, 0, SEEK_SET) != 0) return false;
#elif defined(__NEWLIB__) || defined(__ARM_EABI__)
            if (std::fseek(file_, 0, SEEK_SET) != 0) return false;
#else
            if (std::fseeko(file_, 0, SEEK_SET) != 0) return false;
#endif
            if (usize % block_size_ != 0) return false;
            block_count_ = usize / block_size_;
            return true;
        }

        static Status read_impl(void* ctx, util::u64 lba, std::span<util::u8> out) noexcept {
            auto* self = static_cast<BlockFile*>(ctx);
            if (!self || !self->file_) return Status{Errc::io};
            if (self->block_size_ == 0) return Status{Errc::inval};
            if ((out.size() % self->block_size_) != 0) return Status{Errc::inval};
            const util::u64 blocks = out.size() / self->block_size_;
            if (lba + blocks > self->block_count_) return Status{Errc::inval};
            const util::u64 offset = lba * self->block_size_;
            if (!seek64(self->file_, offset)) return Status{Errc::io};
            const std::size_t read = std::fread(out.data(), 1, out.size(), self->file_);
            if (read != out.size()) return Status{Errc::io};
            return Status{Errc::ok};
        }

        static Status write_impl(void* ctx, util::u64 lba, std::span<const util::u8> in) noexcept {
            auto* self = static_cast<BlockFile*>(ctx);
            if (!self || !self->file_) return Status{Errc::io};
            if (self->block_size_ == 0) return Status{Errc::inval};
            if ((in.size() % self->block_size_) != 0) return Status{Errc::inval};
            const util::u64 blocks = in.size() / self->block_size_;
            if (lba + blocks > self->block_count_) return Status{Errc::inval};
            const util::u64 offset = lba * self->block_size_;
            if (!seek64(self->file_, offset)) return Status{Errc::io};
            const std::size_t written = std::fwrite(in.data(), 1, in.size(), self->file_);
            if (written != in.size()) return Status{Errc::io};
            return Status{Errc::ok};
        }

        static Status erase_impl(void* ctx, util::u64 lba, util::u64 count) noexcept {
            auto* self = static_cast<BlockFile*>(ctx);
            if (!self || !self->file_) return Status{Errc::io};
            if (self->block_size_ == 0) return Status{Errc::inval};
            if (lba + count > self->block_count_) return Status{Errc::inval};
            const util::u64 offset = lba * self->block_size_;
            if (!seek64(self->file_, offset)) return Status{Errc::io};
            std::array<util::u8, 512> zeros{};
            util::u64 remaining = count * self->block_size_;
            while (remaining > 0) {
                const std::size_t chunk = static_cast<std::size_t>(
                    remaining > zeros.size() ? zeros.size() : remaining);
                if (std::fwrite(zeros.data(), 1, chunk, self->file_) != chunk) return Status{Errc::io};
                remaining -= chunk;
            }
            return Status{Errc::ok};
        }

        static Status flush_impl(void* ctx) noexcept {
            auto* self = static_cast<BlockFile*>(ctx);
            if (!self || !self->file_) return Status{Errc::io};
            if (std::fflush(self->file_) != 0) return Status{Errc::io};
            return Status{Errc::ok};
        }

        std::FILE* file_{nullptr};
        util::u64 block_size_{0};
        util::u64 block_count_{0};
        BlockDevice device_{};
    };
} // namespace fs
