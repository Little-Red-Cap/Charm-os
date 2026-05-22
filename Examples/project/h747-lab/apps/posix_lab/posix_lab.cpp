#include "posix_lab.h"
#include "elf_load_region.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "console.h"
#include "console_service.hpp"

import fs_core;
import fs_errno;
import fs_ramfs;
import fs_stream;
import fs_vfs;
import io.channel;
import out.core;
import out.format;
import posix.api;
import posix.exec_source;
import posix.file;
import posix.fd_table;
import posix.pipe;
import posix.proc;
import posix.proc_types;
import posix.term;
import posix.user_runtime;
import util.core;
import util.error;

#include "append_file.elf.inc"
#include "argv_dump.elf.inc"
#include "cat_file.elf.inc"
#include "env_dump.elf.inc"
#include "exit_code.elf.inc"
#include "fd_probe.elf.inc"
#include "hello.elf.inc"
#include "stat_probe.elf.inc"
#include "stderr_demo.elf.inc"
#include "write_file.elf.inc"

namespace h747::apps::posix_lab {
namespace {

using namespace std::literals::string_view_literals;

template <charm::cap::ByteSink Sink>
class OutSinkAdapter {
public:
    explicit OutSinkAdapter(Sink& sink) : sink_(&sink) {}

    out::result<std::size_t> write(const out::bytes bytes) noexcept {
        if (sink_ == nullptr) {
            return out::ok<std::size_t>(0U);
        }
        const auto transfer = sink_->write(bytes);
        return out::ok(static_cast<std::size_t>(transfer.bytes));
    }

