#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

import charm.foundation;
import charm.runtime;

struct MemBlock {
    static constexpr util::usize block_size = 128;
    static constexpr util::usize block_count = 8;
    std::array<std::byte, block_size * block_count> storage{};

    fs::Status read(util::u64 lba, std::span<util::u8> out) noexcept {
        if (lba >= block_count || out.size() != block_size) return fs::Status{fs::Errc::inval};
        std::memcpy(out.data(), storage.data() + lba * block_size, block_size);
        return fs::Status{fs::Errc::ok};
    }

    fs::Status write(util::u64 lba, std::span<const util::u8> in) noexcept {
        if (lba >= block_count || in.size() != block_size) return fs::Status{fs::Errc::inval};
        std::memcpy(storage.data() + lba * block_size, in.data(), block_size);
        return fs::Status{fs::Errc::ok};
    }

    fs::Status erase(util::u64 lba, util::u64 count) noexcept {
        if (lba + count > block_count) return fs::Status{fs::Errc::inval};
        std::memset(storage.data() + lba * block_size, 0, block_size * count);
        return fs::Status{fs::Errc::ok};
    }
};

static MemBlock* g_mem = nullptr;
static fs::BlockFs<MemBlock::block_size, 4, MemBlock::block_count>* g_bfs = nullptr;

static fs::Status dev_read(util::u64 lba, std::span<util::u8> out) noexcept {
    return g_mem ? g_mem->read(lba, out) : fs::Status{fs::Errc::nosys};
}

static fs::Status dev_write(util::u64 lba, std::span<const util::u8> in) noexcept {
    return g_mem ? g_mem->write(lba, in) : fs::Status{fs::Errc::nosys};
}

static fs::Status dev_erase(util::u64 lba, util::u64 count) noexcept {
    return g_mem ? g_mem->erase(lba, count) : fs::Status{fs::Errc::nosys};
}

static fs::Status dev_flush() noexcept {
    return fs::Status{fs::Errc::ok};
}

int main() {
    MemBlock mem{};
    g_mem = &mem;

    fs::BlockDevice dev{
        .read = &dev_read,
        .write = &dev_write,
        .erase = &dev_erase,
        .flush = &dev_flush,
        .block_size = MemBlock::block_size,
        .block_count = MemBlock::block_count
    };

    fs::BlockFs<MemBlock::block_size, 4, MemBlock::block_count> bfs(dev);
    g_bfs = &bfs;
    if (!g_bfs->mount()) {
        (void)g_bfs->format();
        (void)g_bfs->mount();
    }

    static fs::MountOps mops{
        .open = +[](fs::Mount*, std::string_view path, fs::File& f) noexcept {
            return g_bfs ? g_bfs->open(path, f) : fs::Status{fs::Errc::nosys};
        },
        .flush = +[](fs::Mount* m) noexcept {
            auto* bfs = m ? static_cast<fs::BlockFs<MemBlock::block_size, 4, MemBlock::block_count>*>(m->data) : nullptr;
            return bfs ? bfs->flush() : fs::Status{fs::Errc::nosys};
        },
        .unmount = +[](fs::Mount* m, bool force) noexcept {
            auto* bfs = m ? static_cast<fs::BlockFs<MemBlock::block_size, 4, MemBlock::block_count>*>(m->data) : nullptr;
            return bfs ? bfs->unmount(force) : fs::Status{fs::Errc::nosys};
        },
        .unlink = +[](fs::Mount* m, std::string_view path) noexcept {
            auto* bfs = m ? static_cast<fs::BlockFs<MemBlock::block_size, 4, MemBlock::block_count>*>(m->data) : nullptr;
            return bfs ? bfs->unlink(path) : fs::Status{fs::Errc::nosys};
        },
        .rename = +[](fs::Mount* m, std::string_view from, std::string_view to) noexcept {
            auto* bfs = m ? static_cast<fs::BlockFs<MemBlock::block_size, 4, MemBlock::block_count>*>(m->data) : nullptr;
            return bfs ? bfs->rename(from, to) : fs::Status{fs::Errc::nosys};
        },
        .truncate = +[](fs::Mount* m, std::string_view path, util::u64 size) noexcept {
            auto* bfs = m ? static_cast<fs::BlockFs<MemBlock::block_size, 4, MemBlock::block_count>*>(m->data) : nullptr;
            return bfs ? bfs->truncate(path, size) : fs::Status{fs::Errc::nosys};
        }
    };
    static fs::Mount m{ &mops, g_bfs };
    fs::set_mount(&m);

    using Posix = shell_posix::PosixApi<4>;
    int fd = Posix::open("/hello.txt");
    const char* msg = "blockfs";
    (void)Posix::write(fd, msg, std::strlen(msg));
    (void)Posix::lseek(fd, 0);
    char buf[16]{};
    (void)Posix::read(fd, buf, std::strlen(msg));
    Posix::close(fd);
    std::printf("[blockfs] %s\n", buf);

    auto st = fs::vfs_flush("/");
    std::printf("[blockfs] flush=%d\n", static_cast<int>(st.err));
    st = fs::vfs_unmount("/", false);
    std::printf("[blockfs] unmount=%d\n", static_cast<int>(st.err));

    return 0;
}
