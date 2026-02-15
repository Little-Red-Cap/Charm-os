module;

#include <cstddef>
#include <cstdint>
#include <array>
#include <string_view>
#include <span>

export module shell_posix;

import util.core;
import fs_vfs;
import fs_core;
import fs_stream;
import fs_errno;
import hal_core;
import hal_time;

export namespace shell_posix {
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

    template <std::size_t MaxFd, typename Caps = hal::DefaultCaps>
    class PosixApi {
    public:
        static int open(const char* path) noexcept {
            return open(path, O_RDONLY);
        }

        static int open(const char* path, int flags) noexcept {
            if (!path) return -static_cast<int>(fs::Err::inval);
            auto idx = alloc_fd();
            if (idx < 0) return idx;
            auto st = fs::vfs_open(path, fds_[idx].file);
            if (!st) {
                if (flags & O_CREAT) {
                    st = fs::vfs_open(path, fds_[idx].file);
                }
                if (!st) {
                    fds_[idx].used = false;
                    return static_cast<int>(st.err);
                }
            }
            if (flags & O_TRUNC) {
                auto st = fs::vfs_truncate(path, 0);
                if (!st) {
                    fds_[idx].used = false;
                    return static_cast<int>(st.err);
                }
                fds_[idx].file.node.size = 0;
                fds_[idx].file.node.offset = 0;
            }
            return static_cast<int>(idx);
        }

        static int close(int fd) noexcept {
            if (!valid(fd)) return -static_cast<int>(fs::Err::inval);
            fds_[fd].used = false;
            return 0;
        }

        static int read(int fd, void* buf, std::size_t n) noexcept {
            if (!valid(fd) || !buf) return -static_cast<int>(fs::Err::inval);
            auto& file = fds_[fd].file;
            const auto before = file.node.offset;
            auto st = fs::read(file, std::span<util::u8>(reinterpret_cast<util::u8*>(buf), n));
            if (!st) return static_cast<int>(st.err);
            const auto after = file.node.offset;
            if (before >= 0 && after >= before) {
                return static_cast<int>(after - before);
            }
            return static_cast<int>(n);
        }

        static int write(int fd, const void* buf, std::size_t n) noexcept {
            if (!valid(fd) || !buf) return -static_cast<int>(fs::Err::inval);
            auto& file = fds_[fd].file;
            const auto before = file.node.offset;
            auto st = fs::write(file, std::span<const util::u8>(reinterpret_cast<const util::u8*>(buf), n));
            if (!st) return static_cast<int>(st.err);
            const auto after = file.node.offset;
            if (before >= 0 && after >= before) {
                return static_cast<int>(after - before);
            }
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
            auto st = fs::seek(file, target);
            if (!st) return static_cast<int>(st.err);
            return static_cast<int>(file.node.offset);
        }

        static std::uint32_t clock_ms() noexcept {
            return static_cast<std::uint32_t>(Caps::TimeSource::now());
        }

        static void sleep_ms(std::uint32_t ms) noexcept {
            if constexpr (hal::SleepProvider<typename Caps::SleepProvider>) {
                Caps::SleepProvider::sleep_ms(static_cast<hal::tick_t>(ms));
            } else if constexpr (hal::DelayProvider<typename Caps::DelayProvider>) {
                Caps::DelayProvider::delay_ms(static_cast<hal::tick_t>(ms));
            } else {
                (void)ms;
            }
        }

        static int unlink(const char* path) noexcept {
            if (!path) return -static_cast<int>(fs::Err::inval);
            auto st = fs::vfs_unlink(path);
            if (!st) return static_cast<int>(st.err);
            return 0;
        }

        static int rename(const char* from, const char* to) noexcept {
            if (!from || !to) return -static_cast<int>(fs::Err::inval);
            auto st = fs::vfs_rename(from, to);
            if (!st) return static_cast<int>(st.err);
            return 0;
        }

        static int truncate(const char* path, std::uint64_t size) noexcept {
            if (!path) return -static_cast<int>(fs::Err::inval);
            auto st = fs::vfs_truncate(path, size);
            if (!st) return static_cast<int>(st.err);
            return 0;
        }

        struct Mutex {
            bool locked{false};
        };

        static int mutex_init(Mutex& m) noexcept {
            m.locked = false;
            return 0;
        }

        static int mutex_lock(Mutex& m) noexcept {
            if (m.locked) return -static_cast<int>(fs::Err::busy);
            m.locked = true;
            return 0;
        }

        static int mutex_unlock(Mutex& m) noexcept {
            if (!m.locked) return -static_cast<int>(fs::Err::inval);
            m.locked = false;
            return 0;
        }

        struct Sem {
            std::int32_t count{0};
        };

        static int sem_init(Sem& s, std::int32_t v) noexcept {
            s.count = v;
            return 0;
        }

        static int sem_post(Sem& s) noexcept {
            ++s.count;
            return 0;
        }

        static int sem_wait(Sem& s) noexcept {
            if (s.count <= 0) return -static_cast<int>(fs::Err::again);
            --s.count;
            return 0;
        }

        struct Pipe {
            std::array<util::u8, 64> buf{};
            util::usize head{0};
            util::usize tail{0};
            util::usize size{0};
        };

        static int pipe_create(Pipe& p) noexcept {
            p.head = 0;
            p.tail = 0;
            p.size = 0;
            return 0;
        }

        static int pipe_write(Pipe& p, const void* data, std::size_t n) noexcept {
            const auto* in = reinterpret_cast<const util::u8*>(data);
            for (std::size_t i = 0; i < n; ++i) {
                if (p.size >= p.buf.size()) return static_cast<int>(i);
                p.buf[p.tail] = in[i];
                p.tail = (p.tail + 1) % p.buf.size();
                ++p.size;
            }
            return static_cast<int>(n);
        }

        static int pipe_read(Pipe& p, void* out, std::size_t n) noexcept {
            auto* dst = reinterpret_cast<util::u8*>(out);
            std::size_t read = 0;
            while (read < n && p.size > 0) {
                dst[read++] = p.buf[p.head];
                p.head = (p.head + 1) % p.buf.size();
                --p.size;
            }
            return static_cast<int>(read);
        }

    private:
        static bool valid(int fd) noexcept { return fd >= 0 && static_cast<std::size_t>(fd) < MaxFd && fds_[fd].used; }
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
