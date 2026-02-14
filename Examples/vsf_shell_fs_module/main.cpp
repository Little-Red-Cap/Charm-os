#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

import util.core;
import fs_core;
import fs_ramfs;
import fs_vfs;
import shell_core;
import shell_cmd;
import shell_posix;
import shell_stdio;
import module_core;
import module_loader;
import module_link;
import module_registry;

using Posix = shell_posix::PosixApi<8>;

static util::usize console_write(void*, shell::Buffer buf) noexcept {
    return std::fwrite(buf.data, 1, buf.size, stdout);
}

static void print_num(shell::Console& con, const char* prefix, long long value) noexcept {
    char buf[96]{};
    std::snprintf(buf, sizeof(buf), "%s%lld\n", prefix, value);
    (void)shell::write(con, buf);
}

static const char* to_cstr(std::string_view sv, char* buf, std::size_t cap) noexcept {
    if (!buf || cap == 0) return "";
    const auto n = (sv.size() + 1 < cap) ? sv.size() : (cap - 1);
    std::memcpy(buf, sv.data(), n);
    buf[n] = '\0';
    return buf;
}

struct DemoModule {
    struct DemoImage {
        modulex::ImageHeader hdr{};
        std::array<std::byte, 16> text{};
        util::usize data_entry{0};
        util::usize data_ext{0};
        util::u32 rel32_slot{0};
        modulex::Reloc rels[3]{};
        modulex::Symbol syms[2]{};
        char strtab[24]{};
        modulex::Dependency deps[1]{};
    };

    DemoImage image{};
    modulex::Registry<4> reg{};

    static void external_func() {}

    static bool resolve_symbol(std::string_view name, util::usize& out) noexcept {
        if (name == "ext_fn") {
            out = reinterpret_cast<util::usize>(&external_func);
            return true;
        }
        return false;
    }

    static bool resolve_dep_ctx(void* ctx, std::string_view name, std::string_view version) noexcept {
        auto* reg = static_cast<modulex::Registry<4>*>(ctx);
        if (!reg) return false;
        auto* info = reg->find(name);
        if (!info) return false;
        return modulex::match_version(version, info->version);
    }

    DemoModule() {
        auto& img = image;
        img.hdr.magic = modulex::k_magic;
        img.hdr.version = modulex::k_version;
        img.hdr.text_offset = static_cast<util::u32>(offsetof(DemoImage, text));
        img.hdr.data_offset = static_cast<util::u32>(offsetof(DemoImage, data_entry));
        img.hdr.rel_offset = static_cast<util::u32>(offsetof(DemoImage, rels));
        img.hdr.sym_offset = static_cast<util::u32>(offsetof(DemoImage, syms));
        img.hdr.text_size = static_cast<util::u32>(img.text.size());
        img.hdr.data_size = static_cast<util::u32>(sizeof(img.data_entry) + sizeof(img.data_ext) + sizeof(img.rel32_slot));
        img.hdr.rel_size = static_cast<util::u32>(sizeof(img.rels));
        img.hdr.sym_size = static_cast<util::u32>(sizeof(img.syms));
        img.hdr.entry_offset = 0;
        img.hdr.str_offset = static_cast<util::u32>(offsetof(DemoImage, strtab));
        img.hdr.str_size = static_cast<util::u32>(sizeof(img.strtab));
        img.hdr.dep_offset = static_cast<util::u32>(offsetof(DemoImage, deps));
        img.hdr.dep_size = static_cast<util::u32>(sizeof(img.deps));

        img.rels[0].offset = static_cast<util::u32>(offsetof(DemoImage, data_entry));
        img.rels[0].type = modulex::RelocType::abs_addr;
        img.rels[0].sym_index = 0;

        img.rels[1].offset = static_cast<util::u32>(offsetof(DemoImage, data_ext));
        img.rels[1].type = modulex::RelocType::abs_addr;
        img.rels[1].sym_index = 1;

        img.rels[2].offset = static_cast<util::u32>(offsetof(DemoImage, rel32_slot));
        img.rels[2].type = modulex::RelocType::rel32;
        img.rels[2].sym_index = 0;

        img.syms[0].name_offset = 0;
        img.syms[0].value = static_cast<util::usize>(img.hdr.entry_offset);
        img.syms[0].kind = modulex::SymbolKind::global;

        img.syms[1].name_offset = 6;
        img.syms[1].kind = modulex::SymbolKind::external;

        std::memcpy(img.strtab, "entry\0ext_fn\0host\0^1\0", 22);
        img.deps[0].name_offset = 13;
        img.deps[0].version_offset = 18;

        (void)reg.add(modulex::ModuleInfo{"host", "v1", 0});
    }