    out::result<std::size_t> flush() noexcept {
        if (sink_ != nullptr) {
            (void)sink_->flush();
        }
        return out::ok<std::size_t>(0U);
    }

private:
    Sink* sink_{nullptr};
};

h747::console::ConsoleStream& console_stream() noexcept {
    static h747::console::ConsoleStream stream{};
    return stream;
}

OutSinkAdapter<h747::console::ConsoleStream>& out_sink() noexcept {
    static OutSinkAdapter adapter{console_stream()};
    return adapter;
}

io::result console_channel_read(void*, io::MutByteView) noexcept {
    return io::fail(io::errc::would_block);
}

io::result console_channel_write(void*, io::ByteView buf) noexcept {
    for (const auto byte : buf) {
        h747::console::write_char(static_cast<char>(byte));
    }
    return io::ok(buf.size());
}

io::result console_channel_flush(void*) noexcept {
    return io::ok(0);
}

io::Channel& term_channel() noexcept {
    static io::Channel channel{
        nullptr,
        {
            &console_channel_read,
            &console_channel_write,
            &console_channel_flush,
        }
    };
    return channel;
}

template <out::fixed_string Fmt, class... Args>
void emit(Args&&... args) noexcept {
    out::discard(out::vprint<Fmt>(out_sink(), std::forward<Args>(args)...));
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

template <util::usize MaxProcs,
          util::usize MaxExecs,
          util::usize MaxFds,
          util::usize MaxFiles,
          util::usize MaxPathLen,
          util::usize MaxArgc,
          util::usize MaxEnvp,
          util::usize MaxArgBytes,
          util::usize MaxElfImage,
          util::usize MaxElfLoad>
class H747ProcService final
    : public posix::ProcService<MaxProcs,
                                MaxExecs,
                                MaxFds,
                                MaxFiles,
                                MaxPathLen,
                                MaxArgc,
                                MaxEnvp,
                                MaxArgBytes,
                                MaxElfImage,
                                MaxElfLoad> {
public:
    using Base = posix::ProcService<MaxProcs,
                                    MaxExecs,
                                    MaxFds,
                                    MaxFiles,
                                    MaxPathLen,
                                    MaxArgc,
                                    MaxEnvp,
                                    MaxArgBytes,
                                    MaxElfImage,
                                    MaxElfLoad>;

    void* elf_load_base_ptr() noexcept {
        return elf_load_region_base();
    }

    util::usize elf_load_capacity() const noexcept {
        return elf_load_region_capacity();
    }

    util::Result<void> apply_elf_hostcalls() noexcept {
        auto result = Base::apply_elf_hostcalls();
        if (result) {
            prepare_elf_load_region();
        }
        return result;
    }
};

struct SampleSpec {
    std::string_view name;
    const util::u8* data;
    util::usize size;
};

constexpr SampleSpec kSamples[] = {
    {"hello"sv, hello_elf, hello_elf_len},
    {"argv_dump"sv, argv_dump_elf, argv_dump_elf_len},
    {"env_dump"sv, env_dump_elf, env_dump_elf_len},
    {"stderr_demo"sv, stderr_demo_elf, stderr_demo_elf_len},
    {"exit_code"sv, exit_code_elf, exit_code_elf_len},
    {"cat_file"sv, cat_file_elf, cat_file_elf_len},
    {"write_file"sv, write_file_elf, write_file_elf_len},
    {"append_file"sv, append_file_elf, append_file_elf_len},
    {"fd_probe"sv, fd_probe_elf, fd_probe_elf_len},
    {"stat_probe"sv, stat_probe_elf, stat_probe_elf_len},
};

struct RuntimeState {
    static constexpr util::usize kMaxFds = 24;
    static constexpr util::usize kMaxPipes = 8;
    static constexpr util::usize kPipeCapacity = 128;
    static constexpr util::usize kMaxProcs = 8;
    static constexpr util::usize kMaxExecs = 16;
    static constexpr util::usize kMaxFiles = 24;
    static constexpr util::usize kMaxPath = 160;
    static constexpr util::usize kMaxElfImage = 8192;
    static constexpr util::usize kMaxElfLoad = 8192;

    posix::FdTable<kMaxFds> fd_table{};
    posix::FileService<kMaxFiles> file_service{};
    posix::PipeService<kMaxPipes, kPipeCapacity> pipe_service{};
    H747ProcService<kMaxProcs, kMaxExecs, kMaxFds, kMaxFiles, kMaxPath, 16, 16, 320, kMaxElfImage, kMaxElfLoad> proc_service{};
    posix::Api<kMaxFds, kMaxPipes, kPipeCapacity, kMaxProcs, kMaxExecs, kMaxFiles, kMaxPath, 16, 16, 320, kMaxElfImage, kMaxElfLoad> api{
        fd_table, file_service, pipe_service, proc_service
    };
    posix::user::ProcessBinding<decltype(api)> binding{api};
    posix::TermDevice term{};
    RamFsMount<128, 48, 96> ramfs{};
    console::ConsoleLineSource line_source{};
    bool ready{false};
    bool prompt_needed{true};
    bool file_backed_ready{false};
};

RuntimeState& state() noexcept {
    static RuntimeState runtime{};
    return runtime;
}

constexpr std::string_view trim_left(std::string_view sv) noexcept {
    while (!sv.empty() && sv.front() == ' ') {
        sv.remove_prefix(1);
    }
    return sv;
}

constexpr std::pair<std::string_view, std::string_view> split_token(std::string_view sv) noexcept {
    sv = trim_left(sv);
    const auto pos = sv.find(' ');
    if (pos == std::string_view::npos) {
        return {sv, {}};
    }
    return {sv.substr(0, pos), trim_left(sv.substr(pos + 1))};
}

template <util::usize MaxItems>
struct ArgList {
    std::array<const char*, MaxItems> items{};
    util::usize count{0};
};

template <util::usize MaxItems, util::usize MaxBlob>
util::Result<ArgList<MaxItems>> parse_args(std::string_view args,
                                           std::array<char, MaxBlob>& blob) noexcept {
    ArgList<MaxItems> out{};
    util::usize cursor = 0;
    blob.fill('\0');
    auto remaining = trim_left(args);
    while (!remaining.empty()) {
        if (out.count >= MaxItems) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        auto [token, rest] = split_token(remaining);
        if (token.empty()) {
            break;
        }
        if (cursor + token.size() + 1 > blob.size()) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        auto* dst = &blob[cursor];
        std::memcpy(dst, token.data(), token.size());
        dst[token.size()] = '\0';
        out.items[out.count++] = dst;
        cursor += token.size() + 1;
        remaining = rest;
    }
    return out;
}

std::array<const char*, 2> default_envp() noexcept {
    return { "PATH=/bin:/apps" , nullptr };
}

void attach_stdio(RuntimeState& runtime) noexcept {
    runtime.fd_table.init();
    runtime.file_service.init();
    runtime.pipe_service.init();
    runtime.term.channel = &term_channel();
    (void)runtime.term.attach_stdio(runtime.fd_table);
}

bool write_text_file(RuntimeState& runtime, const char* path, std::string_view text) noexcept {
    const int fd = runtime.api.open(path, posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
    if (fd < 0) {
        return false;
    }
    const auto wrote = runtime.api.write(fd, text.data(), text.size());
    const bool ok = wrote == static_cast<posix::ssize_t>(text.size());
    return (runtime.api.close(fd) == 0) && ok;
}

void seed_ramfs(RuntimeState& runtime) noexcept {
    fs::clear_mounts();
    (void)fs::add_mount(""sv, runtime.ramfs.mount_point());
    (void)runtime.api.mkdir("/bin");
    (void)runtime.api.mkdir("/apps");
    (void)runtime.api.mkdir("/tmp");
    (void)write_text_file(runtime, "/cat.txt", "hello from h747 posix lab\n");
    (void)write_text_file(runtime, "/stat.txt", "stat-probe");
    (void)write_text_file(runtime, "/write-out.txt", "seed\n");
    (void)write_text_file(runtime, "/append-out.txt", "base\n");
}

void register_samples(RuntimeState& runtime) noexcept {
    runtime.proc_service.enable_elf_exec(true);
    runtime.proc_service.enable_elf_hostcalls(true);
    for (const auto& sample : kSamples) {
        (void)runtime.proc_service.register_elf_mem(sample.name, sample.data, sample.size);
    }
}

void init_runtime(RuntimeState& runtime) noexcept {
    runtime.proc_service.init();
    runtime.proc_service.bind_fd_table(runtime.fd_table);
    runtime.proc_service.bind_file_service(runtime.file_service);
    runtime.proc_service.bind_process_runtime_hooks(
        &posix::user::process_enter<decltype(runtime.api)>,
        &posix::user::process_exit<decltype(runtime.api)>,
        &runtime.binding);
    runtime.api.bind_process(posix::ProcessId{0});
}

void print_prompt() noexcept {
    emit<"\r\nposix-lab> ">();
}

void print_banner() noexcept {
    emit<"posix_lab: resident monitor ready\n">();
    emit<"posix_lab: image_source=elfmem+file-backed(stub)\n">();
    emit<"posix_lab: cwd=/ PATH=/bin:/apps\n">();
}

void print_help() noexcept {
    emit<"Commands:\n">();
    emit<"  help                     - Show help\n">();
    emit<"  elf list                 - List builtin ELF samples\n">();
    emit<"  elf status               - Show runtime status\n">();
    emit<"  elf run <name> [args]    - Run builtin ELF sample\n">();
    emit<"  elf run-path <path> ...  - Run file-backed ELF path\n">();
    emit<"  elf smoke                - Run official Stage-2 smoke subset\n">();
}

void print_status(RuntimeState& runtime) noexcept {
    emit<"status: monitor=ready file_backed={} elf_exec={} hostcalls={}\n">(
        runtime.file_backed_ready,
        runtime.proc_service.elf_exec_enabled(),
        runtime.proc_service.elf_hostcalls_enabled());
    emit<"status: cwd=/ PATH=/bin:/apps samples={}\n">(static_cast<unsigned>(std::size(kSamples)));
}

void list_samples() noexcept {
    emit<"builtin ELF samples:\n">();
    for (const auto& sample : kSamples) {
        emit<"  {} ({} bytes)\n">(sample.name, static_cast<unsigned>(sample.size));
    }
}

std::optional<int> find_sample(std::string_view name) noexcept {
    for (util::usize i = 0; i < std::size(kSamples); ++i) {
        if (kSamples[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return std::nullopt;
}

void print_wait_result(const posix::WaitStatus& st) noexcept {
    emit<"wait: pid={} kind={} code={}\n">(
        st.pid.value,
        static_cast<unsigned>(st.kind),
        st.code);
}

bool run_spawn(RuntimeState& runtime,
               const char* path,
               std::span<const char* const> argv,
               posix::PathMode mode) noexcept {
    auto envp_storage = default_envp();
    posix::SpawnConfig cfg{};
    cfg.path = path;
    cfg.argv = argv;
    cfg.envp = std::span<const char* const>{envp_storage.data(), envp_storage.size() - 1};
    cfg.cwd = "/";
    cfg.path_mode = mode;

    auto result = runtime.proc_service.spawn(cfg);
    if (!result) {
        emit<"spawn: failed err={}\n">(static_cast<unsigned>(result.error()));
        return false;
    }
    emit<"spawn: pid={} path={}\n">(result.value().pid.value, std::string_view{path});
    auto waited = runtime.proc_service.waitpid(result.value().pid, 0);
    if (!waited) {
        emit<"wait: failed err={}\n">(static_cast<unsigned>(waited.error()));
        return false;
    }
    print_wait_result(waited.value());
    return true;
}

bool run_builtin(RuntimeState& runtime, std::string_view name, std::string_view arg_text) noexcept {
    std::array<char, 192> arg_blob{};
    auto parsed = parse_args<16>(arg_text, arg_blob);
    if (!parsed) {
        emit<"run: argv too large\n">();
        return false;
    }

    std::array<const char*, 17> argv{};
    std::array<char, 64> image_path{};
    const auto image_name = "elfmem:"sv;
    if (image_name.size() + name.size() + 1 > image_path.size()) {
        emit<"run: name too long\n">();
        return false;
    }
    std::memcpy(image_path.data(), image_name.data(), image_name.size());
    std::memcpy(image_path.data() + image_name.size(), name.data(), name.size());
    image_path[image_name.size() + name.size()] = '\0';

    util::usize argc = 0;
    argv[argc++] = image_path.data();
    for (util::usize i = 0; i < parsed.value().count; ++i) {
        argv[argc++] = parsed.value().items[i];
    }

    return run_spawn(runtime,
                     image_path.data(),
                     std::span<const char* const>{argv.data(), argc},
                     posix::PathMode::exact);
}

void run_path(RuntimeState& runtime, std::string_view path, std::string_view arg_text) noexcept {
    std::array<char, 192> arg_blob{};
    auto parsed = parse_args<16>(arg_text, arg_blob);
    if (!parsed) {
        emit<"run-path: argv too large\n">();
        return;
    }

    std::array<char, 160> path_buf{};
    if (path.size() + 1 > path_buf.size()) {
        emit<"run-path: path too long\n">();
        return;
    }
    std::memcpy(path_buf.data(), path.data(), path.size());
    path_buf[path.size()] = '\0';

    std::array<const char*, 17> argv{};
    util::usize argc = 0;
    argv[argc++] = path_buf.data();
    for (util::usize i = 0; i < parsed.value().count; ++i) {
        argv[argc++] = parsed.value().items[i];
    }

    if (!runtime.file_backed_ready) {
        emit<"run-path: backend not ready path={} err=not_supported\n">(path);
        return;
    }

    (void)run_spawn(runtime,
                    path_buf.data(),
                    std::span<const char* const>{argv.data(), argc},
                    posix::PathMode::search_path);
}

bool smoke_step(RuntimeState& runtime, std::string_view name, std::string_view args = {}) noexcept {
    emit<"[smoke] run {} {}\n">(name, args);
    return run_builtin(runtime, name, args);
}

void run_smoke(RuntimeState& runtime) noexcept {
    bool ok = true;
    ok = smoke_step(runtime, "hello") && ok;
    ok = smoke_step(runtime, "argv_dump", "a b") && ok;
    ok = smoke_step(runtime, "env_dump") && ok;
    ok = smoke_step(runtime, "stderr_demo") && ok;
    ok = smoke_step(runtime, "exit_code", "7") && ok;
    ok = smoke_step(runtime, "fd_probe", "/cat.txt") && ok;
    ok = smoke_step(runtime, "stat_probe", "/stat.txt") && ok;
    ok = smoke_step(runtime, "cat_file", "/cat.txt") && ok;
    ok = smoke_step(runtime, "write_file", "/tmp/write-smoke.txt payload") && ok;
    ok = smoke_step(runtime, "append_file", "/tmp/append-smoke.txt tail") && ok;
    emit<"[smoke] result={}\n">(ok ? "ok"sv : "failed"sv);
}

void handle_command(RuntimeState& runtime, std::string_view line) noexcept {
    const auto cmd = trim_left(line);
    if (cmd.empty()) {
        return;
    }
    if (cmd == "help"sv) {
        print_help();
        return;
    }
    if (cmd == "elf list"sv) {
        list_samples();
        return;
    }
    if (cmd == "elf status"sv) {
        print_status(runtime);
        return;
    }
    if (cmd == "elf smoke"sv) {
        run_smoke(runtime);
        return;
    }
    if (cmd.starts_with("elf run-path "sv)) {
        auto rest = cmd.substr(13);
        auto [path, args] = split_token(rest);
        if (path.empty()) {
            emit<"usage: elf run-path <path> [args...]\n">();
            return;
        }
        run_path(runtime, path, args);
        return;
    }
    if (cmd.starts_with("elf run "sv)) {
        auto rest = cmd.substr(8);
        auto [name, args] = split_token(rest);
        if (name.empty()) {
            emit<"usage: elf run <name> [args...]\n">();
            return;
        }
        if (!find_sample(name).has_value()) {
            emit<"run: no such builtin sample {}\n">(name);
            return;
        }
        (void)run_builtin(runtime, name, args);
        return;
    }
    emit<"unknown command: {}\n">(cmd);
}

} // namespace

void init() {
    auto& runtime = state();
    if (runtime.ready) {
        return;
    }
    attach_stdio(runtime);
    seed_ramfs(runtime);
    init_runtime(runtime);
    register_samples(runtime);
    runtime.ready = true;
    print_banner();
    print_help();
}

void loop_once() noexcept {
    auto& runtime = state();
    if (runtime.prompt_needed) {
        print_prompt();
        runtime.prompt_needed = false;
    }
    if (const auto line = runtime.line_source.poll_line()) {
        handle_command(runtime, *line);
        runtime.prompt_needed = true;
    }
}

} // namespace h747::apps::posix_lab
