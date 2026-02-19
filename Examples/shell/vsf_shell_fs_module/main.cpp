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
import fs_stream;
import fs_vfs;
import fs_errno;
import shell_core;
import shell_cmd;
import shell_posix;
import shell_repl;
import shell_service;
import shell_stdio;
import module_core;
import module_loader;
import module_link;
import module_registry;
import module_view;

using Posix = shell_posix::PosixApi<8>;

static constexpr util::u32 kCapsFs = 0x1;
static constexpr util::u32 kCapsMod = 0x2;
static util::u32 g_caps = kCapsFs | kCapsMod;

static util::usize console_write(void*, shell::Buffer buf) noexcept {
    return std::fwrite(buf.data, 1, buf.size, stdout);
}

static void print_num(shell::Console& con, const char* prefix, long long value) noexcept {
    char buf[96]{};
    std::snprintf(buf, sizeof(buf), "%s%lld\n", prefix, value);
    (void)shell::write(con, buf);
}

static void print_text(shell::Console& con, const char* prefix, std::string_view value) noexcept {
    (void)shell::write(con, prefix);
    (void)shell::write(con, value);
    (void)shell::write(con, "\n");
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
    static void demo_entry() {
        std::printf("[mod] entry called\n");
    }

    static bool resolve_symbol(std::string_view name, util::usize& out) noexcept {
        if (name == "ext_fn") {
            out = reinterpret_cast<util::usize>(&external_func);
            return true;
        }
        if (name == "entry") {
            out = reinterpret_cast<util::usize>(&demo_entry);
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
        img.hdr.image_size = static_cast<util::u32>(sizeof(DemoImage));

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
        img.syms[0].value = 0;
        img.syms[0].kind = modulex::SymbolKind::external;

        img.syms[1].name_offset = 6;
        img.syms[1].kind = modulex::SymbolKind::external;

        std::memcpy(img.strtab, "entry\0ext_fn\0host\0^1\0", 22);
        img.deps[0].name_offset = 13;
        img.deps[0].version_offset = 18;

        (void)reg.add(modulex::ModuleInfo{"host", "v1", 0});
    }

    void info(shell::Console& con) noexcept {
        auto view = modulex::make_view(&image, sizeof(image));
        const auto val = modulex::validate(view);
        auto loaded = modulex::Loader::load(view);
        auto dep_status = modulex::Linker::validate_deps_ctx(view, &resolve_dep_ctx, &reg);
        (void)modulex::Linker::bind_externals(view, &resolve_symbol);
        auto linked = modulex::Linker::relocate(view);

        print_num(con, "[module] load=", loaded.ok ? 1 : 0);
        print_num(con, "[module] valid=", val.ok ? 1 : 0);
        print_num(con, "[module] val_err=", static_cast<long long>(val.err));
        print_num(con, "[module] deps=", dep_status.ok ? 1 : 0);
        print_num(con, "[module] dep_fail=", static_cast<long long>(dep_status.failed_index));
        print_num(con, "[module] dep_err=", static_cast<long long>(dep_status.error));
        print_num(con, "[module] link=", linked ? 1 : 0);
        print_num(con, "[module] entry=", static_cast<long long>(loaded.entry));
    }
};

static DemoModule g_mod{};
static shell_service::Alias<4> g_alias{};
static shell_service::Vars<4> g_vars{};
extern const std::array<shell::Command, 4> cmds;

static shell::Result run_line_with_alias(shell::Console& con, std::string_view line) noexcept {
    auto parsed = shell::parse_line<8>(line);
    if (parsed.count == 0) return shell::ok();
    const auto alias = g_alias.get(parsed.args[0]);
    if (alias.empty()) {
        return shell::run_lines<8>(con, cmds, line, g_caps);
    }
    char buf[160]{};
    std::size_t len = 0;
    const auto a_copy = (alias.size() < sizeof(buf) - 1) ? alias.size() : (sizeof(buf) - 1);
    std::memcpy(buf, alias.data(), a_copy);
    len += a_copy;
    for (util::usize i = 1; i < parsed.count; ++i) {
        if (len + 1 >= sizeof(buf)) break;
        buf[len++] = ' ';
        const auto part = parsed.args[i];
        const auto copy = (len + part.size() < sizeof(buf)) ? part.size() : (sizeof(buf) - len - 1);
        std::memcpy(buf + len, part.data(), copy);
        len += copy;
    }
    buf[len] = '\0';
    return shell::run_lines<8>(con, cmds, std::string_view{buf, len}, g_caps);
}

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

static shell::Result cmd_ls(shell::Console& con, int argc, std::span<std::string_view> argv) noexcept {
    bool recursive = false;
    std::string_view path = "/";
    for (int i = 1; i < argc; ++i) {
        const auto arg = argv[static_cast<std::size_t>(i)];
        if (arg == "-R" || arg == "-r") {
            recursive = true;
        } else {
            path = arg;
        }
    }

    struct Entry {
        std::string_view name{};
        fs::NodeType type{fs::NodeType::file};
        util::u64 size{0};
    };

    auto list_dir = [&](auto&& self, std::string_view dir, int depth) noexcept -> fs::Status {
        std::array<Entry, 32> entries{};
        util::usize count = 0;
        auto cb = +[](void* c, const fs::MountOps::ListEntry& entry) noexcept -> fs::Status {
            auto* ctx = static_cast<std::pair<Entry*, util::usize*>*>(c);
            if (!ctx || !ctx->first || !ctx->second) return fs::Status{fs::Err::inval};
            auto* entries = ctx->first;
            auto& idx = *ctx->second;
            if (idx >= 32) return fs::Status{fs::Err::ok};
            entries[idx++] = Entry{entry.name, entry.type, entry.size};
            return fs::Status{fs::Err::ok};
        };
        auto ctx = std::pair<Entry*, util::usize*>{entries.data(), &count};
        auto st = fs::vfs_list(dir, &ctx, cb);
        if (!st) return st;

        for (util::usize i = 0; i + 1 < count; ++i) {
            for (util::usize j = i + 1; j < count; ++j) {
                if (entries[j].name < entries[i].name) {
                    const auto tmp = entries[i];
                    entries[i] = entries[j];
                    entries[j] = tmp;
                }
            }
        }

        if (recursive) {
            (void)shell::write(con, "[ls] ");
            (void)shell::write(con, dir);
            (void)shell::write(con, "\n");
        }

        for (util::usize i = 0; i < count; ++i) {
            for (int d = 0; d < depth; ++d) {
                (void)shell::write(con, "  ");
            }
            const char* kind = (entries[i].type == fs::NodeType::dir) ? "d" : "f";
            (void)shell::write(con, kind);
            (void)shell::write(con, " ");
            (void)shell::write(con, entries[i].name);
            if (entries[i].type == fs::NodeType::file) {
                char buf[48]{};
                std::snprintf(buf, sizeof(buf), " %llu", static_cast<unsigned long long>(entries[i].size));
                (void)shell::write(con, buf);
            }
            (void)shell::write(con, "\n");
        }

        if (!recursive) return fs::Status{fs::Err::ok};

        for (util::usize i = 0; i < count; ++i) {
            if (entries[i].type != fs::NodeType::dir) continue;
            char buf[128]{};
            std::size_t len = 0;
            if (!dir.empty()) {
                const auto copy = (dir.size() < sizeof(buf) - 1) ? dir.size() : (sizeof(buf) - 1);
                std::memcpy(buf, dir.data(), copy);
                len = copy;
            }
            if (len == 0 || buf[len - 1] != '/') {
                if (len + 1 < sizeof(buf)) buf[len++] = '/';
            }
            const auto name = entries[i].name;
            const auto ncopy = (len + name.size() < sizeof(buf)) ? name.size() : (sizeof(buf) - len - 1);
            std::memcpy(buf + len, name.data(), ncopy);
            len += ncopy;
            buf[len] = '\0';
            (void)self(self, std::string_view{buf, len}, depth + 1);
        }
        return fs::Status{fs::Err::ok};
    };

    auto st = list_dir(list_dir, path, 0);
    return st ? shell::ok() : shell::err(shell::Errno::io);
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

static shell::Result cmd_cp(shell::Console&, int argc, std::span<std::string_view> argv) noexcept {
    bool recursive = false;
    std::string_view from_sv{};
    std::string_view to_sv{};
    for (int i = 1; i < argc; ++i) {
        const auto arg = argv[static_cast<std::size_t>(i)];
        if (arg == "-R" || arg == "-r") {
            recursive = true;
        } else if (from_sv.empty()) {
            from_sv = arg;
        } else if (to_sv.empty()) {
            to_sv = arg;
        }
    }
    if (from_sv.empty() || to_sv.empty()) return shell::err(shell::Errno::inval);

    auto copy_file = [](std::string_view from_sv, std::string_view to_sv) noexcept -> bool {
        char from_buf[96]{};
        char to_buf[96]{};
        const auto* from = to_cstr(from_sv, from_buf, sizeof(from_buf));
        const auto* to = to_cstr(to_sv, to_buf, sizeof(to_buf));
        const int src = Posix::open(from);
        if (src < 0) return false;
        const int dst = Posix::open(to, shell_posix::O_CREAT | shell_posix::O_TRUNC);
        if (dst < 0) {
            Posix::close(src);
            return false;
        }
        std::array<util::u8, 128> buf{};
        while (true) {
            const int n = Posix::read(src, buf.data(), buf.size());
            if (n <= 0) break;
            const int w = Posix::write(dst, buf.data(), static_cast<std::size_t>(n));
            if (w < 0) break;
        }
        Posix::close(src);
        Posix::close(dst);
        return true;
    };

    auto is_dir = [](std::string_view path) noexcept -> bool {
        util::usize count = 0;
        auto cb = +[](void* c, const fs::MountOps::ListEntry&) noexcept -> fs::Status {
            auto* cnt = static_cast<util::usize*>(c);
            if (cnt) ++(*cnt);
            return fs::Status{fs::Err::ok};
        };
        auto st = fs::vfs_list(path, &count, cb);
        return static_cast<bool>(st);
    };

    auto copy_dir = [&](auto&& self, std::string_view src, std::string_view dst) noexcept -> bool {
        (void)fs::vfs_mkdir(dst);
        std::array<fs::MountOps::ListEntry, 32> entries{};
        util::usize count = 0;
        auto cb = +[](void* c, const fs::MountOps::ListEntry& entry) noexcept -> fs::Status {
            auto* pack = static_cast<std::pair<fs::MountOps::ListEntry*, util::usize*>*>(c);
            if (!pack || !pack->first || !pack->second) return fs::Status{fs::Err::inval};
            auto* entries = pack->first;
            auto& idx = *pack->second;
            if (idx >= 32) return fs::Status{fs::Err::ok};
            entries[idx++] = entry;
            return fs::Status{fs::Err::ok};
        };
        auto ctx = std::pair<fs::MountOps::ListEntry*, util::usize*>{entries.data(), &count};
        auto st = fs::vfs_list(src, &ctx, cb);
        if (!st) return false;
        for (util::usize i = 0; i < count; ++i) {
            char src_buf[128]{};
            char dst_buf[128]{};
            std::size_t slen = 0;
            std::size_t dlen = 0;
            if (!src.empty()) {
                const auto copy = (src.size() < sizeof(src_buf) - 1) ? src.size() : (sizeof(src_buf) - 1);
                std::memcpy(src_buf, src.data(), copy);
                slen = copy;
            }
            if (!dst.empty()) {
                const auto copy = (dst.size() < sizeof(dst_buf) - 1) ? dst.size() : (sizeof(dst_buf) - 1);
                std::memcpy(dst_buf, dst.data(), copy);
                dlen = copy;
            }
            if (slen == 0 || src_buf[slen - 1] != '/') {
                if (slen + 1 < sizeof(src_buf)) src_buf[slen++] = '/';
            }
            if (dlen == 0 || dst_buf[dlen - 1] != '/') {
                if (dlen + 1 < sizeof(dst_buf)) dst_buf[dlen++] = '/';
            }
            const auto name = entries[i].name;
            const auto ncopy = (slen + name.size() < sizeof(src_buf)) ? name.size() : (sizeof(src_buf) - slen - 1);
            const auto dcopy = (dlen + name.size() < sizeof(dst_buf)) ? name.size() : (sizeof(dst_buf) - dlen - 1);
            std::memcpy(src_buf + slen, name.data(), ncopy);
            std::memcpy(dst_buf + dlen, name.data(), dcopy);
            slen += ncopy;
            dlen += dcopy;
            src_buf[slen] = '\0';
            dst_buf[dlen] = '\0';
            if (entries[i].type == fs::NodeType::dir) {
                if (!self(self, std::string_view{src_buf, slen}, std::string_view{dst_buf, dlen})) {
                    return false;
                }
            } else {
                if (!copy_file(std::string_view{src_buf, slen}, std::string_view{dst_buf, dlen})) {
                    return false;
                }
            }
        }
        return true;
    };

    if (recursive && is_dir(from_sv)) {
        return copy_dir(copy_dir, from_sv, to_sv) ? shell::ok() : shell::err(shell::Errno::io);
    }
    return copy_file(from_sv, to_sv) ? shell::ok() : shell::err(shell::Errno::io);
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

static shell::Result cmd_dirty(shell::Console& con, int, std::span<std::string_view>) noexcept {
    const bool dirty = fs::vfs_is_dirty("/");
    (void)shell::write(con, dirty ? "dirty\n" : "clean\n");
    return shell::ok();
}

static shell::Result cmd_modinfo(shell::Console& con, int, std::span<std::string_view>) noexcept {
    g_mod.info(con);
    return shell::ok();
}

static std::array<std::byte, 512> g_mod_buf{};
static util::usize g_mod_size{0};
static util::usize g_mod_entry{0};
static util::usize g_mod_entry_internal{0};
static modulex::ImageView g_mod_view{};
static bool g_mod_view_ok{false};

static util::usize resolve_entry_external(const modulex::LoadResult& loaded) noexcept {
    if (!loaded.symtab || loaded.sym_count == 0) return 0;
    for (util::u32 i = 0; i < loaded.sym_count; ++i) {
        const auto* sym = loaded.symtab + i;
        const auto view = loaded.sym_reader.at(*sym);
        if (view.name == "entry") {
            if (sym->kind == modulex::SymbolKind::external) {
                return sym->value;
            }
            return 0;
        }
    }
    return 0;
}

static bool load_module_from_fs(shell::Console& con, std::string_view path) noexcept {
    char path_buf[96]{};
    const int fd = Posix::open(to_cstr(path, path_buf, sizeof(path_buf)));
    if (fd < 0) {
        (void)shell::write(con, "[mod] open failed\n");
        return false;
    }
    const int n = Posix::read(fd, g_mod_buf.data(), g_mod_buf.size());
    Posix::close(fd);
    if (n <= 0) {
        (void)shell::write(con, "[mod] read failed\n");
        return false;
    }
    g_mod_size = static_cast<util::usize>(n);
    auto view = modulex::make_view(g_mod_buf.data(), g_mod_size);
    const auto val = modulex::validate(view);
    if (!val.ok) {
        (void)shell::write(con, "[mod] validate failed\n");
        return false;
    }
    g_mod_view = view;
    g_mod_view_ok = true;
    auto loaded = modulex::Loader::load(view);
    auto dep_status = modulex::Linker::validate_deps_ctx(view, &DemoModule::resolve_dep_ctx, &g_mod.reg);
    (void)modulex::Linker::bind_externals(view, &DemoModule::resolve_symbol);
    const bool linked = modulex::Linker::relocate(view);
    print_num(con, "[mod] load=", loaded.ok ? 1 : 0);
    print_num(con, "[mod] deps=", dep_status.ok ? 1 : 0);
    print_num(con, "[mod] link=", linked ? 1 : 0);
    g_mod_entry = resolve_entry_external(loaded);
    g_mod_entry_internal = loaded.entry;
    return loaded.ok && linked;
}

static shell::Result cmd_modload(shell::Console& con, int argc, std::span<std::string_view> argv) noexcept {
    if (argc < 2) return shell::err(shell::Errno::inval);
    const bool ok = load_module_from_fs(con, argv[1]);
    return ok ? shell::ok() : shell::err(shell::Errno::io);
}

static shell::Result cmd_modexec(shell::Console& con, int argc, std::span<std::string_view> argv) noexcept {
    if (argc >= 2) {
        if (!load_module_from_fs(con, argv[1])) return shell::err(shell::Errno::io);
    }
    if (g_mod_entry == 0) {
        if (!g_mod_view_ok || g_mod_entry_internal == 0) {
            (void)shell::write(con, "[mod] no entry\n");
            return shell::err(shell::Errno::noent);
        }
        if (!modulex::can_exec_internal(g_mod_view, g_mod_entry_internal)) {
            (void)shell::write(con, "[mod] internal entry out of text\n");
            return shell::err(shell::Errno::io);
        }
        print_num(con, "[mod] exec internal=", static_cast<long long>(g_mod_entry_internal));
        auto* fn = reinterpret_cast<void (*)()>(g_mod_entry_internal);
        fn();
        return shell::ok();
    }
    print_num(con, "[mod] exec entry=", static_cast<long long>(g_mod_entry));
    auto* fn = reinterpret_cast<void (*)()>(g_mod_entry);
    fn();
    return shell::ok();
}

static shell::Result cmd_script(shell::Console& con, int, std::span<std::string_view>) noexcept {
    static constexpr std::string_view script =
        "fs write /script.txt \"hello script\"\n"
        "fs cat /script.txt\n"
        "hi\n";
    return shell_service::run_script<&run_line_with_alias>(con, script);
}

static const std::array<shell::Command, 8> fs_cmds{{
    {"ls", &cmd_ls, "ls [path]"},
    {"write", &cmd_write, "write <path> <text>"},
    {"cat", &cmd_cat, "cat <path>"},
    {"cp", &cmd_cp, "cp <from> <to>"},
    {"rm", &cmd_rm, "rm <path>"},
    {"mv", &cmd_mv, "mv <from> <to>"},
    {"trunc", &cmd_trunc, "trunc <path> <size>"},
    {"dirty", &cmd_dirty, "show dirty state"},
}};

static const std::array<shell::Command, 3> mod_cmds{{
    {"info", &cmd_modinfo, "module demo info"},
    {"load", &cmd_modload, "load <path>"},
    {"exec", &cmd_modexec, "exec [path]"},
}};

static const std::array<shell::Command, 8> svc_cmds{{
    {"jobs", &shell_service::cmd_jobs, "list jobs"},
    {"start", &shell_service::cmd_start, "start <name>"},
    {"stop", &shell_service::cmd_stop, "stop <id>"},
    {"set", &shell_service::cmd_set<4>, "set <key> <value>"},
    {"get", &shell_service::cmd_get<4>, "get <key>"},
    {"vars", &shell_service::cmd_vars<4>, "list vars"},
    {"alias", &shell_service::cmd_alias<4>, "alias <key> <value>"},
    {"script", &cmd_script, "run demo script"},
}};

static const std::array<shell::Command, 4> cmds{{
    {"fs", nullptr, "filesystem commands", fs_cmds.data(), fs_cmds.size(), kCapsFs},
    {"mod", nullptr, "module commands", mod_cmds.data(), mod_cmds.size(), kCapsMod},
    {"svc", nullptr, "service commands", svc_cmds.data(), svc_cmds.size(), kCapsMod},
    {"help",
     +[](shell::Console& c, int, std::span<std::string_view>) noexcept {
         return shell::emit_help(c, std::span<const shell::Command>(cmds.data(), cmds.size()), g_caps);
     },
     "show help"},
}};

int main() {
    static fs::RamFs<64, 8, 32> ramfs;
    static fs::MountOps mops{
        .open = +[](fs::Mount*, std::string_view path, fs::File& f) noexcept { return ramfs.open(path, f); },
        .unlink = +[](fs::Mount*, std::string_view path) noexcept { return ramfs.unlink(path); },
        .rename = +[](fs::Mount*, std::string_view from, std::string_view to) noexcept { return ramfs.rename(from, to); },
        .truncate = +[](fs::Mount*, std::string_view path, util::u64 size) noexcept { return ramfs.truncate(path, size); },
        .mkdir = +[](fs::Mount*, std::string_view path) noexcept { return ramfs.mkdir_mount(path); },
        .list = +[](fs::Mount*, std::string_view path, void* ctx, fs::MountOps::ListFn fn) noexcept {
            return ramfs.list(path, ctx, fn);
        }
    };
    static fs::Mount m{ &mops, &ramfs };
    fs::clear_mounts();
    (void)fs::add_mount("/", &m);

    shell::Console con = shell::make_console(&console_write);
    (void)shell::write(con, "[shell] fs+module demo\n");
    {
        char path_buf[32]{};
        const int fd = Posix::open(to_cstr("/demo.mod", path_buf, sizeof(path_buf)),
                                   shell_posix::O_CREAT | shell_posix::O_TRUNC);
        if (fd >= 0) {
            (void)Posix::write(fd, &g_mod.image, sizeof(g_mod.image));
            Posix::close(fd);
        }
    }

    shell_service::JobTable<4> jobs{};
    shell_service::set_job_table(&jobs);
    shell_service::set_vars<4>(&g_vars);
    shell_service::set_alias<4>(&g_alias);

    shell::Prompt prompt{"charm> "};
    std::array<char, 32> prompt_buf{};
    (void)shell::format_prompt(prompt, std::span<char>(prompt_buf.data(), prompt_buf.size()));
    (void)shell::write(con, prompt_buf.data());

    shell::History<4> hist{};
    hist.push("fs write /demo.txt \"hello modulex\"");
    hist.push("fs cat /demo.txt");
    hist.push("mod info");
    (void)shell::write(con, "[shell] last: ");
    (void)shell::write(con, hist.last());
    (void)shell::write(con, "\n");

    const auto matches = shell::complete<4>(std::span<const shell::Command>(cmds.data(), cmds.size()), "fs c");
    for (util::usize i = 0; i < matches.count; ++i) {
        (void)shell::write(con, "[shell] suggest: fs ");
        (void)shell::write(con, matches.matches[i]);
        (void)shell::write(con, "\n");
    }
    (void)shell::run_line<8>(con, cmds, "help", g_caps);
    (void)shell::run_line<8>(con, cmds, "fs ls /", g_caps);
    (void)shell::run_line<8>(con, cmds, "fs write /demo.txt \"hello modulex\"", g_caps);
    (void)shell::run_line<8>(con, cmds, "fs cat /demo.txt", g_caps);
    (void)shell::run_line<8>(con, cmds, "fs cp /demo.txt /copy.txt", g_caps);
    (void)shell::run_line<8>(con, cmds, "fs ls /", g_caps);
    (void)shell::run_line<8>(con, cmds, "fs trunc /demo.txt 5", g_caps);
    (void)shell::run_line<8>(con, cmds, "fs cat /demo.txt", g_caps);
    (void)shell::run_line<8>(con, cmds, "fs mv /demo.txt /moved.txt", g_caps);
    (void)shell::run_line<8>(con, cmds, "fs cat /moved.txt", g_caps);
    (void)shell::run_line<8>(con, cmds, "fs rm /moved.txt", g_caps);
    (void)shell::run_line<8>(con, cmds, "mod info", g_caps);
    (void)shell::run_line<8>(con, cmds, "mod load /demo.mod", g_caps);
    (void)shell::run_line<8>(con, cmds, "mod exec /demo.mod", g_caps);
    (void)shell::run_line<8>(con, cmds, "svc start worker", g_caps);
    (void)shell::run_line<8>(con, cmds, "svc jobs", g_caps);
    (void)shell::run_line<8>(con, cmds, "svc stop 1", g_caps);
    (void)shell::run_line<8>(con, cmds, "svc jobs", g_caps);
    return 0;
}