    void info(shell::Console& con) noexcept {
        auto* hdr = &image.hdr;
        auto loaded = modulex::Loader::load(hdr);
        const auto base = reinterpret_cast<util::usize>(&image);
        auto dep_status = modulex::Linker::validate_deps_ctx(hdr, base, &resolve_dep_ctx, &reg);
        (void)modulex::Linker::bind_externals(hdr, base, &resolve_symbol);
        auto linked = modulex::Linker::relocate(hdr, base);

        print_num(con, "[module] load=", loaded.ok ? 1 : 0);
        print_num(con, "[module] deps=", dep_status.ok ? 1 : 0);
        print_num(con, "[module] dep_fail=", static_cast<long long>(dep_status.failed_index));
        print_num(con, "[module] dep_err=", static_cast<long long>(dep_status.error));
        print_num(con, "[module] link=", linked ? 1 : 0);
        print_num(con, "[module] entry=", static_cast<long long>(loaded.entry));
    }
};

static DemoModule g_mod{};

static shell::Result cmd_write(shell::Console& con, int argc, std::span<std::string_view> argv) noexcept {
    if (argc < 3) return shell::err(shell::Errno::inval);
    const auto path = argv[1];
    char msg[128]{};
    std::size_t len = 0;
    for (int i = 2; i < argc; ++i) {
        const auto part = argv[static_cast<std::size_t>(i)];
        const auto copy = (len + part.size() + 1 < sizeof(msg)) ? part.size() : (sizeof(msg) - len - 1);
        std::memcpy(msg + len, part.data(), copy);
        len += copy;
        if (i + 1 < argc && len + 1 < sizeof(msg)) {
            msg[len++] = ' ';
        }
    }
    msg[len] = '\0';
    char path_buf[96]{};
    int fd = Posix::open(to_cstr(path, path_buf, sizeof(path_buf)), shell_posix::O_CREAT | shell_posix::O_TRUNC);
    if (fd < 0) return shell::err(shell::Errno::io);
    (void)Posix::write(fd, msg, len);
    Posix::close(fd);
    return shell::ok();
}

static shell::Result cmd_cat(shell::Console& con, int argc, std::span<std::string_view> argv) noexcept {
    if (argc < 2) return shell::err(shell::Errno::inval);
    char path_buf[96]{};
    int fd = Posix::open(to_cstr(argv[1], path_buf, sizeof(path_buf)));
    if (fd < 0) return shell::err(shell::Errno::noent);
    char buf[128]{};
    int n = Posix::read(fd, buf, sizeof(buf) - 1);
    Posix::close(fd);
    if (n < 0) return shell::err(shell::Errno::io);
    buf[n] = '\0';
    (void)shell::write(con, buf);
    (void)shell::write(con, "\n");
    return shell::ok();
}

static shell::Result cmd_rm(shell::Console&, int argc, std::span<std::string_view> argv) noexcept {
    if (argc < 2) return shell::err(shell::Errno::inval);
    char path_buf[96]{};
    const int rc = Posix::unlink(to_cstr(argv[1], path_buf, sizeof(path_buf)));
    return rc == 0 ? shell::ok() : shell::err(shell::Errno::io);
}

