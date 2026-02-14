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
    static fs::MountOps mops{ .open = +[](std::string_view path, fs::File& f) noexcept { return ramfs.open(path, f); } };
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
    return 0;
}
