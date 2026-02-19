#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

import charm.foundation;
import charm.runtime;

int main() {
    fs::RamFs<64, 4, 16> ramfs;
    fs::File f{};
    (void)ramfs.mkdir("/demo");
    (void)ramfs.open("/demo/hello.txt", f);

    const char* msg = "hello world";
    std::span<const util::u8> bytes{reinterpret_cast<const util::u8*>(msg), std::strlen(msg)};
    (void)fs::write(f, bytes);

    (void)fs::seek(f, 0);
    std::array<util::u8, 32> buf{};
    (void)fs::read(f, std::span<util::u8>(buf.data(), buf.size()));
    buf[std::strlen(msg)] = 0;

    std::printf("[fs_demo] %s\n", reinterpret_cast<char*>(buf.data()));
    return 0;
}