static shell::Result cmd_mv(shell::Console&, int argc, std::span<std::string_view> argv) noexcept {
    if (argc < 3) return shell::err(shell::Errno::inval);
    char from_buf[96]{};
    char to_buf[96]{};
    const int rc = Posix::rename(to_cstr(argv[1], from_buf, sizeof(from_buf)),
                                 to_cstr(argv[2], to_buf, sizeof(to_buf)));
    return rc == 0 ? shell::ok() : shell::err(shell::Errno::io);
}

static shell::Result cmd_trunc(shell::Console&, int argc, std::span<std::string_view> argv) noexcept {
    if (argc < 3) return shell::err(shell::Errno::inval);
    const auto sz = static_cast<util::u64>(std::strtoull(argv[2].data(), nullptr, 10));
    char path_buf[96]{};
    const int rc = Posix::truncate(to_cstr(argv[1], path_buf, sizeof(path_buf)), sz);
    return rc == 0 ? shell::ok() : shell::err(shell::Errno::io);
}

static shell::Result cmd_modinfo(shell::Console& con, int, std::span<std::string_view>) noexcept {
    g_mod.info(con);
    return shell::ok();
}

static const std::array<shell::Command, 5> fs_cmds{{
    {"write", &cmd_write, "write <path> <text>"},
    {"cat", &cmd_cat, "cat <path>"},
    {"rm", &cmd_rm, "rm <path>"},
    {"mv", &cmd_mv, "mv <from> <to>"},
    {"trunc", &cmd_trunc, "trunc <path> <size>"},
}};

static const std::array<shell::Command, 1> mod_cmds{{
    {"info", &cmd_modinfo, "module demo info"},
}};

static const std::array<shell::Command, 3> cmds{{
    {"fs", nullptr, "filesystem commands", std::span<const shell::Command>(fs_cmds.data(), fs_cmds.size())},
    {"mod", nullptr, "module commands", std::span<const shell::Command>(mod_cmds.data(), mod_cmds.size())},
    {"help",
     +[](shell::Console& c, int, std::span<std::string_view>) noexcept {
         return shell::emit_help(c, std::span<const shell::Command>(cmds.data(), cmds.size()));
     },
     "show help"},
}};

int main() {
    static fs::RamFs<64, 8, 32> ramfs;
    static fs::MountOps mops{
        .open = +[](std::string_view path, fs::File& f) noexcept { return ramfs.open(path, f); },
        .unlink = +[](fs::Mount*, std::string_view path) noexcept { return ramfs.unlink(path); },
        .rename = +[](fs::Mount*, std::string_view from, std::string_view to) noexcept { return ramfs.rename(from, to); },
        .truncate = +[](fs::Mount*, std::string_view path, util::u64 size) noexcept { return ramfs.truncate(path, size); }
    };
    static fs::Mount m{ &mops, &ramfs };
    fs::clear_mounts();
    (void)fs::add_mount("/", &m);

    shell::Console con = shell::make_console(&console_write);
    (void)shell::write(con, "[shell] fs+module demo\n");
    (void)shell::run_line<8>(con, cmds, "help");
    (void)shell::run_line<8>(con, cmds, "fs write /demo.txt \"hello modulex\"");
    (void)shell::run_line<8>(con, cmds, "fs cat /demo.txt");
    (void)shell::run_line<8>(con, cmds, "fs trunc /demo.txt 5");
    (void)shell::run_line<8>(con, cmds, "fs cat /demo.txt");
    (void)shell::run_line<8>(con, cmds, "fs mv /demo.txt /moved.txt");
    (void)shell::run_line<8>(con, cmds, "fs cat /moved.txt");
    (void)shell::run_line<8>(con, cmds, "fs rm /moved.txt");
    (void)shell::run_line<8>(con, cmds, "mod info");
    return 0;
}
