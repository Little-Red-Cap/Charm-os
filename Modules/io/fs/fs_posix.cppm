module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

export module fs_posix;

import util.core;
import fs_vfs;
import fs_core;
import fs_errno;

export namespace fs_posix {
    enum OpenFlags : int {
        O_RDONLY = 0x0,
        O_WRONLY = 0x1,
        O_RDWR   = 0x2,
        O_CREAT  = 0x40,
        O_TRUNC  = 0x200
    };

    enum SeekWhence : int {
        SeekSet = 0,
        SeekCur = 1,
        SeekEnd = 2
    };

    struct FileDesc {
        bool used{false};
        fs::File file{};
    };

    template <std::size_t MaxFd>
    class PosixApi {
    public:
        static int open(const char* path) noexcept {
            return open(path, O_RDONLY);
        }

        static int open(const char* path, int flags) noexcept {
            if (!path) return -static_cast<int>(fs::Err::inval);
            auto idx = alloc_fd();
            if (idx < 0) return idx;
            fs::OpenFlags oflags = fs::OpenFlags::read;
            if ((flags & O_WRONLY) == O_WRONLY) {
                oflags = fs::OpenFlags::write;
            } else if ((flags & O_RDWR) == O_RDWR) {
                oflags = static_cast<fs::OpenFlags>(
                    static_cast<util::u32>(fs::OpenFlags::read) |
                    static_cast<util::u32>(fs::OpenFlags::write));
            }
            if (flags & O_CREAT) {
                oflags = static_cast<fs::OpenFlags>(
                    static_cast<util::u32>(oflags) |
                    static_cast<util::u32>(fs::OpenFlags::create));
            }
            if (flags & O_TRUNC) {
                oflags = static_cast<fs::OpenFlags>(
                    static_cast<util::u32>(oflags) |
                    static_cast<util::u32>(fs::OpenFlags::trunc));
            }

            auto st = fs::vfs_open(path, fds_[idx].file, oflags);
            if (!st) {
                fds_[idx].used = false;
                return static_cast<int>(st.err);
            }
            if (flags & O_TRUNC) {
                auto st2 = fs::vfs_truncate(path, 0);
                if (!st2) {
                    fds_[idx].used = false;
                    return static_cast<int>(st2.err);
                }
                fds_[idx].file.node.size = 0;
                fds_[idx].file.node.offset = 0;
            }
            return static_cast<int>(idx);
        }

        static int close(int fd) noexcept {
            if (!valid(fd)) return -static_cast<int>(fs::Err::inval);
            auto st = fs::vfs_close(fds_[fd].file);
            fds_[fd].used = false;
            return st ? 0 : -static_cast<int>(st.err);
        }

        static int read(int fd, void* buf, std::size_t n) noexcept {
            if (!valid(fd) || !buf) return -static_cast<int>(fs::Err::inval);
            auto& file = fds_[fd].file;
            const auto before = file.node.offset;
            auto st = fs::vfs_read(file, std::span<util::u8>(reinterpret_cast<util::u8*>(buf), n));
            if (!st) return -static_cast<int>(st.err);
            const auto after = file.node.offset;
            if (before >= 0 && after >= before) return static_cast<int>(after - before);
            return static_cast<int>(n);
        }

        static int write(int fd, const void* buf, std::size_t n) noexcept {
            if (!valid(fd) || !buf) return -static_cast<int>(fs::Err::inval);
            auto& file = fds_[fd].file;
            const auto before = file.node.offset;
            auto st = fs::vfs_write(file, std::span<const util::u8>(reinterpret_cast<const util::u8*>(buf), n));
            if (!st) return -static_cast<int>(st.err);
            const auto after = file.node.offset;
            if (before >= 0 && after >= before) return static_cast<int>(after - before);
            return static_cast<int>(n);
        }

        static int lseek(int fd, std::int64_t off) noexcept {
            return lseek(fd, off, SeekSet);
        }

        static int lseek(int fd, std::int64_t off, int whence) noexcept {
            if (!valid(fd)) return -static_cast<int>(fs::Err::inval);
            auto& file = fds_[fd].file;
            std::int64_t base = 0;
            switch (whence) {
            case SeekSet:
                base = 0;
                break;
            case SeekCur:
                base = file.node.offset;
                break;
            case SeekEnd:
                base = file.node.size;
                break;
            default:
                return -static_cast<int>(fs::Err::inval);
            }
            const auto target = base + off;
            if (target < 0) return -static_cast<int>(fs::Err::inval);
            auto st = fs::vfs_seek(file, target);
            if (!st) return -static_cast<int>(st.err);
            return static_cast<int>(file.node.offset);
        }

        static int unlink(const char* path) noexcept {
            if (!path) return -static_cast<int>(fs::Err::inval);
            auto st = fs::vfs_unlink(path);
            return st ? 0 : -static_cast<int>(st.err);
        }

        static int rename(const char* from, const char* to) noexcept {
            if (!from || !to) return -static_cast<int>(fs::Err::inval);
            auto st = fs::vfs_rename(from, to);
            return st ? 0 : -static_cast<int>(st.err);
        }

        static int truncate(const char* path, std::uint64_t size) noexcept {
            if (!path) return -static_cast<int>(fs::Err::inval);
            auto st = fs::vfs_truncate(path, size);
            return st ? 0 : -static_cast<int>(st.err);
        }

    private:
        static bool valid(int fd) noexcept {
            return fd >= 0 && static_cast<std::size_t>(fd) < MaxFd && fds_[fd].used;
        }

        static int alloc_fd() noexcept {
            for (std::size_t i = 0; i < MaxFd; ++i) {
                if (!fds_[i].used) {
                    fds_[i].used = true;
                    return static_cast<int>(i);
                }
            }
            return -static_cast<int>(fs::Err::busy);
        }

        static inline std::array<FileDesc, MaxFd> fds_{};
    };
}
