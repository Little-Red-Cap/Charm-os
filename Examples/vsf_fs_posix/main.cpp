#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

import util.core;
import fs_core;
import fs_ramfs;
import fs_vfs;
import shell_posix;

using Posix = shell_posix::PosixApi<8>;

int main() {
    std::printf("[fs_posix] start\n");
    static fs::RamFs<64, 4, 16> ramfs;
    static fs::MountOps mops{
        .open = +[](std::string_view path, fs::File& f) noexcept { return ramfs.open(path, f); },
        .unlink = +[](fs::Mount*, std::string_view path) noexcept { return ramfs.unlink(path); },
        .rename = +[](fs::Mount*, std::string_view from, std::string_view to) noexcept { return ramfs.rename(from, to); },
        .truncate = +[](fs::Mount*, std::string_view path, util::u64 size) noexcept { return ramfs.truncate(path, size); }
    };
    static fs::Mount m{ &mops, &ramfs };
    fs::clear_mounts();
    (void)fs::add_mount("/", &m);

    int fd = Posix::open("/hello");
    std::printf("[fs_posix] open fd=%d\n", fd);
    if (fd < 0) return 1;

    const char* msg = "hello posix";
    auto w = Posix::write(fd, msg, std::strlen(msg));
    std::printf("[fs_posix] write ret=%d\n", w);
    if (w < 0) return 1;

    char buf[32]{};
    (void)Posix::lseek(fd, 0);
    auto r = Posix::read(fd, buf, std::strlen(msg));
    std::printf("[fs_posix] read ret=%d\n", r);
    Posix::close(fd);
    if (r < 0) return 1;

    std::printf("[fs_posix] %s\n", buf);
    std::printf("[fs_posix] clock=%u\n", Posix::clock_ms());
    Posix::sleep_ms(1);

    Posix::Mutex mtx{};
    (void)Posix::mutex_init(mtx);
    (void)Posix::mutex_lock(mtx);
    (void)Posix::mutex_unlock(mtx);

    Posix::Sem sem{};
    (void)Posix::sem_init(sem, 1);
    (void)Posix::sem_wait(sem);
    (void)Posix::sem_post(sem);

    Posix::Pipe pipe{};
    (void)Posix::pipe_create(pipe);
    const char* pmsg = "pipe";
    (void)Posix::pipe_write(pipe, pmsg, std::strlen(pmsg));
    char pbuf[8]{};
    (void)Posix::pipe_read(pipe, pbuf, sizeof(pbuf));
    std::printf("[fs_posix] pipe=%s\n", pbuf);
    return 0;
}
