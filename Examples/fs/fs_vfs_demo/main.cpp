#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

import charm.core;
import fs_core;
import fs_ramfs;
import fs_vfs;

static void write_and_read(std::string_view path, const char* msg) {
    fs::File f{};
    auto st = fs::vfs_open(path, f);
    if (!st) {
        std::printf("[vfs_demo] open failed path=%.*s err=%d\n",
                    static_cast<int>(path.size()), path.data(), static_cast<int>(st.err));
        return;
    }
    std::span<const util::u8> bytes{reinterpret_cast<const util::u8*>(msg), std::strlen(msg)};
    (void)fs::write(f, bytes);
    (void)fs::seek(f, 0);
    std::array<util::u8, 32> buf{};
    (void)fs::read(f, std::span<util::u8>(buf.data(), buf.size()));
    buf[std::strlen(msg)] = 0;
    std::printf("[vfs_demo] %.*s -> %s\n",
                static_cast<int>(path.size()), path.data(),
                reinterpret_cast<char*>(buf.data()));
}

int main() {
    static fs::RamFs<64, 4, 16> root_fs;
    static fs::RamFs<64, 4, 16> tmp_fs;
    static fs::RamFs<64, 4, 16> data_fs;
    static fs::RamFs<64, 4, 16> logs_fs;

    static fs::MountOps root_ops{ .open = +[](fs::Mount*, std::string_view path, fs::File& f, fs::OpenFlags flags) noexcept { return root_fs.open(path, f, flags); } };
    static fs::MountOps tmp_ops{ .open = +[](fs::Mount*, std::string_view path, fs::File& f, fs::OpenFlags flags) noexcept { return tmp_fs.open(path, f, flags); } };
    static fs::MountOps data_ops{ .open = +[](fs::Mount*, std::string_view path, fs::File& f, fs::OpenFlags flags) noexcept { return data_fs.open(path, f, flags); } };
    static fs::MountOps logs_ops{ .open = +[](fs::Mount*, std::string_view path, fs::File& f, fs::OpenFlags flags) noexcept { return logs_fs.open(path, f, flags); } };

    static fs::Mount root_mount{ &root_ops, &root_fs };
    static fs::Mount tmp_mount{ &tmp_ops, &tmp_fs };
    static fs::Mount data_mount{ &data_ops, &data_fs };
    static fs::Mount logs_mount{ &logs_ops, &logs_fs };

    fs::clear_mounts();
    (void)fs::add_mount("/", &root_mount);
    (void)fs::add_mount("/tmp", &tmp_mount);
    (void)fs::add_mount("/data", &data_mount);
    (void)fs::add_mount("/data/logs", &logs_mount);

    write_and_read("/tmp/hello.txt", "tmp");
    write_and_read("/data/hello.txt", "data");
    write_and_read("/data/logs/system.log", "logs");
    write_and_read("/root.txt", "root");

    std::printf("[vfs_demo] mounts=%zu\n", fs::mount_count());
    return 0;
}
