#include <array>
#include <cstdint>
#include <span>
#include <string_view>

import fs_core;
import fs_errno;
import fs_ramfs;
import fs_stream;
import fs_vfs;
import posix.api;
import posix.file;
import posix.fd_table;
import posix.pipe;
import posix.proc;
import util.core;
import util.error;

namespace demo {
    struct UartCmsdk {
        static constexpr std::uint32_t base = 0x40004000u;
        static constexpr std::uint32_t data = base + 0x00u;
        static constexpr std::uint32_t state = base + 0x04u;
        static constexpr std::uint32_t ctrl = base + 0x08u;
        static constexpr std::uint32_t state_txbf = 1u << 1;
        static constexpr std::uint32_t ctrl_tx_enable = 1u << 0;
        static constexpr std::uint32_t ctrl_rx_enable = 1u << 1;

        static void init() noexcept {
            auto* reg = reinterpret_cast<volatile std::uint32_t*>(ctrl);
            *reg = ctrl_tx_enable | ctrl_rx_enable;
        }

        static void write_byte(char ch) noexcept {
            auto* status = reinterpret_cast<volatile std::uint32_t*>(state);
            auto* out = reinterpret_cast<volatile std::uint32_t*>(data);
            while ((*status & state_txbf) != 0u) {
            }
            *out = static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
        }

        static void write(const char* text) noexcept {
            static bool inited = false;
            if (!inited) {
                init();
                inited = true;
            }
            if (!text) return;
            while (*text) {
                write_byte(*text++);
            }
        }
    };

    inline void log_line(const char* msg) noexcept {
        UartCmsdk::write(msg);
        UartCmsdk::write("\n");
    }

    template <util::usize BlockSize, util::usize MaxFiles, util::usize MaxBlocks>
    struct RamFsMount {
        fs::RamFs<BlockSize, MaxFiles, MaxBlocks> fs{};
        fs::Mount mount{};

        RamFsMount() noexcept {
            mount.ops = &ops_;
            mount.data = this;
        }

        fs::Mount* mount_point() noexcept { return &mount; }

        static fs::Status open_impl(fs::Mount* m, std::string_view path, fs::File& out, fs::OpenFlags flags) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.open(path, out, flags);
        }

        static fs::Status mkdir_impl(fs::Mount* m, std::string_view path) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.mkdir(path);
        }

        static fs::Status unlink_impl(fs::Mount* m, std::string_view path) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.unlink(path);
        }

        static fs::Status truncate_impl(fs::Mount* m, std::string_view path, util::u64 size) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.truncate(path, size);
        }

        static fs::Status rename_impl(fs::Mount* m, std::string_view from, std::string_view to) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.rename(from, to);
        }

        static fs::Status list_impl(fs::Mount* m, std::string_view path, void* ctx, fs::MountOps::ListFn fn) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.list(path, ctx, fn);
        }

        static fs::MountOps ops_;
    };

    template <util::usize BlockSize, util::usize MaxFiles, util::usize MaxBlocks>
    fs::MountOps RamFsMount<BlockSize, MaxFiles, MaxBlocks>::ops_{
        &RamFsMount::open_impl,
        nullptr,
        nullptr,
        &RamFsMount::unlink_impl,
        &RamFsMount::rename_impl,
        &RamFsMount::truncate_impl,
        &RamFsMount::mkdir_impl,
        &RamFsMount::list_impl
    };

    struct Smoke {
        posix::FdTable<16> fds{};
        posix::FileService<16> files{};
        posix::PipeService<8, 64> pipes{};
        posix::ProcService<4, 4, 16, 16> procs{};
        posix::Api<16, 8, 64, 4, 4, 16> api;

        Smoke() : api(fds, files, pipes, procs) {
            fds.init();
            files.init();
            pipes.init();
            procs.init();
        }

        bool write_file(const char* path, std::string_view text) noexcept {
            int fd = api.open(path, posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
            if (fd < 0) return false;
            auto w = api.write(fd, text.data(), text.size());
            if (w != static_cast<posix::ssize_t>(text.size())) {
                (void)api.close(fd);
                return false;
            }
            return api.close(fd) == 0;
        }

        bool read_file(const char* path, std::span<char> out, util::usize& out_size) noexcept {
            int fd = api.open(path, posix::O_RDONLY, 0);
            if (fd < 0) return false;
            auto r = api.read(fd, out.data(), out.size());
            if (r < 0) {
                (void)api.close(fd);
                return false;
            }
            out_size = static_cast<util::usize>(r);
            return api.close(fd) == 0;
        }

        bool pipe_roundtrip(std::string_view text) noexcept {
            int fds_pair[2]{-1, -1};
            if (api.pipe(fds_pair) != 0) return false;
            auto w = api.write(fds_pair[1], text.data(), text.size());
            if (w != static_cast<posix::ssize_t>(text.size())) return false;
            std::array<char, 64> buf{};
            auto r = api.read(fds_pair[0], buf.data(), buf.size());
            if (r != static_cast<posix::ssize_t>(text.size())) return false;
            return true;
        }

        bool pipeline_roundtrip(std::string_view text) noexcept {
            int p1[2]{-1, -1};
            int p2[2]{-1, -1};
            if (api.pipe(p1) != 0) return false;
            if (api.pipe(p2) != 0) return false;
            if (api.write(p1[1], text.data(), text.size()) != static_cast<posix::ssize_t>(text.size())) return false;
            std::array<char, 64> mid{};
            auto r1 = api.read(p1[0], mid.data(), mid.size());
            if (r1 != static_cast<posix::ssize_t>(text.size())) return false;
            if (api.write(p2[1], mid.data(), static_cast<util::usize>(r1)) != r1) return false;
            std::array<char, 64> out{};
            auto r2 = api.read(p2[0], out.data(), out.size());
            if (r2 != static_cast<posix::ssize_t>(text.size())) return false;
            for (util::usize i = 0; i < text.size(); ++i) {
                if (out[i] != text[i]) return false;
            }
            return true;
        }
    };

    bool run_busybox_phase2_smoke() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        if (!st) return false;

        Smoke smoke{};
        if (!smoke.write_file("/out.txt", "hello\n")) return false;
        std::array<char, 16> buf{};
        util::usize out_size = 0;
        if (!smoke.read_file("/out.txt", buf, out_size)) return false;
        if (std::string_view{buf.data(), out_size} != "hello\n") return false;
        if (!smoke.pipe_roundtrip("hello")) return false;
        if (!smoke.pipeline_roundtrip("hello")) return false;
        if (!smoke.write_file("/a.txt", "hi\n")) return false;
        if (!smoke.write_file("/b.txt", "hi\n")) return false;
        return true;
    }
}

int main() {
    if (demo::run_busybox_phase2_smoke()) {
        demo::log_line("bb2 all ok");
    } else {
        demo::log_line("bb2 failed");
    }
    while (true) {
    }
    return 0;
}
