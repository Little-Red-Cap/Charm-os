#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

import charm.core;
import shell_cmd;
import shell_core;
import shell_stream;
import service_ring_buffer;
import service_stream;
import service_trace;
import service_json;

struct TestStream {
    std::array<char, 64> buf{};
    std::size_t pos{0};

    service::StreamStatus read(std::span<util::u8>) noexcept {
        return service::StreamStatus{service::StreamResult::error};
    }

    service::StreamStatus write(std::span<const util::u8> in) noexcept {
        const auto n = in.size();
        if (pos + n > buf.size()) {
            return service::StreamStatus{service::StreamResult::error};
        }
        for (std::size_t i = 0; i < n; ++i) {
            buf[pos++] = static_cast<char>(in[i]);
        }
        return service::StreamStatus{service::StreamResult::ok};
    }

    service::StreamStatus flush() noexcept {
        return service::StreamStatus{service::StreamResult::ok};
    }
};

static util::usize console_write(void*, shell::Buffer buf) noexcept {
    return std::fwrite(buf.data, 1, buf.size, stdout);
}

static const std::array<shell::Command, 2> cmds{{
    {"echo",
     +[](shell::Console& con, int argc, std::span<std::string_view> argv) noexcept {
         for (int i = 1; i < argc; ++i) {
             (void)shell::write(con, argv[static_cast<std::size_t>(i)]);
             if (i + 1 < argc) (void)shell::write(con, " ");
         }
         (void)shell::write(con, "\n");
         return shell::ok();
     },
     "echo arguments"},
    {"help",
     +[](shell::Console& c, int, std::span<std::string_view>) noexcept {
         return shell::emit_help(c, std::span<const shell::Command>(cmds.data(), cmds.size()));
     },
     "show help"},
}};

int main() {
    service::RingBuffer<int, 4> rb;
    (void)rb.push(1);
    int v = 0;
    (void)rb.pop(v);

    service::TraceBuffer<util::u32, 4> trace;
    trace.push(service::TraceRecord<util::u32, 4>{1, 1, 42, 1});

    std::array<char, 64> out{};
    service::JsonWriter json{std::span<char>(out.data(), out.size())};
    (void)json.push('{');
    (void)json.write_kv("v", 42);
    (void)json.push('}');

    shell::Console con = shell::make_console(&console_write);
    (void)shell::write(con, "[shell] ok\n");

    (void)shell::run_line<8>(con, cmds, "echo hello shell");
    (void)shell::run_line<8>(con, cmds, "echo \"hello quoted world\"");
    (void)shell::run_line<8>(con, cmds, "echo \"hello \\\"escape\\\" world\"");
    (void)shell::run_line<8>(con, cmds, "help");

    TestStream ts{};
    shell::StreamConsole<TestStream> sc{&ts};
    auto scon = sc.make();
    (void)shell::write(scon, "[shell] stream console ok\n");
    std::printf("[shell] stream bytes=%zu\n", ts.pos);

    service::BufferedStream<TestStream, 16> buffered{ts};
    (void)service::write(buffered, "buffered stream");
    (void)buffered.flush();
    std::printf("[shell] buffered bytes=%zu\n", ts.pos);

    return 0;
}
